#pragma once


#include <WiFiClient.h>

constexpr size_t MAC_LEN = 6;         // raw MAC address bytes
constexpr size_t MAC_STRING_LEN = 13; // normalized hex MAC string incl. terminator, e.g. "8cbfeaa03350"
constexpr size_t IP_LEN = 16;         // "255.255.255.255" incl. terminator

namespace NetworkUtils
{
    /**
     * @brief Normalisiert eine MAC-Adresse auf Kleinbuchstaben in-place.
     *        Erwartet einen null-terminierten String ohne Trennzeichen (z.B. "8CBFEAA03350").
     * @param mac Zu normalisierende MAC-Adresse.
     */
    void normalizeMac(char *mac);

    /**
     * @brief converts a mac address in bytes to char array
     * @param out buffer for mac as char
     * @param byteMac mac as byte array to convert
     * @param len length of the buffer to write
     * @return true on success, false if the buffer is too small (< MAC_STRING_LEN)
    */
    bool macBytesToChar(char *out, const uint8_t byteMac[6], size_t len);

    /**
     * @brief converts a mac address given as char array to bytes
     * @param byteMac buffer for mac as bytes (6 bytes)
     * @param mac mac as null-terminated hex string without separators (e.g. "8cbfeaa03350")
     * @return true if all 6 bytes were parsed successfully, false on malformed input
    */
    bool macCharToBytes(uint8_t byteMac[6], const char *mac);

    /**
     * @brief converts an ip address given as uint32_t to char array
     * @param out buffer for ip as char
     * @param ip id address to convert
     * @param len length of the buffer to write
     * @return true on success, false if the buffer is too small (< IP_LEN)
     */
    bool ipToChar(char *out, const uint32_t ip,size_t len);
}

