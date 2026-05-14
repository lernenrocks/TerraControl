#pragma once
#include <WiFiClient.h>

#define TCP_MAX_TIME 5000UL // 5s Versuch Daten über TCP zu empfangen
#define URL_PATH_LEN 128                // Länge für einen URL Puffer
#define HTTP_RESPONSE_LINE_BUFFER_LEN 512
#define KEY_BUFFER_MAX 64 // Buffer für Value
#define HTTP_HEADER_LINE_MAX 128

namespace HttpClient{
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
     * @brief Sendet einen HTTP GET Request und liest die Response-Header.
     *        Stream liegt am Body bei Rückgabe.
     * @param ip         Ziel-IP als null-terminierter String.
     * @param uri        Pfad ohne führenden Slash (z.B. "rpc/Shelly.GetStatus").
     * @param client     Geöffneter WiFiClient — gehört dem Caller.
     * @param authHeader Optionaler Authorization-Header, nullptr für unauthentifizierte Requests.
     * @return HTTP-Statuscode, -1 bei Verbindungsfehler.
     */
    int get(const char *ip, const char *uri, WiFiClient &client, const char *authHeader = nullptr);

    /**
     * @brief Sends a GET request and reads only the status line.
     *        Remaining response headers stay in the stream for the caller to read.
     * @param ip         Target IP as null-terminated string.
     * @param uri        Path without leading slash.
     * @param client     Open WiFiClient — owned by caller.
     * @param authHeader Optional Authorization header, nullptr for unauthenticated requests.
     * @return HTTP status code, -1 on connection error, 0 if no status line received.
     */
    int sendGet(const char *ip, const char *uri, WiFiClient &client, const char *authHeader = nullptr);
}