#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "NetworkWorker.h"
#include "WiFiManager.h"
#include "RelayRegistry.h"
#include "NetworkUtils.h"
#include "DigestAuth.h"
#include "SecureUtils.h"
#include <Arduino.h>
#include <WiFi.h>

#define WORKER_DELAY_MS 50UL
#define GET_STATUS_BACKOFF_MS 5000UL

namespace
{
    QueueHandle_t relayMailbox[REGISTRY_MAX];
    bool send(WiFiClient &client, NetworkWorker::RelayRequestType type, uint8_t relayId);

    void respond(WiFiClient &client, uint8_t relayId);

    void handleRelayRequest()
    {
        for (uint8_t relayId = 0; relayId < REGISTRY_MAX; relayId++)
        {
            if (relayMailbox[relayId] == nullptr)
                continue;
            NetworkWorker::RelayRequestType type = {};
            if (xQueueReceive(relayMailbox[relayId], &type, 0) != pdTRUE)
                continue;
            if (type == NetworkWorker::RelayRequestType::NONE)
                continue;

            WiFiClient client;
            if (send(client, type, relayId))
            {
                respond(client, relayId);
            }
        }
    }

    bool send(WiFiClient &client, NetworkWorker::RelayRequestType type, uint8_t relayId)
    {
        char request[RELAY_REQUEST_LEN] = {};
        switch (type)
        {
        case NetworkWorker::RelayRequestType::SWITCH_ON:
        case NetworkWorker::RelayRequestType::SWITCH_OFF:
            if (!RelayRegistry::getSwitchRequest(type == NetworkWorker::RelayRequestType::SWITCH_ON, request, sizeof(request), relayId))
                return false;
            break;
        case NetworkWorker::RelayRequestType::GET_STAT:
            if (!RelayRegistry::getStatusRequest(request, sizeof(request), relayId))
                return false;
            break;
        default:
            return false; // BIND/RESET kommen später dazu
        }

        uint8_t mac[MAC_LEN];
        if (!RelayRegistry::getMac(relayId, mac))
            return false;

        char ip[IP_LEN];
        if (!WiFiManager::resolveIp(mac, ip, sizeof(ip)))
            return false;

        char password[RELAY_PASSWORD_LEN];
        if (!RelayRegistry::getPassword(relayId, password, sizeof(password)))
            return false;

        int code = DigestAuth::digestAuthRequest(ip, request, "admin", password, client);
        SecureUtils::ensureZero(password, sizeof(password));
        return code == 200;
    }
    // TODO fill me
    void handleSensorRequest() {}
    // TODO fill or replace me
    void handleSTARequest() {}

    void handle(void *pvParameters)
    {
        for (;;)
        {
            handleRelayRequest();
            handleSensorRequest();
            handleSTARequest();
        }
    }

}
namespace NetworkWorker
{
    bool pushRelayRequest(RelayRequestType type, uint8_t relayId)
    {
        if (relayMailbox[relayId] == nullptr)
            return false;
        return xQueueOverwrite(relayMailbox[relayId], &type);
    }
    bool createRelayMailbox(uint8_t relayId)
    {
        relayMailbox[relayId] = xQueueCreate(1, sizeof(RelayRequestType));
        if (relayMailbox[relayId] == nullptr)
            return false;
        RelayRequestType type = RelayRequestType::NONE;
        xQueueOverwrite(relayMailbox[relayId], &type);
        return true;
    }
    void destroyRelayMailbox(uint8_t relayId)
    {
        if (relayMailbox[relayId] != nullptr)
        {
            vQueueDelete(relayMailbox[relayId]);
            relayMailbox[relayId] = nullptr;
        }
    }

    /**
     * @brief starts NetworkWorker, called in initWifi().
     */
    void start()
    {
        xTaskCreatePinnedToCore(handle, "NetworkWorker", 8192, nullptr, 2, nullptr, 0);
    }
}
