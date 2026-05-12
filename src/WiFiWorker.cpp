#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_netif_sta_list.h"
#include <atomic>
#include "WiFiWorker.h"
#include "WiFiManager.h"
#include "DataHub.h"
#include "NetworkUtils.h"
#include <Arduino.h>

namespace
{

    QueueHandle_t relayMailBox[RELAY_SIZE];
    QueueHandle_t staQueue;
    std::atomic<bool> apEventPending{false};

    enum class MacIpStatus
    {
        IP_MATCH,     /** <@brief IP and Mac corresponding */
        IP_CHANGED,   /** <@brief IP and Mac different Devices */
        NOT_CONNECTED /** <@brief Mac not found. */
    };

    void syncApClients()
    {
        Serial.println("AP Clients sync");
        wifi_sta_list_t staList = {};
        esp_netif_sta_list_t netifList = {};
        esp_wifi_ap_get_sta_list(&staList);
        esp_netif_get_sta_list(&staList, &netifList);

        DataHub::WifiRelay relays[RELAY_SIZE] = {};
        if (DataHub::getWifiRelayArray(relays) != true)
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
            bool changed = false;
            for (size_t j = 0; j < netifList.num; j++)
            {
                // check for mac
                char mac[MAC_LEN + 1] = {};
                NetworkUtils::macBytesToChar(mac, netifList.sta[j].mac, sizeof(mac));
                if (strcmp(mac, relays[i].mac) == 0)
                { // found
                    found = true;
                    // check for ip, if found
                    char ip[IP_LEN + 1] = {};
                    NetworkUtils::ipToChar(ip, netifList.sta[j].ip.addr, sizeof(ip));
                    if (strcmp(ip, relays[i].ipV4Adress) != 0)
                    {
                        strncpy(relays[i].ipV4Adress, ip, sizeof(relays[i].ipV4Adress) - 1);
                        changed = true;
                    }
                }
            }
            if (relays[i].online != found)
            {
                relays[i].online = found;
                changed = true;
            }
            if (changed)
            {
                relays[i].isDirty = true;
                DataHub::setWifiRelayEntry(relays[i], i);
                WiFiWorker::RelayCommand cmd = WiFiWorker::RelayCommand::GET_STATUS;
                xQueueOverwrite(relayMailBox[i], &cmd);
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
            // @node handle AP Event
            if (apEventPending.exchange(false))
            {
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
                    if (!relay.online)
                    {
                        continue;
                    }
                    switch (command)
                    {
                    case WiFiWorker::RelayCommand::GET_STATUS:
                    {
                        Serial.printf("[WORKER] Relay %d: GET_STATUS\n", i);
                        int result = WiFiManager::updateRelayStatus(relay.ipV4Adress);
                        if (result == -1)
                        {
                            Serial.printf("[WORKER] Relay %d: GET_STATUS fehlgeschlagen, AP-Sync ausgelöst\n", i);
                            apEventPending = true;
                        }
                        else if (result != 200)
                        {
                            Serial.printf("[WORKER] Relay %d: GET_STATUS fehlgeschlagen (HTTP %d)\n", i, result);
                        }
                        break;
                    }
                    case WiFiWorker::RelayCommand::SWITCH_ON:[[fallthrough]];
                    case WiFiWorker::RelayCommand::SWITCH_OFF:
                    {
                        MacIpStatus status = checkMacIpStatus(relay);
                        switch (status)
                        {
                        case MacIpStatus::NOT_CONNECTED:
                            WiFiWorker::notifyApEvent();
                            break;
                        case MacIpStatus::IP_CHANGED:
                            DataHub::setWifiRelayEntry(relay, i);[[fallthrough]];
                        case MacIpStatus::IP_MATCH:
                            if(!WiFiManager::switchRelay(relay, command == WiFiWorker::RelayCommand::SWITCH_ON)){
                                apEventPending=true;
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
            vTaskDelay(pdMS_TO_TICKS(10));
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
    void notifyApEvent()
    {
        apEventPending = true;
    }
}
