#include <Arduino.h>
#include <WiFi.h>
#include "HttpUtils.h"

namespace HttpUtils
{
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
        //DEBUG drinnen lassen für Bugfixing
        // Serial.println(buffer);
        return len;
    }

    void skipHeader(WiFiClient &client)
    {
        char line[HTTP_HEADER_LINE_MAX];
        while (HttpUtils::readLine(client, line, sizeof(line)) > 0)
        {
        }
    }

    void normalizeMac(char *mac)
    {
        for (int i = 0; mac[i] != 0; i++)
        {
            if (mac[i] >= 'A' && mac[i] <= 'F')
            {
                mac[i] += 'a' - 'A';
            }
        }
    }
}