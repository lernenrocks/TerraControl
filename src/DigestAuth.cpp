#include <WiFi.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>

#include "DigestAuth.h"
#include "HttpClient.h"

// Puffer Größen
#define SHA256_HEX_LEN 64
#define REALM_MAX 64                    // Größe des Schutzbereichs (bei Shelly z.B. Geräte ID)
#define NONCE_MAX 32                    // Number used once: Zufallszahl, die bei jedem Request neu generiert wird, kommt von Server, verhindert Replay Angriffe
#define CNONCE_LEN 8                    // CLient Number used once: Zufallszahl durch den Client, Server kann Hash Berechnung nicht mehr kontrollieren und vorhersagen, verhindert Wörterbuchangriffe
#define WWW_AUTHENTICATE_HEADER_MAX 256 // Größe des vom Server gesendeten authentification Headers mit den Bestandteilen für die Hash Generierung
#define AUTORISATION_HEADER_MAX 512     // Header für den 2. HS mit den gehashten Zugangsdaten und dem GET Request
#define HASH_INPUT_MAX 256              // Puffer für die Eingabe in die Hash Funktion

namespace
{ // === Interne Hilfsfunktionen (nur DigestAuth.cpp) ===

    /**
     * @brief Extrahiert einen Wert aus einem HTTP-Header-Feld.
     *
     * Unterstützt zwei Formate:
     * - key="value"  (Anführungszeichen, z.B. realm="shellyplugsg3-...")
     * - key=value    (ohne Anführungszeichen, z.B. algorithm=SHA-256)
     *
     * @param line     Vollständige Header-Zeile als C-String
     * @param key      Gesuchter Schlüsselname
     * @param dest     Zielpuffer für den extrahierten Wert
     * @param destSize Größe des Zielpuffers (inkl. Nullterminator)
     * @return true wenn Wert gefunden und in dest geschrieben, false sonst
     */
    bool extractValue(const char *line, const char *key, char *dest, size_t destSize)
    {
        // ### 1. Fall: key steht in Quotes ###
        //  suche nach key="..." Verwendet, wenn Anführungszeichen um den Value stehen
        char keyQ[KEY_BUFFER_MAX];
        snprintf(keyQ, sizeof(keyQ), "%s=\"", key); // kopiert den key mit dem ersten Anführungszeichen
        const char *start = strstr(line, keyQ);
        if (start)
        {                                         // Pointer auf erstes Zeichen des Subchar[] im char[], nullpointer, falls nicht drin
            start += strlen(keyQ);                // veschieben zum Anfang des Keys
            const char *end = strchr(start, '"'); // end Pointer aufs erste " nach start - ist das Ende des gew. Key
            if (end)
            {
                size_t length = (size_t)(end - start);
                if (length >= destSize)
                { // defensives Prüfen, Abschneiden, falls nicht, Verhindert Pufferüberläufe durch fehlerhafte Server
                    length = destSize - 1;
                }
                memcpy(dest, start, length);
                dest[length] = '\0';
                return true;
            }
        }
        // ### 2. Fall: key steht nicht in Quotes, ähnliches Prinzip wie Fall 1
        char keyP[KEY_BUFFER_MAX];
        snprintf(keyP, sizeof(keyP), "%s=", key);
        start = strstr(line, keyP);
        if (start)
        {
            start += strlen(keyP);
            const char *end = strpbrk(start, ", \r\n");                  // Pointer auf das erste gefundenen Zeichen einer Menge, oder nullptr
            size_t length = end ? (size_t)(end - start) : strlen(start); // falls end==nullptr ist die Länge bis zum nächsten \0
            if (length >= destSize)
            {
                length = destSize - 1;
            }
            memcpy(dest, start, length);
            dest[length] = '\0';
            return true;
        }
        // keinen Key extrahiert
        dest[0] = '\0';
        return false;
    }
    /**
     * @brief Berechnet den SHA-256 Hash und gibt ihn als Hex-String zurück.
     *
     * @param input  Eingabedaten als C-String
     * @param output Zielpuffer, muss mindestens SHA256_HEX_LEN+1 (65) Bytes groß sein
     * @param length Länge der Eingabe in Bytes
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
            Serial.println("[ERROR] DigestAuth: Verbindung fehlgeschlagen");
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
            // 2. Extrahiere realm und nonce aus WWW-Authenticate
            /* Beispiel:
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
            Serial.printf("[ERROR] DigestAuth: realm/nonce nicht erhalten (HTTP %d)\n", firstCode);
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
        snprintf(cnonce, sizeof(cnonce), "%08x", (uint32_t)esp_random()); // eigene Zufallszahl für Verschleierung
        // nc:
        const char *nc = "1"; // Shelly Gen2/3 erwartet 1
        //? bei anderen Gerätetypen in ein ENUM auslagern
        // response:
        char responseInput[HASH_INPUT_MAX + 1];
        char responseHash[SHA256_HEX_LEN + 1];
        snprintf(responseInput, sizeof(responseInput), "%s:%s:%s:%s:auth:%s", ha1, nonce, nc, cnonce, ha2);
        sha256Hex(responseInput, responseHash, strlen(responseInput));

        // 4. Response
        char autorisationHeader[AUTORISATION_HEADER_MAX];
        snprintf(autorisationHeader, sizeof(autorisationHeader),
                 "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\","
                 " algorithm=SHA-256, qop=auth, nc=%s, cnonce=\"%s\", response=\"%s\"",
                 user, realm, nonce, uri, nc, cnonce, responseHash);
        return HttpClient::get(ip, uri, wifiClient, autorisationHeader);
    }
}
