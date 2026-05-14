#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "HttpClient.h"
#include <WiFi.h>


namespace
{
    /**
     * @brief Skips the complete HTTP header in the stream.
     *        Reads lines until an empty line (CRLF) appears.
     * @param client Open WiFiClient stream.
     */
    void skipHeader(WiFiClient &client)
    {
        char line[HTTP_HEADER_LINE_MAX];
        while (HttpClient::readLine(client, line, sizeof(line)) > 0)
        {
        }
    }
        /**
     * @brief Extracts the HTTP status code from the first response line.
     *
     * Expected format: "HTTP/1.1 200 OK"
     *
     * @param line  First line of the HTTP response
     * @return HTTP status code as int (e.g. 200, 401), -1 if not found
     */
    int extractHTTPCode(char *line)
    {
        const char *space = strchr(line, ' ');
        if (!space)
            return -1;
        return atoi(space + 1);
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
        while ((client.connected() || client.available()) && (millis() - start) < TCP_MAX_TIME)
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
        unsigned long start = millis();
        while (len < bufferSize - 1)
        {
            if (!client.available())
            {
                if (!client.connected() || (millis() - start) >= TCP_MAX_TIME)
                    break;
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            start = millis();
            char c = client.read();
            if (c == '\n')
                break;
            if (c != '\r')
                buffer[len++] = c;
        }
        buffer[len] = '\0';
        //Serial.println(buffer);
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
        while ((client.connected() || client.available()) && (millis() - start) < TCP_MAX_TIME)
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