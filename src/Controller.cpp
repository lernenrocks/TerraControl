#include "freertos/FreeRTOS.h"
#include "Controller.h"
#include "DataHub.h"
#include "WiFiWorker.h"
#include "WiFiWorkerTypes.h"
#include "SensorManager.h"
#include <Arduino.h>

#define CONTROLLER_UPDATE_INTERVAL 200UL
#define SWITCH_RETRY_INTERVAL_MS 2000UL

namespace
{
    DataHub::DataHub dataHub = {};
    unsigned long lastSwitchOn[RELAY_SIZE] = {};
    unsigned long lastStatusRequest[RELAY_SIZE] = {};
    /**
     * @brief monitors the status of all wifiRelays
     * @param now current timestamp in millis
     *
     */
    void monitorRelayStatus(unsigned long now)
    {
        for (size_t i = 0; i < RELAY_SIZE; i++)
        {
            DataHub::WifiRelay &relay = dataHub.wifiRelay[i];
            if (!relay.online)
            {
                continue;
            }
            if (now - lastStatusRequest[i] > dataHub.systemData.relayStatusInterval_ms)
            {
                Serial.printf("[CTRL] Relay[%d] %s → GET_STATUS\n", i, relay.name);
                lastStatusRequest[i] = now;
                WiFiWorker::queueRelayCommand(i, WiFiWorker::RelayCommand::GET_STATUS);
            }
        }
    }
    /**
     * @brief switches a relay into the desired Mode
     * @param now the current timestap in millis
     */
    void enforceRelayState(unsigned long now)
    {
        for (size_t i = 0; i < RELAY_SIZE; i++)
        {
            DataHub::WifiRelay &relay = dataHub.wifiRelay[i];
            if (!relay.online)
            {
                continue;
            }
            switch (relay.relayMode)
            {
            case RelayTypes::RelayMode::FORCED_ON:
                if (now - lastSwitchOn[i] > relay.switchDuration / 3 ||
                    (!relay.output && now - lastSwitchOn[i] > SWITCH_RETRY_INTERVAL_MS))
                {
                    Serial.printf("[CTRL] Relay[%d] %s → SWITCH_ON\n", i, relay.name);
                    lastSwitchOn[i] = now;
                    WiFiWorker::queueRelayCommand(i, WiFiWorker::RelayCommand::SWITCH_ON);
                }
                break;
            case RelayTypes::RelayMode::FORCED_OFF:
                if (relay.output)
                {
                    Serial.printf("[CTRL] Relay[%d] %s → SWITCH_OFF\n", i, relay.name);
                    WiFiWorker::queueRelayCommand(i, WiFiWorker::RelayCommand::SWITCH_OFF);
                }
                break;
            case RelayTypes::RelayMode::AUTO:
                // implement rule node evaluation
                break;
            default:
                break;
            }
        }
    }
}

namespace Controller
{
    void update(unsigned long now)
    {
        static unsigned long last = 0;
        if (now - last < CONTROLLER_UPDATE_INTERVAL)
        {
            return;
        }
        if (!DataHub::getDataHub(dataHub))
        {
            Serial.println("[ERROR] DataHub nicht lesbar");
            return;
        }

        monitorRelayStatus(now);
        enforceRelayState(now);
        SensorManager::update(now);
        last = now;
    }
}
