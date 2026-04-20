#include "DataHub.h"
#include <Arduino.h>
#define DATAHUB_MUTEX_TIMEOUT 100

DataHub::DataHub dataHub;
static SemaphoreHandle_t dataHubMutex = nullptr;

namespace DataHub
{
    bool initDataHub()
    {

        if (dataHubMutex == nullptr)
        {
            dataHubMutex = xSemaphoreCreateMutex();
        }
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        strncpy(dataHub.systemData.deviceName, "DevControl001", sizeof(dataHub.systemData.deviceName));
        Serial.println(dataHub.systemData.deviceName);
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    bool setWifiStatus(const WiFiStatus &in)
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        dataHub.wifiStatus = in;
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    bool getWiFiStatus(WiFiStatus &out)
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        out = dataHub.wifiStatus;
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    bool setSystemData(const SystemData &in)
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        dataHub.systemData = in;
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    bool getSystemData(SystemData &out)
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        out = dataHub.systemData;
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    bool setWifiRelayEntry(const WifiRelay &in, const int index)
    {
        if (index >= RELAY_SIZE)
        {
            return false;
        }
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        dataHub.wifiRelay[index] = in;
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    bool getWifiRelayEntryByMac(WifiRelay &out, int &idx, const char *mac, const int id)
    {
        if (mac == nullptr)
        {
            return false;
        }
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        bool found = false;
        for (int i = 0; i < RELAY_SIZE; i++)
        {
            if (strcmp(mac, dataHub.wifiRelay[i].mac) == 0 && dataHub.wifiRelay[i].id == id)
            {
                out = dataHub.wifiRelay[i];
                idx = i;
                found = true;
                break;
            }
        }
        xSemaphoreGive(dataHubMutex);
        return found;
    }

    bool getWifiRelayArray(WifiRelay (&out)[RELAY_SIZE])
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        for (size_t i = 0; i < RELAY_SIZE; i++)
        {
            out[i] = dataHub.wifiRelay[i];
        }
        xSemaphoreGive(dataHubMutex);
        return true;
    }
    /**
     * @brief Setzt alle WifiRelay-Einträge mit passender IP auf online=false.
     *        IP-Adresse bleibt erhalten, damit der Eintrag bei erneutem Kontakt
     *        ohne mDNS-Lookup wiederverwendet werden kann.
     * @param ip IPv4-Adresse als null-terminierter String.
     */
    void setWifiRelayOfflineByIP(const char *ip)
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return;
        }
        for (WifiRelay &entry : dataHub.wifiRelay)
        {
            if (strcmp(entry.ipV4Adress, ip) == 0)
            {
                entry.online = false;
            }
        }
        xSemaphoreGive(dataHubMutex);
    }

    int registerSensor(SensorData &in)
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return -1;
        }
        for (int i = 0; i < SENSOR_DATA_SIZE; i++)
        {
            if (!dataHub.sensorData[i].inUse)
            {
                dataHub.sensorData[i] = in;
                xSemaphoreGive(dataHubMutex);
                return i;
            }
        }
        Serial.println("Kein freier Platz in Sensor Array");
        xSemaphoreGive(dataHubMutex);
        return -2;
    }

    bool deleteSensor(int id)
    {
        if (id >= SENSOR_DATA_SIZE)
        {
            return false;
        }
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        dataHub.sensorData[id] = {};
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    bool setSensorData(SensorData &in, int id)
    {
        if (id >= SENSOR_DATA_SIZE)
        {
            return false;
        }
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        dataHub.sensorData[id] = in;
        xSemaphoreGive(dataHubMutex);
        return true;
    }
    bool getSensorData(SensorData &out, int idx)
    {
        if (idx >= SENSOR_DATA_SIZE)
        {
            return false;
        }
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        out = dataHub.sensorData[idx];
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    bool getSensorDataArray(SensorData (&out)[SENSOR_DATA_SIZE])
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            return false;
        }
        for (size_t i = 0; i < SENSOR_DATA_SIZE; i++)
        {
            out[i] = dataHub.sensorData[i];
        }
        xSemaphoreGive(dataHubMutex);
        return true;
    }

    void dataHubToSerial()
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            Serial.println("dataHubToSerial: mutex take failed");
            return;
        }

        Serial.println("--- DataHub Dump ---");

        Serial.print("System.deviceName: ");
        Serial.println(dataHub.systemData.deviceName);

        Serial.print("WiFi.ssid: ");
        Serial.println(dataHub.wifiStatus.ssid);
        Serial.print("WiFi.rssi: ");
        Serial.println(dataHub.wifiStatus.rssi);
        Serial.print("WiFi.ipV4Adress: ");
        Serial.println(dataHub.wifiStatus.ipV4Adress);
        Serial.print("WiFi.mac: ");
        Serial.println(dataHub.wifiStatus.mac);
        Serial.print("WiFi.dirty: ");
        Serial.println(dataHub.wifiStatus.dirty ? "true" : "false");

        for (int i = 0; i < RELAY_SIZE; i++)
        {
            auto &r = dataHub.wifiRelay[i];
            Serial.printf("\nRelay[%d] id=%d inUse=%s online=%s\n", i, r.id, r.inUse ? "true" : "false", r.online ? "true" : "false");
            Serial.printf("  name=%s mac=%s ip=%s\n", r.name, r.mac, r.ipV4Adress);
            Serial.printf("  output=%s voltage=%.1fV current=%.3fA power=%.1fW aenergy=%.3fkWh lastUpdate=%lu\n",
                          r.output ? "true" : "false", r.voltage, r.current, r.power, r.aenergy, r.lastUpdate);
        }

        Serial.println("--- end DataHub Dump ---");

        xSemaphoreGive(dataHubMutex);
    }

    void dataHubStatusToSerial()
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            Serial.println("dataHubStatusToSerial: mutex take failed");
            return;
        }
        unsigned long now = millis();
        Serial.printf("\n\033[1m=== TerraControl [t=%lu.%lus] ==========================\033[0m\n",
                      now / 1000, (now % 1000) / 100);
        Serial.printf("WiFi: %s  IP: %s  RSSI: %d dBm\n",
                      dataHub.wifiStatus.ssid,
                      dataHub.wifiStatus.ipV4Adress,
                      dataHub.wifiStatus.rssi);
        Serial.println("--------------------------------------------------------");
        for (int i = 0; i < RELAY_SIZE; i++)
        {
            auto &r = dataHub.wifiRelay[i];
            if (!r.inUse)
                continue;
            unsigned long age = (r.lastUpdate > 0) ? (now - r.lastUpdate) / 1000 : 0;
            const char *statusColor = r.online ? "\033[32m" : "\033[31m"; // grün / rot
            const char *outputColor = r.output ? "\033[33m" : "\033[0m";  // gelb wenn ON
            Serial.printf("[%d] %-12s  %s%-7s\033[0m  %s%-3s\033[0m  %6.3fA  %6.1fW %ddBm ",
                          i,
                          r.mac[0] ? r.mac : "(kein mac)",
                          statusColor, r.online ? "ONLINE" : "OFFLINE",
                          outputColor, r.output ? "ON" : "OFF",
                          r.current, r.power,
                          r.rssi);
            if (r.lastUpdate == 0)
                Serial.println("\033[33m(nie gesehen)\033[0m");
            else
                Serial.printf("(vor %lus)\n", age);
        }
        Serial.println("========================================================");
        xSemaphoreGive(dataHubMutex);
    }

    void wifiRelayToSerial(const WifiRelay &relay)
    {
        Serial.printf("wifiRelay: id=%d mac=%s ip=%s name=%s inUse=%s online=%s\n",
                      relay.id,
                      relay.mac,
                      relay.ipV4Adress,
                      relay.name,
                      relay.inUse ? "true" : "false",
                      relay.online ? "true" : "false");
        Serial.printf("  output=%s voltage=%.1fV current=%.3fA power=%.1fW aenergy=%.3fkWh lastUpdate=%lu\n",
                      relay.output ? "true" : "false",
                      relay.voltage,
                      relay.current,
                      relay.power,
                      relay.aenergy,
                      relay.lastUpdate);
    }
    void sensorDataToSerial()
    {
        if (xSemaphoreTake(dataHubMutex, pdMS_TO_TICKS(DATAHUB_MUTEX_TIMEOUT)) != pdTRUE)
        {
            Serial.println("sensorDataToSerial: mutex take failed");
            return;
        }
        unsigned long now = millis();
        Serial.println("--- SensorData ---");
        for (int i = 0; i < SENSOR_DATA_SIZE; i++)
        {
            auto &s = dataHub.sensorData[i];
            // if (!s.inUse)
            //     continue;
            unsigned long age = (s.lastUpdate > 0) ? (now - s.lastUpdate) / 1000 : 0;
            const char *statusColor = s.online ? "\033[32m" : "\033[31m";
            const char *typeName = "UNKNOWN";
            switch (s.type)
            {
            case SensorType::DHT22_TEMPERATURE: typeName = "DHT22_TEMP"; break;
            case SensorType::DHT22_HUMIDITY:    typeName = "DHT22_HUM";  break;
            case SensorType::SOIL_MOISTURE:     typeName = "SOIL";       break;
            case SensorType::REMOTE_SENSOR:     typeName = "REMOTE";     break;
            default: break;
            }
            Serial.printf("[%d] %-16s  %s%-7s\033[0m  %8.2f %-4s  type=%-10s idx=%d  ",
                          i, s.name, statusColor, s.online ? "ONLINE" : "OFFLINE",
                          s.value, s.unit, typeName, s.sensorIndex);
            if (s.lastUpdate == 0)
                Serial.println("\033[33m(nie gesehen)\033[0m");
            else
                Serial.printf("(vor %lus)\n", age);
        }
        Serial.println("--- end SensorData ---");
        xSemaphoreGive(dataHubMutex);
    }
}