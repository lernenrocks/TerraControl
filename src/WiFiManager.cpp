#include <WiFi.h>
#include <stdint.h>
#include <Arduino.h>
#include "WiFiManager.h"
#include "wifi_config.h"
#include "DigestAuth.h"
#include "HttpClient.h"
#include "System.h"
#include "Logger.h"
#include "SecureUtils.h"
#include "NetworkUtils.h"
#include "esp_wifi.h"
#include "esp_netif_sta_list.h"
constexpr unsigned long WIFI_RETRY_INTERVAL = 10000; /**< @brief 10 second try to connect to STA */

#define URI_LEN 64

namespace WiFiManager
{
    bool initWiFi()
    {
        Serial.println("Initialisiere WiFi Modul");
        WiFi.disconnect(true);
        delay(1000);
        char deviceName[SYSTEM_DEVICE_NAME_LEN] = {};
        char devicePassword[SYSTEM_DEVICE_PASSWORD_LEN] = {};
        if (!System::getDeviceName(deviceName, sizeof(deviceName)) || !System::getDevicePassword(devicePassword, sizeof(devicePassword)))
        {
            return false;
        }
        WiFi.softAP(deviceName, devicePassword,1,0,10);
        SecureUtils::ensureZero(devicePassword, sizeof(devicePassword));
        // set credentials for Access Point
        WiFi.mode(WIFI_AP_STA);       // starts Access Point and Station Point Mode
        WiFi.setAutoReconnect(false); // prevent reconnect polling
        return true;
    }
    void syncStaConnection()
    {
        static unsigned long lastAttempt = 0 - WIFI_RETRY_INTERVAL;
        if (System::isStaMode() && WiFi.status() == WL_CONNECTED)
        {
            return;
        }
        else if (System::isStaMode() && (WiFi.status() != WL_CONNECTED))
        {
            if (millis() - lastAttempt >= WIFI_RETRY_INTERVAL)
            {
                char ssid[SYSTEM_STA_SSID_LEN] = {};
                char pw[SYSTEM_STA_PASSWORD_LEN] = {};
                System::getStaSsid(ssid, sizeof(ssid));
                System::getStaPassword(pw, sizeof(pw));
                WiFi.begin(ssid, pw);
                SecureUtils::ensureZero(ssid, sizeof(ssid));
                SecureUtils::ensureZero(pw, sizeof(pw));
            }
        }
        else if (!System::isStaMode() && (WiFi.status() == WL_CONNECTED))
        {
            WiFi.disconnect();
        }
    }

    bool getRssi(int &rssi)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            return false;
        }
        rssi = WiFi.RSSI();
        return true;
    }

    bool getLocalIp(char *buffer, size_t len)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            return false;
        }
        IPAddress ip = WiFi.localIP();
        return NetworkUtils::ipToChar(buffer, (uint32_t)ip, len);
    }

    bool resolveIp(const uint8_t *mac, char *buffer, size_t len)
    {
        wifi_sta_list_t staList = {};
        esp_netif_sta_list_t netifList = {};
        esp_wifi_ap_get_sta_list(&staList);
        esp_netif_get_sta_list(&staList, &netifList);
        for (size_t i = 0; i < netifList.num; i++)
        {
            if (memcmp(mac, netifList.sta[i].mac, MAC_LEN) == 0)
            {
                return NetworkUtils::ipToChar(buffer, netifList.sta[i].ip.addr, len);
            }
        }
        return false;
    }
}