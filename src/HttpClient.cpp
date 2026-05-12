#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "HttpClient.h"
#include <WiFi.h>


namespace
{
    /**
     * @brief Überspringt den kompletten HTTP-Header im Stream.
     *        Liest so lange Zeilen, bis eine Leerzeile (\\r\\n) erscheint.
     * @param client Geöffneter WiFiClient-Stream.
     */
    void skipHeader(WiFiClient &client)
    {
        char line[HTTP_HEADER_LINE_MAX];
        while (HttpClient::readLine(client, line, sizeof(line)) > 0)
        {
        }
    }
        /**
     * @brief Extrahiert den HTTP-Statuscode aus der ersten Antwortzeile.
     *
     * Erwartet Format: "HTTP/1.1 200 OK"
     *
     * @param line  Erste Zeile der HTTP-Antwort
     * @return HTTP-Statuscode als int (z.B. 200, 401), -1 wenn kein Code gefunden
     */
    int extractHTTPCode(char *line)
    {
        const char *space = strchr(line, ' ');
        if (!space)
            return -1;
        return atoi(space + 1); // atoi ASCII zu Integer, bis zeichen nicht als Zahl interpretiert werden kann. nullptr, wenn nicht.
    }
}

namespace HttpClient
{
    int get(const char *ip, const char *uri, WiFiClient &client, const char *authHeader)
    {
        if (!client.connect(ip, 80, 3000))
        {
            client.stop();
            return -1;
        }
        char url[URL_PATH_LEN];
        snprintf(url, sizeof(url), "/%s", uri);
        client.printf("GET %s HTTP/1.1\r\nHost: %s\r\n", url, ip);
        if (authHeader)
            client.printf("Authorization: %s\r\n", authHeader);
        client.printf("Connection: close\r\n\r\n");

        char lineBuffer[HTTP_HEADER_LINE_MAX];
        int code = 0;
        unsigned long start = millis();
        while (client.connected() && (millis() - start) < TCP_MAX_TIME)
        {
            if (!client.available())
            {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            size_t lineLen = readLine(client, lineBuffer, sizeof(lineBuffer));
            if (lineLen == 0)
                break;
            if (code == 0)
            {
                int c = extractHTTPCode(lineBuffer);
                if (c != -1)
                    code = c;
            }
        }
        return code;
    }

    size_t readLine(WiFiClient &client, char *buffer, size_t bufferSize)
    {
        size_t len = 0;
        while (client.available() && len < bufferSize - 1)
        {
            char c = client.read();
            if (c == '\n')
                break;
            if (c != '\r')
                buffer[len++] = c;
        }
        buffer[len] = '\0';
        // DEBUG drinnen lassen für Bugfixing
        //  Serial.println(buffer);
        return len;
    }

    int sendGet(const char *ip, const char *uri, WiFiClient &client, const char *authHeader)
    {
        if (!client.connect(ip, 80, 3000))
        {
            client.stop();
            return -1;
        }
        char url[URL_PATH_LEN];
        snprintf(url, sizeof(url), "/%s", uri);
        client.printf("GET %s HTTP/1.1\r\nHost: %s\r\n", url, ip);
        if (authHeader)
            client.printf("Authorization: %s\r\n", authHeader);
        client.printf("Connection: close\r\n\r\n");

        char lineBuffer[HTTP_HEADER_LINE_MAX];
        unsigned long start = millis();
        while (client.connected() && (millis() - start) < TCP_MAX_TIME)
        {
            if (!client.available())
            {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            size_t lineLen = readLine(client, lineBuffer, sizeof(lineBuffer));
            if (lineLen == 0)
                return 0;
            int code = extractHTTPCode(lineBuffer);
            if (code != -1)
                return code;
        }
        return -1;
    }
}