#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_netif_sta_list.h"
#include "WiFiWorker.h"
#include "WiFiManager.h"
#include "DataHub.h"
#include "NetworkUtils.h"
#include <Arduino.h>

#define WORKER_DELAY_MS 50UL
#define GET_STATUS_BACKOFF_MS 5000UL

namespace
{
    QueueHandle_t relayMailBox[RELAY_SIZE];
    QueueHandle_t staQueue;
    unsigned long lastApSync = 0;
    unsigned long lastFailTime[RELAY_SIZE] = {};

    enum class MacIpStatus
    {
        IP_MATCH,     /** <@brief IP and Mac corresponding */
        IP_CHANGED,   /** <@brief IP and Mac different Devices */
        NOT_CONNECTED /** <@brief Mac not found. */
    };

    void syncApClients()
    {
        wifi_sta_list_t staList = {};
        esp_netif_sta_list_t netifList = {};
        esp_wifi_ap_get_sta_list(&staList);
        esp_netif_get_sta_list(&staList, &netifList);

        DataHub::WifiRelay relays[RELAY_SIZE] = {};
        if (!DataHub::getWifiRelayArray(relays))
        {
            return;
        }
        for (size_t i = 0; i < RELAY_SIZE; i++)
        {
            if (!relays[i].inUse)
            {
                continue;
            }
            bool found = false;
            for (size_t j = 0; j < netifList.num; j++)
            {
                // check for mac
                char mac[MAC_LEN + 1] = {};
                NetworkUtils::macBytesToChar(mac, netifList.sta[j].mac, sizeof(mac));
                if (strcmp(mac, relays[i].mac) == 0)
                { // found
                    found = true;
                    // update IP if changed
                    char ip[IP_LEN + 1] = {};
                    NetworkUtils::ipToChar(ip, netifList.sta[j].ip.addr, sizeof(ip));
                    if (strcmp(ip, relays[i].ipV4Adress) != 0)
                    {
                        strncpy(relays[i].ipV4Adress, ip, sizeof(relays[i].ipV4Adress) - 1);
                        DataHub::setWifiRelayEntry(relays[i], i);
                    }
                    break;
                }
            }
            // MAC gone but relay still marked online
            if (!found && relays[i].online)
            {
                relays[i].online = false;
                DataHub::setWifiRelayEntry(relays[i], i);
            }
        }
    }

    MacIpStatus checkMacIpStatus(DataHub::WifiRelay &relay)
    {
        wifi_sta_list_t staList = {};
        esp_netif_sta_list_t netifList = {};
        esp_wifi_ap_get_sta_list(&staList);
        esp_netif_get_sta_list(&staList, &netifList);
        for (size_t i = 0; i < netifList.num; i++)
        {
            char mac[MAC_LEN + 1] = {};
            NetworkUtils::macBytesToChar(mac, netifList.sta[i].mac, sizeof(mac));
            if (strcmp(mac, relay.mac) == 0)
            {
                char ip[IP_LEN + 1] = {};
                NetworkUtils::ipToChar(ip, netifList.sta[i].ip.addr, sizeof(ip));
                if (strcmp(ip, relay.ipV4Adress) == 0)
                {
                    return MacIpStatus::IP_MATCH;
                }
                else
                {
                    strncpy(relay.ipV4Adress, ip, sizeof(relay.ipV4Adress) - 1);
                    return MacIpStatus::IP_CHANGED;
                }
            }
        }
        return MacIpStatus::NOT_CONNECTED;
    }

    void worker(void *param)
    {
        while (true)
        {
            // periodic AP client sync — IP mapping and offline detection
            unsigned long now = millis();
            DataHub::SystemData sys = {};
            DataHub::getSystemData(sys);
            if (now - lastApSync >= sys.relayStatusInterval_ms)
            {
                lastApSync = now;
                syncApClients();
            }
            // @note handle Switch Commands
            for (int i = 0; i < RELAY_SIZE; i++)
            {
                WiFiWorker::RelayCommand command = {};
                if (xQueueReceive(relayMailBox[i], &command, 0) == pdTRUE)
                {
                    DataHub::WifiRelay relay = {};
                    if (!DataHub::getWifiRelayEntry(relay, i))
                    {
                        continue;
                    }
                    if (!relay.online && command != WiFiWorker::RelayCommand::GET_STATUS)
                    {
                        continue;
                    }
                    switch (command)
                    {
                    case WiFiWorker::RelayCommand::GET_STATUS:
                    {
                        if (millis() - lastFailTime[i] < GET_STATUS_BACKOFF_MS)
                        {
                            break;
                        }
                        int result = WiFiManager::updateRelayStatus(relay.ipV4Adress);
                        if (result != 200)
                        {
                            lastFailTime[i] = millis();
                            if (result == -1)
                            {
                                relay.online = false;
                                DataHub::setWifiRelayEntry(relay, i);
                                Serial.printf("[%lus][WORKER] Relay %d: nicht erreichbar\n", millis() / 1000, i);
                            }
                            else
                            {
                                Serial.printf("[%lus][WORKER] Relay %d: GET_STATUS fehlgeschlagen (HTTP %d)\n", millis() / 1000, i, result);
                            }
                        }
                        break;
                    }
                    case WiFiWorker::RelayCommand::SWITCH_ON:
                        [[fallthrough]];
                    case WiFiWorker::RelayCommand::SWITCH_OFF:
                    {
                        MacIpStatus status = checkMacIpStatus(relay);
                        switch (status)
                        {
                        case MacIpStatus::NOT_CONNECTED:
                            relay.online = false;
                            DataHub::setWifiRelayEntry(relay, i);
                            break;
                        case MacIpStatus::IP_CHANGED:
                            DataHub::setWifiRelayEntry(relay, i);
                            [[fallthrough]];
                        case MacIpStatus::IP_MATCH:
                            if (WiFiManager::switchRelay(relay, command == WiFiWorker::RelayCommand::SWITCH_ON))
                            {
                                WiFiWorker::RelayCommand cmd = WiFiWorker::RelayCommand::GET_STATUS;
                                xQueueOverwrite(relayMailBox[i], &cmd);
                            }
                            break;
                        default:
                            break;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
            // @node handle STA Command
            WiFiWorker::StaMessage message = {};
            if (xQueueReceive(staQueue, &message, 0) == pdTRUE)
            {
                switch (message.command)
                {
                case WiFiWorker::StaCommand::NTP_SYNC:
                    /* sync ntp */
                    break;
                case WiFiWorker::StaCommand::EXTERNAL_COMMAND:
                    /* handle external command*/
                    break;
                default:
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(WORKER_DELAY_MS));
        }
    }
}

namespace WiFiWorker
{
    void start()
    {
        for (int i = 0; i < RELAY_SIZE; i++)
        {
            relayMailBox[i] = xQueueCreate(1, sizeof(RelayCommand));
        }
        staQueue = xQueueCreate(8, sizeof(StaMessage));
        xTaskCreatePinnedToCore(worker, "WiFiWorker", 8192, nullptr, 5, nullptr, 0);
    }
    void queueRelayCommand(int idx, const RelayCommand &command)
    {
        xQueueOverwrite(relayMailBox[idx], &command);
    }

    void queueStaCommand(const StaMessage &message)
    {
        xQueueSend(staQueue, &message, pdMS_TO_TICKS(100));
    }
}
