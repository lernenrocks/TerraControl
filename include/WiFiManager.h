#pragma once
#include <Arduino.h>
// WiFiManager - Neuaufbau
// Minimale Struktur für ESP32 ohne angeschlossene Hardware

namespace WiFiManager
{
    /**
     * @brief Initialisiert WiFi-Verbindung und wifiMutex. Startet mDNS mit dem Gerätenamen aus dem DataHub.
     * @return true bei erfolgreicher Verbindung und mDNS-Start, false bei Timeout oder mDNS-Fehler.
     */
    bool initWiFi();

    void syncStaConnection();
    bool resolveIp(const uint8_t *mac, char *buffer, size_t len);

    /**
     * @brief Liest die aktuelle STA-Signalstärke.
     * @param rssi Zielvariable für RSSI in dBm.
     * @return true bei Erfolg, false wenn nicht verbunden.
     */
    bool getRssi(int &rssi);

    /**
     * @brief Liest die aktuelle STA-IPv4-Adresse.
     * @param buffer Zielpuffer für die IP als String.
     * @param len Größe des Zielpuffers.
     * @return true bei Erfolg, false wenn nicht verbunden.
     */
    bool getLocalIp(char *buffer, size_t len);

    /**
     * @brief Fragt den Shelly-Status unter der angegebenen IP ab und aktualisiert den DataHub.
     *        Setzt betroffene Entries bei Fehler auf offline.
     * @param ip IPv4-Adresse des Shelly als null-terminierter String.
     * @return 200 bei Erfolg, -1 bei TCP-Verbindungsfehler, 0 bei Parse-Fehler, HTTP-Code sonst.
     */
    int updateRelayStatus(char *ip);


}