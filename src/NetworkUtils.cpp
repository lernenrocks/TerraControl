#include <Arduino.h>
#include <WiFi.h>
#include "NetworkUtils.h"

namespace NetworkUtils
{
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
    void macBytesToChar(char *out, const uint8_t byteMac[6], size_t len){
        snprintf(out,len,"%02x%02x%02x%02x%02x%02x",
            byteMac[0],byteMac[1],byteMac[2],byteMac[3],byteMac[4],byteMac[5]);
    }
    void ipToChar(char *out, const uint32_t ip,size_t len){
        // uint32_t garantiert exakt 32 Bit — portabel, unabhängig von der Plattform.
        // ip speichert alle vier Oktette hintereinander (little-endian: erstes Oktett in den untersten Bits).
        // 0xFF hat nur die untersten 8 Bit gesetzt, der Rest ist 0.
        // & 0xFF isoliert das unterste Byte: AND mit 0 blockiert, AND mit 1 lässt den Originalwert durch.
        // >> 8 schiebt das nächste Byte an die unterste Position, damit & 0xFF es wieder isolieren kann.
        snprintf(out, len, "%u.%u.%u.%u",
        ip & 0xFF,
        (ip >> 8) & 0xFF,
        (ip >> 16) & 0xFF,
        (ip >> 24) & 0xFF);
    }
}