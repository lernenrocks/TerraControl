#include <WiFi.h>
#include <stdint.h>
#include <Arduino.h>
#include "WiFiManager.h"
#include "wifi_config.h"
#include "DataHub.h"
#include "DigestAuth.h"
#include "HttpClient.h"
#include "JsonParser.h"
#include "WiFiWorker.h"

#define WIFI_CONNECTION_TIMEOUT 60000UL /**< @brief 60 Sekunden, um eine WiFi Verbindung aufzubauen */

#define URI_LEN 64

const char uriShellyGetStatus[] = "rpc/Shelly.GetStatus";
const char shellyUser[] = RELAY_USER;
const char shellyPass[] = TERRA_CONTROL_PW;

namespace
{
    /**
     * @brief handler when client has connected to AP
     */
    void onApClientConnected(WiFiEvent_t, WiFiEventInfo_t)
    {
        WiFiWorker::notifyApEvent();
    }
    /**
     * @brief handler when client has disconnected to AP
     */
    void onApClientDisconnected(WiFiEvent_t, WiFiEventInfo_t)
    {
        WiFiWorker::notifyApEvent();
    }

}

namespace WiFiManager
{
    bool initWiFi()
    {
        Serial.println("Initialisiere WiFi Modul");
        WiFi.disconnect(true);
        delay(1000);
        DataHub::SystemData systemData;
        DataHub::getSystemData(systemData);
        WiFi.softAP(systemData.deviceName, AP_WIFI_PW); // set credentials for Access Point
        WiFi.onEvent(onApClientConnected, ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
        WiFi.onEvent(onApClientDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
        WiFi.mode(WIFI_AP_STA);         // starts Access Point and Station Point Mode
        WiFi.setAutoConnect(true);      // try reconnect if connection is lost
        WiFi.begin(WIFI_SSID, WIFI_PW); // start Wifi with STA Credentials
        long lastTry = millis();
        Serial.print("Verbinde WiFi ..");
        while (WiFi.status() != WL_CONNECTED)
        {
            if (millis() - lastTry < WIFI_CONNECTION_TIMEOUT)
            {
                Serial.print('.');
                delay(500);
            }
            else
            {
                DataHub::setWifiStatus({});
                return false;
            }
        }
        WiFiWorker::start();
        return true;
    }

    void updateWifiStatus()
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            return;
        }
        DataHub::WiFiStatus status;
        //? Umschreiben auf SSID aus preferences
        strncpy(status.ssid, WIFI_SSID, sizeof(status.ssid) - 1);
        IPAddress ip = WiFi.localIP();
        snprintf(status.ipV4Adress, sizeof(status.ipV4Adress),
                 "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        status.rssi = WiFi.RSSI();
        DataHub::setWifiStatus(status);
    }

    bool switchRelay(DataHub::WifiRelay relay, bool switchOn)
    {
        char uri[URI_LEN] = {};
        if (!relay.inUse)
        {
            Serial.println("[WARN] Schaltversuch eines ungültigen WiFiRelays");
            return false;
        }
        if (switchOn)
        {
            snprintf(uri, sizeof(uri), "rpc/Switch.Set?id=%d&on=true&toggle_after=%ld",
                     relay.id, relay.switchDuration / 1000);
        }
        else
        {
            snprintf(uri, sizeof(uri), "rpc/Switch.Set?id=%d&on=false",
                     relay.id);
        }
        Serial.println("Schalte Shelly:");
        DataHub::wifiRelayToSerial(relay); // Debug
        WiFiClient wifiClient;
        int code = DigestAuth::digestAuthRequest(relay.ipV4Adress, uri, shellyUser, shellyPass, wifiClient);
        // Serial.printf("Rückgabewert von digestAuthGET: %d\n", code);
        wifiClient.stop();
        if (code != 200)
        {
            Serial.printf("[ERROR] Switch.Set fehlgeschlagen, HTTP %d\n", code);
            return false;
        }
        return updateRelayStatus(relay.ipV4Adress) == 200;
    }

    int updateRelayStatus(char *ip)
    {
        WiFiClient wifiClient;
        int code = DigestAuth::digestAuthRequest(ip, uriShellyGetStatus, shellyUser, shellyPass, wifiClient);
        if (code != 200)
        {
            wifiClient.stop();
            Serial.printf("[ERROR] GET_STATUS HTTP %d für %s\n", code, ip);
            return code;
        }
        unsigned long bodyWait = millis();
        while (!wifiClient.available() && (millis() - bodyWait) < TCP_MAX_TIME)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        wifiClient.setTimeout(TCP_MAX_TIME);
        bool success = JsonParser::parseJson(wifiClient, JsonParser::TargetType::WIFI_RELAY_STATUS) == JsonParser::ParseResult::OK;
        if (!success)
        {
            Serial.println("[ERROR] Body Parsing Fehler in WiFiManager::updateRelayStatus");
        }
        wifiClient.stop();
        return success ? 200 : 0;
    }
}