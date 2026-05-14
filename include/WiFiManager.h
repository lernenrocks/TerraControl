#pragma once
#include "DataHub.h"
// WiFiManager - Neuaufbau
// Minimale Struktur für ESP32 ohne angeschlossene Hardware

namespace WiFiManager
{
    /**
     * @brief Initialisiert WiFi-Verbindung und wifiMutex. Startet mDNS mit dem Gerätenamen aus dem DataHub.
     * @return true bei erfolgreicher Verbindung und mDNS-Start, false bei Timeout oder mDNS-Fehler.
     */
    bool initWiFi();

    /**
     * @brief Liest aktuellen WiFi-Status (SSID, IP, RSSI) und schreibt ihn in den DataHub.
     *        Tut nichts, wenn WiFi nicht verbunden.
     */
    void updateWifiStatus();

    /**
     * @brief Fragt den Shelly-Status unter der angegebenen IP ab und aktualisiert den DataHub.
     *        Setzt betroffene Entries bei Fehler auf offline.
     * @param ip IPv4-Adresse des Shelly als null-terminierter String.
     * @return 200 bei Erfolg, -1 bei TCP-Verbindungsfehler, 0 bei Parse-Fehler, HTTP-Code sonst.
     */
    int updateRelayStatus(char *ip);

    /**
     * @brief Schaltet ein Relay eines Shelly per Legacy HTTP-Endpoint und aktualisiert danach den Status.
     * @param relay wifiRelay entry to switch
     * @param Switchon true = switch relay on, false = switch Relay off.
     * @return true, if wifiRelay was found
     */
    bool switchRelay(DataHub::WifiRelay relay, bool switchOn);

}