#include <WiFi.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>

#include "DigestAuth.h"
#include "HttpClient.h"

// buffer sizes
#define SHA256_HEX_LEN 64
#define REALM_MAX 64                    // protection scope (Shelly uses device ID)
#define NONCE_MAX 32                    // server-generated random per request, prevents replay attacks
#define CNONCE_LEN 8                    // client-generated random, prevents dictionary attacks
#define WWW_AUTHENTICATE_HEADER_MAX 256 // auth challenge header from server
#define AUTORIZATION_HEADER_MAX 512     // auth response header with hashed credentials
#define HASH_INPUT_MAX 256              // hash function input buffer

namespace
{
    /**
     * @brief Extracts a value from an HTTP header field.
     *
     * Supports two formats:
     * - key="value"  (quoted, e.g. realm="shellyplugsg3-...")
     * - key=value    (unquoted, e.g. algorithm=SHA-256)
     *
     * @param line     Complete header line as C-string
     * @param key      Key name to search for
     * @param dest     Destination buffer for extracted value
     * @param destSize Size of destination buffer (including null terminator)
     * @return true if value found and written to dest, false otherwise
     */
    bool extractValue(const char *line, const char *key, char *dest, size_t destSize)
    {
        // case 1: quoted value
        char keyQ[KEY_BUFFER_MAX];
        snprintf(keyQ, sizeof(keyQ), "%s=\"", key);
        const char *start = strstr(line, keyQ);
        if (start)
        {
            start += strlen(keyQ);                // advance past key="
            const char *end = strchr(start, '"'); // closing quote marks end of value
            if (end)
            {
                size_t length = (size_t)(end - start);
                if (length >= destSize)
                { // clamp to prevent buffer overflow from malformed server responses
                    length = destSize - 1;
                }
                memcpy(dest, start, length);
                dest[length] = '\0';
                return true;
            }
        }
        // case 2: unquoted value
        char keyP[KEY_BUFFER_MAX];
        snprintf(keyP, sizeof(keyP), "%s=", key);
        start = strstr(line, keyP);
        if (start)
        {
            start += strlen(keyP);
            const char *end = strpbrk(start, ", \r\n");
            size_t length = end ? (size_t)(end - start) : strlen(start); // end==nullptr: value extends to end of string
            if (length >= destSize)
            {
                length = destSize - 1;
            }
            memcpy(dest, start, length);
            dest[length] = '\0';
            return true;
        }
        // key not found
        dest[0] = '\0';
        return false;
    }
    /**
     * @brief Computes SHA-256 hash and returns it as a hex string.
     *
     * @param input  Input data as C-string
     * @param output Destination buffer, must be at least SHA256_HEX_LEN+1 (65) bytes
     * @param length Length of input in bytes
     */
    void sha256Hex(const char *input, char output[SHA256_HEX_LEN + 1], size_t length)
    {
        byte digest[32];
        mbedtls_sha256((const byte *)input, length, digest, 0);
        for (int i = 0; i < sizeof(digest); i++)
        {
            sprintf(output + i * 2, "%02x", digest[i]);
        }
    }

}

namespace DigestAuth
{
    int digestAuthRequest(const char *ip, const char *uri,
                          const char *user, const char *password, WiFiClient &wifiClient)
    {

        // first handshake: retrieve auth challenge (realm, nonce)
        int firstCode = HttpClient::sendGet(ip, uri, wifiClient);
        if (firstCode == -1)
        {
            Serial.printf("[%lus][ERROR] DigestAuth: Verbindung fehlgeschlagen\n", millis() / 1000);
            return -1;
        }

        char realm[REALM_MAX] = {};
        char nonce[NONCE_MAX] = {};
        char lineBuffer[WWW_AUTHENTICATE_HEADER_MAX];
        unsigned long start = millis();
        while ((wifiClient.connected() || wifiClient.available()) && (millis() - start) < TCP_MAX_TIME)
        {
            if (!wifiClient.available())
            {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            size_t lineLen = HttpClient::readLine(wifiClient, lineBuffer, sizeof(lineBuffer));
            if (lineLen == 0)
                break;
            // 2. Extract realm und nonce aus WWW-Authenticate
            /* e.g.
             Digest qop="auth", realm="shellyplugsg3-8cbfeaa03350", nonce="1773258266", algorithm=SHA-256
             */
            if (strncmp(lineBuffer, "WWW-Authenticate:", 17) == 0)
            {
                if (!extractValue(lineBuffer, "realm", realm, sizeof(realm)) || !extractValue(lineBuffer, "nonce", nonce, sizeof(nonce)))
                {
                    Serial.println("realm oder nonce konnte nicht gefunden werden");
                    wifiClient.stop();
                    return -1;
                }
            }
        }
        if (realm[0] == '\0' || nonce[0] == '\0')
        {
            Serial.printf("[%lus][ERROR] DigestAuth: realm/nonce nicht erhalten (HTTP %d)\n", millis() / 1000, firstCode);
            wifiClient.stop();
            return -1;
        }
        // 3. Digest Response berechnen
        // ha1 = SHA256(user:realm:password)
        // ha2 = SHA256("GET:"+uri)
        // response=SHA256(ha1:nonce:nc:cnonce:auth:ha2)

        char ha1Input[HASH_INPUT_MAX];
        char ha1[SHA256_HEX_LEN + 1];
        // ha1:
        snprintf(ha1Input, sizeof(ha1Input), "%s:%s:%s", user, realm, password);
        sha256Hex(ha1Input, ha1, strlen(ha1Input));
        // ha2:
        char ha2Input[HASH_INPUT_MAX];
        char ha2[SHA256_HEX_LEN + 1];
        snprintf(ha2Input, sizeof(ha2Input), "GET:/%s", uri);
        sha256Hex(ha2Input, ha2, strlen(ha2Input));
        // cnonce:
        char cnonce[CNONCE_LEN + 1];
        snprintf(cnonce, sizeof(cnonce), "%08x", (uint32_t)esp_random()); // client-generated random
        // nc:
        const char *nc = "1"; // Shelly Gen2/3 expects nc=1
        //? move to enum for other device types
        // response:
        char responseInput[HASH_INPUT_MAX + 1];
        char responseHash[SHA256_HEX_LEN + 1];
        snprintf(responseInput, sizeof(responseInput), "%s:%s:%s:%s:auth:%s", ha1, nonce, nc, cnonce, ha2);
        sha256Hex(responseInput, responseHash, strlen(responseInput));

        // 4. Response
        char autorizationHeader[AUTORIZATION_HEADER_MAX];
        snprintf(autorizationHeader, sizeof(autorizationHeader),
                 "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\","
                 " algorithm=SHA-256, qop=auth, nc=%s, cnonce=\"%s\", response=\"%s\"",
                 user, realm, nonce, uri, nc, cnonce, responseHash);
        return HttpClient::get(ip, uri, wifiClient, autorizationHeader);
    }
}
