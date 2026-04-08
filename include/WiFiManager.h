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
     * @brief sucht nach mDNS Services und sendet ein GET Status. Prüft anhand der mac Adresse im
     * Body, ob diese der WhiteList wifiRelay stehen. Ordnet die aktuelle IP zu, setzt diese online.
     * @warning blockiert für 1s, damit alle mDNS besser gefunden werden.
     * @note möglichst nur im setup() aufrufen.
     */
    void findStoredRelays();

    /**
     * @brief Fragt den Shelly-Status unter der angegebenen IP ab und aktualisiert den DataHub.
     *        Setzt betroffene Entries bei Fehler auf offline.
     * @param ip IPv4-Adresse des Shelly als null-terminierter String.
     * @return true bei erfolgreichem Abruf, false sonst.
     */
    bool updateRelayStatus(char *ip);

    /**
     * @brief Schaltet ein Relay eines Shelly per Legacy HTTP-Endpoint und aktualisiert danach den Status.
     * @param mac MAC-Adresse des Shelly (normalisiert, ohne Trennzeichen).
     * @param id Relay-ID (0-basiert).
     * @param on true = einschalten, false = ausschalten.
     * @param duration Timer in Sekunden, nach dem das Relay automatisch zurückschaltet. 0 = kein Timer.
     */
    void switchRelay(const char *mac, int id, bool on, long duration);

    /**
     * @brief Prüft, wann ein Relay zum letzten mal geupdatet wurde und löst nach einem
     * einer definierten Zeit das erneute Finden aller wifiRelays aus.
     */
    void checkRelayTimeouts();
}