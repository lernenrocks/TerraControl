// WiFiManager - Neuaufbau
// Minimale Struktur für ESP32 ohne angeschlossene Hardware

#include <WiFi.h>
#include <Arduino.h>
#include <ESPmDNS.h>
#include "WiFiManager.h"
#include "wifi_config.h"
#include "DataHub.h"
#include "DigestAuth.h"
#include "HttpUtils.h"
#include "JsonParser.h"

#define MUTEX_WIFI_TIMEOUT 5000         /**< @brief 5s timeout, um Mutex zu bekommen*/
#define WIFI_CONNECTION_TIMEOUT 60000UL /**< @brief 60 Sekunden, um eine WiFi Verbindung aufzubauen */
#define WIFI_RELAY_CONNECTION_TIMEOUT 300000UL
#define WIFI_RELAY_CONNECTION_SHORT_TIMEOUT 60000UL

#define URI_LEN 64

const char uriShellyGetStatus[] = "rpc/Shelly.GetStatus";
const char shellyUser[] = RELAY_USER;
const char shellyPass[] = TERRA_CONTROL_PW;

static SemaphoreHandle_t wifiMutex = nullptr;
namespace
{
    /**
     * @brief Führt einen Shelly-Statusabruf per Digest-Auth durch und parsed den Response in den DataHub.
     *        Setzt bei Fehler alle Entries mit dieser IP auf offline.
     *        Kein Mutex-Management — Caller ist verantwortlich.
     * @param ip IPv4-Adresse des Shelly als null-terminierter String.
     * @return true bei erfolgreichem Abruf und Parsing, false sonst.
     */
    bool _updateRelayStatusInternal(char *ip)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("WiFi in updateRelayStatus(int index) nicht verbunden");
            return false;
        }
        WiFiClient wifiClient;
        int code = DigestAuth::digestAuthRequest(ip, uriShellyGetStatus, shellyUser, shellyPass, wifiClient);
        Serial.printf("Code nach digestAuth: %d\n", code);
        unsigned long start = millis(); // Startzeit zurücksetzen
        bool success = false;
        while (wifiClient.connected() && (millis() - start < TCP_MAX_TIME))
        {
            if (!wifiClient.available())
            {
                delay(1); //? Umschreiben auf nicht blockierend
                continue;
            }
            HttpUtils::skipHeader(wifiClient);
            JsonParser::ParseResult result = JsonParser::parseJson(wifiClient, JsonParser::TargetType::WIFI_RELAY_STATUS);
            if (result == JsonParser::ParseResult::OK)
            {
                Serial.println("Body erhalten");
                success = true;
            }
            break;
        }
        wifiClient.stop();
        if (!success)
        {
            DataHub::setOfflineByIP(ip);
        }
        return success;
    }
}

namespace WiFiManager
{
    bool initWiFi()
    {
        if (wifiMutex == nullptr)
        {
            wifiMutex = xSemaphoreCreateMutex();
        }
        Serial.println("Initialisiere WiFi Modul");
        WiFi.disconnect(true); // trenne eventuelle Altlasten eines reboots
        delay(1000);
        WiFi.mode(WIFI_STA);            // WiFi Modus Station (Verbinde mit einen AccesPoint z.B. Router)
        WiFi.setAutoConnect(true);      // versuche ein Reconnect, wenn Verbindung unterbrochen
        WiFi.begin(WIFI_SSID, WIFI_PW); // wifi Start
        long lastTry = millis();
        ;
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
        DataHub::SystemData systemData = {};
        if (!DataHub::getSystemData(systemData))
        {
            snprintf(systemData.deviceName, sizeof(systemData.deviceName) - 1, "ESP32");
        }
        if (!MDNS.begin(systemData.deviceName)) // Starte MDNS
        {
            Serial.println("MDNS konnte nicht initialisiert werden");
            return false;
        }
        MDNS.addService("http", "tcp", 80);
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

    void switchRelay(const char *mac, int id, bool on, long duration)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("Kein WLAN verfügbar");
            return;
        }
        char uri[URI_LEN] = {};
        snprintf(uri, sizeof(uri), "relay/%d?timer=%ld&turn=%s", id, duration, on ? "on" : "off");
        DataHub::WifiRelay wifiRelay = {};
        int index = 0;
        if (!DataHub::getWifiRelayEntryByMac(wifiRelay, index, mac, id))
        {
            Serial.println("WifiRelay Mac ID Kombination ungültig");
            return;
        }
        Serial.println("Schalte Shelly:");
        DataHub::wifiRelayToSerial(wifiRelay);
        if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(MUTEX_WIFI_TIMEOUT)) != pdTRUE)
        {
            Serial.println("Mutex in switchRelay() konnte nicht genommen werden");
            return;
        }
        unsigned long start = millis();
        WiFiClient wifiClient;
        int code = DigestAuth::digestAuthRequest(wifiRelay.ipV4Adress, uri, shellyUser, shellyPass, wifiClient);
        Serial.printf("Rückgabewert von digestAuthGET: %d", code);
        _updateRelayStatusInternal(wifiRelay.ipV4Adress);
        wifiClient.stop();
        xSemaphoreGive(wifiMutex);
    }

    void findStoredRelays()
    {
        int services = MDNS.queryService("http", "tcp");
        Serial.printf("Gefundene Services: %d\n", services);
        char lastIp[IP_LEN] = {}; // Puffer für die letzte gefundene IP. Reduziert das mehrfache ansprechen der gleichen IP
        for (int i = 0; i < services; i++)
        {
            char ip[IP_LEN] = {};
            snprintf(ip, IP_LEN, "%u.%u.%u.%u",
                     MDNS.IP(i)[0], MDNS.IP(i)[1], MDNS.IP(i)[2], MDNS.IP(i)[3]);
            // Lese IP jedes Eintrags aus MDNS Querry
            if (strcmp(lastIp, ip) == 0)
            {
                continue;
            }
            strcpy(lastIp, ip);
            Serial.printf("%s\n", ip);
            updateRelayStatus(ip);
        }
    }

    bool updateRelayStatus(char *ip)
    {
        bool success = false;
        if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(MUTEX_WIFI_TIMEOUT)) != pdTRUE)
        {
            Serial.println("Mutex in updateRelayStatus konnte nicht genommen werden");
            return success;
        }
        success = _updateRelayStatusInternal(ip);
        xSemaphoreGive(wifiMutex);
        return success;
    }

    void checkRelayTimeouts()
    {
        DataHub::WifiRelay relays[RELAY_SIZE] = {};
        DataHub::getWifiRelayArray(relays);
        for (DataHub::WifiRelay &entry : relays)
        {
            if (!entry.inUse)
            {
                continue;
            }
            if (entry.lastUpdate == 0) // Seit Systemstart nie gesehen
            {
                if (millis() < WIFI_RELAY_CONNECTION_SHORT_TIMEOUT) // 1 min Timeout
                {
                    continue;
                }
            }
            else
            {
                if (millis() - entry.lastUpdate < WIFI_RELAY_CONNECTION_TIMEOUT) // schon mal gesehen 5min Timeout
                {
                    continue;
                }
            }
            findStoredRelays();
            return;
        }
    }

}