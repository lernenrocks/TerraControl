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
        // uint32_t guarantees exactly 32 bits — portable across platforms.
        // ip stores all four octets consecutively (little-endian: first octet in lowest bits).
        // 0xFF has only the lowest 8 bits set, rest are 0.
        // & 0xFF isolates the lowest byte: AND with 0 blocks, AND with 1 passes through.
        // >> 8 shifts the next byte into the lowest position so & 0xFF can isolate it.
        snprintf(out, len, "%u.%u.%u.%u",
        ip & 0xFF,
        (ip >> 8) & 0xFF,
        (ip >> 16) & 0xFF,
        (ip >> 24) & 0xFF);
    }
}