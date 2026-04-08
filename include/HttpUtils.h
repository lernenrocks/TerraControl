#pragma once
#define TCP_MAX_TIME 5000UL // 5s Versuch Daten über TCP zu empfangen
#define URL_PATH_LEN 128                // Länge für einen URL Puffer
#define HTTP_RESPONSE_LINE_BUFFER_LEN 512
#define HTTP_HEADER_LINE_MAX 128
#define KEY_BUFFER_MAX 64 // Buffer für Value

#include <WiFiClient.h>
#include "DataHub.h"

namespace HttpUtils
{
    /**
     * @brief Liest eine Zeile aus dem WiFiClient-Stream.
     *        Liest bis \\n, überspringt \\r, terminiert mit \\0.
     * @param client  Geöffneter WiFiClient-Stream.
     * @param buffer  Zielpuffer.
     * @param bufferSize Größe des Puffers inkl. Nullterminator.
     * @return Anzahl gelesener Zeichen (0 = Leerzeile oder kein Inhalt).
     */
    size_t readLine(WiFiClient &client, char *buffer, size_t bufferSize);

    /**
     * @brief Überspringt den kompletten HTTP-Header im Stream.
     *        Liest so lange Zeilen, bis eine Leerzeile (\\r\\n) erscheint.
     * @param client Geöffneter WiFiClient-Stream.
     */
    void skipHeader(WiFiClient &client);

    /**
     * @brief Normalisiert eine MAC-Adresse auf Kleinbuchstaben in-place.
     *        Erwartet einen null-terminierten String ohne Trennzeichen (z.B. "8CBFEAA03350").
     * @param mac Zu normalisierende MAC-Adresse.
     */
    void normalizeMac(char *mac);
}
