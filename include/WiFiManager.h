#pragma once
#include <Arduino.h>
// WiFiManager - Neuaufbau
// Minimale Struktur für ESP32 ohne angeschlossene Hardware

namespace WiFiManager
{
    /**
     * @brief Startet den Soft-AP mit Gerätename und Passwort aus System, Modus AP+STA.
     * @return true bei Erfolg, false wenn Gerätename oder Passwort nicht lesbar sind.
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

}