#pragma once

#include "SensorTypes.h"
#include "RelayTypes.h"

#define IP_LEN 16
#define MAC_LEN 13
#define NAME_LEN 32
#define RELAY_SIZE 8
#define UNIT_LEN 8
#define SENSOR_DATA_SIZE 16

namespace DataHub
{
    struct SystemData
    {
        char deviceName[NAME_LEN] = "TerraControl";
        unsigned long relayStatusInterval_ms = 5000;
    };

    struct WiFiStatus
    {
        char ssid[33] = {};           /**< @brief SSID, max 32 chars */
        int rssi = 0;                 /**< @brief signal strength in dBm */
        char ipV4Adress[IP_LEN] = {}; /**< @brief current IPv4 address as char array */
        char mac[MAC_LEN] = {};
        bool dirty = true;
    };
    /**
     * @brief represents a switchable relay. Multi-socket devices not yet supported.
     */
    struct WifiRelay
    {
        char ipV4Adress[IP_LEN] = {};
        char mac[MAC_LEN] = {};
        int rssi = 0; /**<@brief WiFi signal strength in dBm */
        char name[NAME_LEN] = {};
        bool inUse = false;           /**<@brief relay is registered and expected to be present */
        bool online = false;          /**<@brief relay is reachable; set to false when TCP fails */
        int id = 0;                   /**<@brief switch ID on the device; 0 for single socket, 0–3 for multi-socket */
        bool output = false;          /**<@brief current switch state */
        float voltage = 0.0f;         /**<@brief voltage in volts */
        float current = 0.0f;         /**<@brief current in amperes */
        float power = 0.0f;           /**<@brief active power in watts */
        float aenergy = 0.0f;         /**<@brief total energy consumption in kWh */
        unsigned long lastUpdate = 0; /**<@brief timestamp of last successful status read */
        bool isDirty = true;          /**<@brief set when GUI-relevant fields change */
        unsigned long switchDuration = 60000; /** <@brief time in ms how long switch is on, failsafe */
        RelayTypes::RelayMode relayMode=RelayTypes::RelayMode::FORCED_OFF; /** <@brief relay mode, default off */
    };
    /**
     * @brief represents a sensor data entry
     */
    struct SensorData
    {
        SensorType type = SensorType::UNKNOWN; /** <@brief sensor type — determines which API the SensorManager calls */
        char name[NAME_LEN] = {};              /** <@brief name or label */
        float value = 0.0f;                    /** <@brief measured value */
        float calMin = 0.0f;                   /** <@brief minimum calibration value for two point calibration (e.g. minimum soil humidity) */
        float calMax = 0.0f;                   /** <@brief maximum calibration value for two point calibration (e.g. maximum soil humidity) */
        float calOffset = 0.0f;                /** <@brief calibration valiue for offset (e.g. tara, or temperature) */
        char unit[UNIT_LEN] = {};              /** <@brief unit of the measured value, e.g. °C or % */
        bool inUse = false;                    /** <@brief sensor is registered and expected to be present */
        bool online = false;                   /** <@brief last reading was successful; sensor is reachable and responding */
        unsigned long lastUpdate = 0;          /** <@brief timestamp of the last successful update */
        bool dirty = true;                     /** <@brief flag for GUI */
        unsigned long updateInterval = 10000;  /** <@brief update interval in ms */
        char sourceMac[MAC_LEN] = {};          /** <@brief empty for local sensors; MAC of remote ESP32-C3 node */
        int sensorIndex = 0;                   /** <@brief DHT22: physical sensor 0–3 | Remote: sensor ID on the node */
    };

    struct DataHub
    {
        SystemData systemData;
        WiFiStatus wifiStatus;
        WifiRelay wifiRelay[RELAY_SIZE];
        SensorData sensorData[SENSOR_DATA_SIZE];
    };
    /**
     * @brief Initializes the DataHub and its mutex. Sets default values.
     * @return true on success, false if mutex could not be taken.
     */
    bool initDataHub();

