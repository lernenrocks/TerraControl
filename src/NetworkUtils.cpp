#include <Arduino.h>
#include <WiFi.h>
#include "NetworkUtils.h"
#include <cstdio>
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
    bool macBytesToChar(char *out, const uint8_t byteMac[6], size_t len){
        if (len < MAC_STRING_LEN)
            return false;
        snprintf(out,len,"%02x%02x%02x%02x%02x%02x",
            byteMac[0],byteMac[1],byteMac[2],byteMac[3],byteMac[4],byteMac[5]);
        return true;
    }
    bool macCharToBytes(uint8_t byteMac[6], const char *mac){
        // Only exactly 12 hex digits are a MAC. Checked before sscanf: "%2hhx" reads *up to*
        // two digits, stops at the first non-hex character and still counts the byte - so
        // "8cbfeaa0335g" would pass as ...:05. It also accepts signs and leading spaces.
        if (!mac || strlen(mac) != MAC_STRING_LEN - 1)
            return false;
        for (size_t i = 0; i < MAC_STRING_LEN - 1; i++)
        {
            // cast: isxdigit() is undefined for negative char values (e.g. umlauts)
            if (!isxdigit(static_cast<unsigned char>(mac[i])))
                return false;
        }
        return sscanf(mac, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
            &byteMac[0],&byteMac[1],&byteMac[2],&byteMac[3],&byteMac[4],&byteMac[5]) == 6;
    }
    bool ipToChar(char *out, const uint32_t ip,size_t len){
        if (len < IP_LEN)
            return false;
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
        return true;
    }
}