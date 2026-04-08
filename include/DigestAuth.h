#pragma once

/**
 * @brief HTTP Digest Authentication für Shelly Gen2/3 Geräte.
 *
 * Implementiert den zweistufigen SHA-256 Digest Auth Handshake
 * über direktes TCP (WiFiClient), ohne HTTPClient oder Arduino String.
 * Alle Puffer liegen auf dem Stack.
 */
namespace DigestAuth
{

    /**
     * @brief Sendet einen zweistufigen HTTP Digest Authentication GET-Request.
     *
     * 1. Stufe: Request ohne Credentials → Server antwortet mit 401 + WWW-Authenticate Header
     * 2. Stufe: Berechnet SHA-256 Hashes (ha1, ha2, response) und sendet autorisierten Request
     *
     * @param ip      IP-Adresse des Ziels, z.B. "192.168.2.227"
     * @param uri     URL-Pfad ohne führenden Schrägstrich, z.B. "relay/0?turn=on"
     * @param user    Benutzername als Klartext
     * @param password Passwort als Klartext
     * @return HTTP-Statuscode der zweiten Antwort (z.B. 200), -1 bei Verbindungsfehler
     */
    int digestAuthRequest(const char *ip, const char *uri,
                          const char *user, const char *password, WiFiClient &wifiClient);

}