    /**
     * @brief Writes WiFi status to the DataHub.
     * @param in WiFiStatus to write.
     * @return true on success, false if mutex could not be taken.
     */
    bool setWifiStatus(const WiFiStatus &in);

    /**
     * @brief Reads WiFi status from the DataHub.
     * @param out Target object to fill.
     * @return true on success, false if mutex could not be taken.
     */
    bool getWiFiStatus(WiFiStatus &out);

    /**
     * @brief Writes system data to the DataHub.
     * @param in SystemData to write.
     * @return true on success, false if mutex could not be taken.
     */
    bool setSystemData(const SystemData &in);

    /**
     * @brief Reads system data from the DataHub.
     * @param out Target object to fill.
     * @return true on success, false if mutex could not be taken.
     */
    bool getSystemData(SystemData &out);

    /**
     * @brief Writes a WifiRelay entry at the given index.
     * @param in Entry to write.
     * @param index Target index (0 to RELAY_SIZE-1).
     * @return true on success, false on invalid index or mutex error.
     */
    bool setWifiRelayEntry(const WifiRelay &in, const int index);

    /**
     * @brief Searches for a WifiRelay entry by MAC address and relay ID.
     * @param out Found entry (only valid if return true).
     * @param idx Index of the found entry in the array.
     * @param mac MAC address as null-terminated string (normalized, no separators).
     * @param id Relay ID of the requested switch entry.
     * @return true if found, false otherwise.
     */
    bool getWifiRelayEntry(WifiRelay &out, int &idx, const char *mac, const int id);

    /**
     * @brief Getter for wifiRelay on given index.
     * @param out requested wifiRelay entry.
     * @param idx index of the requested entry
     * @return true if found, false if not found
     */
    bool getWifiRelayEntry(WifiRelay &out, const int &idx);

    /**
     * @brief Copies the complete WifiRelay array from the DataHub.
     * @param out Destination array of size RELAY_SIZE.
     * @return true on success, false if mutex could not be taken.
     */
    bool getWifiRelayArray(WifiRelay (&out)[RELAY_SIZE]);

    /**
     * @brief registers a SensorData in an unused slot
     * @param in the SensorData entry to register
     * @return the index of the registration. -2 if the array is full and -1 if there was a mutex error
     */
    int registerSensor(SensorData &in);

    /**
     * @brief deletes a SensorData entry by its id (index)
     * @param id the index of the SensorData entry
     * @return returns true on success
     */
    bool deleteSensor(int id);
    /**
     * @brief setter for SensorData entry. Copies the entry into the DataHub, overwrites the old Data
     * @param in the Sensor entry
     * @param id the index of the entry
     * @return returns true on success
     */
    bool setSensorData(SensorData &in, int id);

    /**
     * @brief getter for a SensorData entry, copies the entry into the given reference
     * @param out target entry
     * @param id identifier of the entry (not array index)
     * @return returns true on success
     */
    bool getSensorData(SensorData &out, int idx);

    /**
     * @brief getter for the SensorData Array, copies the array into the given reference
     * @param out target array
     * @return returns true on success
     */
    bool getSensorDataArray(SensorData (&out)[SENSOR_DATA_SIZE]);

    /**
     * @brief getter for the whole DataHub
     * @param out target buffer
     * @return true on success
     */
    bool getDataHub(DataHub &out);

    //################## debug / serial output ####################
    /**
     * @brief Logs a single WifiRelay entry to Serial.
     * @param relay Reference to the entry.
     */
    void wifiRelayToSerial(const WifiRelay &relay);

    /**
     * @brief Prints the sensorData array to Serial for debugging.
     */
    void sensorDataToSerial();

    /**
     * @brief Writes the complete DataHub state to Serial for debugging.
     */
    void dataHubToSerial();

    /**
     * @brief Compact status display of all active relays with ANSI colors.
     *        Shows online state, output, current, power and age of last update.
     */
    void dataHubStatusToSerial();
}