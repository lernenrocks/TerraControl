#pragma once


#include <WiFiClient.h>
#include "DataHub.h"

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
    */
    void macBytesToChar(char *out, const uint8_t byteMac[6], size_t len);

    /**
     * @brief converts an ip address given as uint32_t to char array
     * @param out buffer for ip as char
     * @param ip id address to convert
     * @param len length of the buffer to write
     */
    void ipToChar(char *out, const uint32_t ip,size_t len);
}

