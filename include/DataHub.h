#pragma once

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
    };

    struct WiFiStatus
    {
        char ssid[33] = {};           /**< @brief ssid - max 32 Zeichen */
        int rssi = 0;                 /**< @brief Signal Stärke in mDB */
        char ipV4Adress[IP_LEN] = {}; /**< @brief aktuelle IPv4 Adresse als car array*/
        char mac[MAC_LEN] = {};
        bool dirty = true;
    };
    /**
     * @brief ein WifiRelay symbolisiert einen Schalter. Mehrfachdosen derzeit nicht vorgesehen.
     */
    struct WifiRelay
    {
        char ipV4Adress[IP_LEN] = {};
        char mac[MAC_LEN] = {};
        int rssi = 0; /**<@brief Wlan Dämpfung */
        char name[NAME_LEN] = {};
        bool inUse = false;           /**<@brief Relay soll gefunden werden (nichtfinden ist Fehlerzustand) */
        bool online = false;          /**<@brief Relay ist ansteuerbar (wird false, wenn nicht mehr da) */
        int id = 0;                   /**<@brief ID der Switch im Relay, bei einfacher Dose 0, bei $ fach 0-3 */
        bool output = false;          /**<@brief ist Switch angeschaltet? */
        float voltage = 0.0f;         /**<@brief Spannung in Volt */
        float current = 0.0f;         /**<@brief Stromstärke in Ampere */
        float power = 0.0f;           /**<@brief Leistung in Watt */
        float aenergy = 0.0f;         /**<@brief Verbrauch in kWh */
        unsigned long lastUpdate = 0; /**<@brief Zeitstempel, wenn das letzte mal die Dose angesprochen wurde */
        bool isDirty = true;          /**<@brief GUI relevante Änderungen wurden gemacht */
    };
    /**
     * @brief represents a sensor data entry
     */
    struct SensorData
    {
        char name[NAME_LEN]; /** <@brief name or label  */
        float value = 0.0f;  /** <@brief measured value */
        char unit[UNIT_LEN] = {};/** <@brief unit of the measured value, e.g. °C or g */
        bool inUse = false; /** <@brief Sensor is represented and expected to be present */
        bool online = false;/** <@brief last reading was succsessful; sensor is reachable and responding  */
        unsigned long lastUpdate = 0; /**<@brief timestamp of the last successful update of this struct */
        bool dirty = true; /** <@brief flag for GUI */
    };

    struct DataHub
    {
        SystemData systemData;
        WiFiStatus wifiStatus;
        WifiRelay wifiRelay[RELAY_SIZE];
        SensorData sensorData[SENSOR_DATA_SIZE];
    };
    /**
     * @brief Initialisiert den DataHub und seinen Mutex. Setzt Standardwerte.
     * @return true bei Erfolg, false wenn Mutex nicht genommen werden konnte.
     */
    bool initDataHub();

    /**
     * @brief Schreibt den WiFi-Status in den DataHub.
     * @param in Zu schreibender WiFiStatus.
     * @return true bei Erfolg, false wenn Mutex nicht genommen werden konnte.
     */
    bool setWifiStatus(const WiFiStatus &in);

    /**
     * @brief Liest den WiFi-Status aus dem DataHub.
     * @param out Zielobjekt, das befüllt wird.
     * @return true bei Erfolg, false wenn Mutex nicht genommen werden konnte.
     */
    bool getWiFiStatus(WiFiStatus &out);

    /**
     * @brief Schreibt die System-Daten in den DataHub.
     * @param in Zu schreibende SystemData.
     * @return true bei Erfolg, false wenn Mutex nicht genommen werden konnte.
     */
    bool setSystemData(const SystemData &in);

    /**
     * @brief Liest die System-Daten aus dem DataHub.
     * @param out Zielobjekt, das befüllt wird.
     * @return true bei Erfolg, false wenn Mutex nicht genommen werden konnte.
     */
    bool getSystemData(SystemData &out);

    /**
     * @brief Schreibt einen WifiRelay-Eintrag an den angegebenen Index.
     * @param in Zu schreibender Eintrag.
     * @param index Zielindex (0 bis RELAY_SIZE-1).
     * @return true bei Erfolg, false bei ungültigem Index oder Mutex-Fehler.
     */
    bool setWifiRelayEntry(const WifiRelay &in, const int index);

    /**
     * @brief Sucht einen WifiRelay-Eintrag anhand von MAC-Adresse und Relay-ID.
     * @param out Gefundener Eintrag (nur gültig wenn return true).
     * @param idx Index des gefundenen Eintrags im Array.
     * @param mac MAC-Adresse als null-terminierter String (normalisiert, ohne Trennzeichen).
     * @param id Relay-ID des gesuchten Switch-Eintrags.
     * @return true wenn gefunden, false sonst.
     */
    bool getWifiRelayEntryByMac(WifiRelay &out, int &idx, const char *mac, const int id);

    /**
     * @brief Kopiert das komplette WifiRelay-Array aus dem DataHub.
     * @param out Ziel-Array der Größe RELAY_SIZE.
     * @return true bei Erfolg, false wenn Mutex nicht genommen werden konnte.
     */
    bool getWifiRelayArray(WifiRelay (&out)[RELAY_SIZE]);

    /**
     * @brief Setzt alle WifiRelay-Einträge mit der angegebenen IP auf online=false.
     *        Wird aufgerufen, wenn ein HTTP-Request an diese IP fehlschlägt.
     * @param ip IPv4-Adresse als null-terminierter String.
     */
    void setWifiRelayOfflineByIP(const char *ip);

    /**
     * @brief registers a SensorData in an unused slot
     * @param in the SensorData entry to register
     * @return the index of the registration. -2 if the array was allready full and -1 if there was a mutex error 
     */
    int registerSensor(SensorData &in);

    /**
     * @brief deletes a SensorData entry by its id (index)
     * @param id the index of the SensorData entry
     * @return returns true on success
     */
    bool deleteSensor(int id);
    /**
     * @brief setter for SensorData entry. Copies the entry into the DataHub
     * @param in the Sensor entry
     * @param id the identifier of the entry (not array index)
     * @return returns true on success
     */
    bool setSensorData(SensorData &in, int id);

    /**
     * @brief getter for a SensorData entry, copies the entry into the given reference
     * @param out target entry
     * @param id identifier of the entry (not array index)
     * @return returns true on success
     */
    bool getSensorData(SensorData &out, int id);

    /**
     * @brief getter for the SensorData Array, copies the array into the given reference
     * @param out target array
     * @return returns true on success
     */
    bool getSensorDataArray(SensorData (&out)[SENSOR_DATA_SIZE]);



    //toString funktions
    /**
     * @brief Loggt einen einzelnen WifiRelay-Eintrag auf Serial.
     * @param relay Referenz zum Eintrag.
     */
    void wifiRelayToSerial(const WifiRelay &relay);


    /**
     * @brief prints the sensorData Array to Serial for debugging
     */
    void sensorDataToSerial();
    /**
     * @brief Schreibt den kompletten DataHub-Zustand auf Serial (Debug / Logging).
     */
    void dataHubToSerial();

    /**
     * @brief Kompakte Statusanzeige aller aktiven Relays mit ANSI-Farben.
     *        ONLINE/OFFLINE, ON/OFF, Strom, Leistung, Alter des letzten Updates.
     *        Für Debugging im laufenden Betrieb (z.B. Relay abstecken).
     */
    void dataHubStatusToSerial();
}