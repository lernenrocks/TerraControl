# TerraControl — Projektkontext für Claude

## Was ist das hier?

ESP32-Firmware zur Steuerung von Shelly Gen2/3-Relays über SHA-256 Digest Auth und mDNS-Discovery. Sensoren steuern zeitgesteuert und schwellwertbasiert das Schalten — ein Messwert pro Schaltentscheidung, keine Kombinationslogik.

Kein Merge mit dem Prototyp (`esp32/terrasteuerung`) — Neuentwicklung.

---

## Zusammenarbeit

- Moderne C++-Ansätze bevorzugen und erklären (z.B. anonymer Namespace statt `static` auf Dateiebene, `nullptr` statt `NULL`)
- Der User lernt aktiv — neue Konzepte kurz erklären wenn sie eingesetzt werden

---

## Nicht verhandelbare Regeln

### Kein Heap — niemals
- **Verboten**: `String`, `new`, `malloc`, `DynamicJsonDocument`, `.c_str()` auf temporärem `String`
- **Erlaubt**: `char[]` auf Stack, `StaticJsonDocument`, `snprintf`, `strlcpy`, `memcpy`
- Ausnahme: einmaliger `WiFi.SSID()`-Aufruf beim Boot tolerierbar

### Kein HTTPClient
- Alle TCP-Kommunikation über `WiFiClient` direkt — Stack-Buffer, kein Heap-Risiko

### JSON Parsing
- Flache JSON-Objekte: eigene `parseJSON()`-Funktion mit `strstr` + `char[]`
- Komplexes JSON: `StaticJsonDocument` (ArduinoJson) — `DynamicJsonDocument` verboten
- `extractValue()` in `DigestAuth.cpp` bleibt für HTTP-Header (`key="value"` und `key=value`)
- Escape-Handling bei JSON zwingend: `\"`, `\\`, `\n`, `\r`, `\t`, `\uXXXX`

### Serial-Präfixe
- `[WARN]` — unerwarteter Zustand, Betrieb weiter möglich
- `[ERROR]` — Fehler, einzelne Funktion nicht verfügbar

---

## Architektur

### Module
- **`DigestAuth`**: generisch, nur SHA-256 Digest Auth GET — keine Shelly-Logik, keine MAC-Logik
- **`WiFiManager`**: alle TCP-Aufrufe gebündelt hier — nirgendwo sonst direktes TCP
- **`DataHub`**: einzige Datenschnittstelle — Getter/Setter, kein direkter Zugriff von außen
- **`JsonParser`**: Eingangs-/Mapping-Layer, erzeugt DTOs (`RelayUpdate`) für DataHub
- **`Watchdog`**: Laufzeitüberwachung, kein Eingriff in Schaltlogik — prüft inhaltliche Anomalien auf DataHub-Werten (z.B. Strom fließt obwohl `output == false`); zählt Parse-Fehler pro Relay und warnt ab Schwelle; Reaktionsstufen: `[WARN]` → Exception-Log (NVS/SD) → später Notification; kein automatischer Neustart

### DataHub-Regeln
- Schreiben nur über API (`updateWifiStatus()`, `applyRelayUpdate()` etc.)
- Getter kopiert Daten — keine internen Pointer zurückgeben
- Lock im DataHub (`xSemaphoreTake/xSemaphoreGive`) — nie außerhalb
- Direkter Zugriff auf `dataHub.wifiRelay[]` ist verboten

### Schaltvorgang
`getStatus()` → `switchShelly()` → `getStatus()` (Verifikation)

### FreeRTOS
- Aktuell: sequenziell und blockierend implementieren
- `delay()` in TCP-Schleifen erst ersetzen wenn WiFiWorker-Thread eingeführt wird
- Umbau-Trigger: Remote-Sensoren — blockierende HTTP-Requests an externe ESP32-Geräte machen einen eigenen WiFiWorker-Task erforderlich, bevor Remote-Sensoren implementiert werden
- `delay(1)` hält den gesamten Core an — nach Umbau: `vTaskDelay(1 / portTICK_PERIOD_MS)`

### WiFiWorker (geplant)
- Ein dedizierter FreeRTOS-Task mit zwei Queues
  - High-Priority: `SWITCH_RELAY`, `GET_STATUS`
  - Low-Priority: `NTP_SYNC`, `LOG_EVENT`, `NOTIFY`
- Alle Netzwerkoperationen laufen sequenziell im Worker — kein direktes TCP außerhalb
- Umbau ist chirurgisch möglich solange alle TCP-Calls im `WiFiManager` gebündelt bleiben

---

## Hardware (MainUnit v1)

| Komponente | Interface | Pins |
|---|---|---|
| TFT Display 3,2" (ILI9341) | SPI | MOSI=23, SCLK=18, MISO=19, CS=5, DC=16, RST=17 |
| Touch (XPT2046) | SPI | CS=4 |
| SD-Karte | SPI | CS=32 |
| DS3231 RTC | I²C | SDA=21, SCL=22 |
| DHT22 #0–#3 | Single-Bus | GPIO 25, 26, 27, 33 |
| Backlight PWM | — | GPIO 15 → S8050 (NPN, TO-92) |

Display-Bibliothek: `TFT_eSPI` (aktuell) — Wechsel zu LVGL nach Flash-Budget-Messung.

---

## Roadmap und Architekturentscheidungen

### Implementierungsreihenfolge
1. **DHT22** — lokal, synchron, kein Threading
2. **WiFiWorker** — asynchroner WiFi-Task mit Request-Queue; Shelly-Polling wandert rein
3. **Remote-Sensoren** — generische Sensorwerte von externen ESP32-Geräten via WiFiWorker-Queue
4. **RTC (DS3231)** — eigener Struct, getrennt von SensorData
5. **Schaltlogik** — ein Messwert pro Schaltentscheidung; zunächst hardcoded in `setup()`
6. **JSON Config Layer** — Serialisierung/Deserialisierung aller Settings; zunächst hardcoded, dann dateibasiert
7. **SD-Karte** — liest und schreibt Config als JSON; primärer Config-Eingabeweg
8. **Display** *(optional — abhängig vom Flash-Budget; Fallback: SPI Card Reader oder USB-CDC)*
9. **NVS** — speichert letzte valide Config über Reboot; Umfang abhängig davon ob Display implementiert wurde

### Sensor-Architektur
- `SensorEntry` ist generisch — ein Eintrag, ein Messwert (`float`), egal ob DHT22, Wägezelle oder Lichtsensor
- Registrierung analog zu Shellys: API-Funktion, zunächst in `setup()` aufgerufen, später von SD/GUI
- Lokale Sensoren schreiben direkt in DataHub; Remote-Sensoren legen Request in WiFiWorker-Queue
- `valid` und `lastUpdate` in `SensorEntry` sind load-bearing — Schaltlogik muss Staleness prüfen

### Config-Persistenz
- JSON ist das einheitliche Format für SD und NVS
- SD: primärer Import-Weg für neue Setups
- NVS: Fallback bei fehlender/defekter SD — speichert den von SD validierten JSON-String
- NVS-Scope wächst mit Display: mehr System-Settings → mehr NVS-Inhalt

---

## Spätere Versionen (kein festgelegter Zeitplan)

### Server-Kommunikation
- TLS via `WiFiClientSecure`: Let's Encrypt + ISRG Root X1 im ESP32 eingebettet
- ESP prüft nur Root-CA → Leaf-Erneuerung durch Certbot transparent
- Authentifizierung: API Key pro Gerät, NVS-gespeichert, über TLS übertragen
- **⚠ ISRG Root X1 läuft ab: 30. September 2035 — OTA-Update auf Root X2 einplanen**

### OTA
- HTTP OTA vom Synology NAS — Voraussetzung für Remote-Bugfixes und Zertifikatswechsel

### ESP als primärer AP für Shellys
- ESP spannt eigenes WLAN auf (`ESP32_TerraControl_<ID>`), Shellys verbinden sich direkt
- STA-Verbindung (Heimnetz) nur für Logging, NTP, OTA — Router-Ausfall unterbricht Kernfunktion nicht

### Shelly-Onboarding direkt vom ESP
- ESP verbindet sich mit Shelly-Werks-AP → konfiguriert per `WiFi.SetConfig` / `Sys.SetConfig` RPC
- Nur Werkseinstellungs-Shellys bindbar

### Datenbank
- InfluxDB für Zeitreihendaten, CouchDB/SQLite+REST für Konfiguration
- Logging dann als InfluxDB Line Protocol statt JSON

### Fernzugriff
- Empfohlen: WireGuard VPN auf Synology NAS

## Nice to have

- **Tasmota**: einfaches `GET /cm?cmnd=Power%20On` — eigene `tasmotaGET()`, unabhängig von DigestAuth
- **Shelly Gen1**: MD5-Digest statt SHA-256 — kleiner Umbau in `digestAuthGET()` bei Bedarf

---

## Testing

**Unit Tests**: Google Test, `[env:native]` in `platformio.ini`

Zu testende Funktionen:
- `extractValue` — Quotes, fehlender Key, Puffergrenze
- `normalizeMac` — Groß/Kleinbuchstaben, Sonderzeichen
- `parseJson` — valides JSON, fehlendes `sys.mac`, fehlendes `wifi.sta_ip`, `aenergy` kein Objekt
- `HttpUtils::readLine` — `\r\n`, Buffer voll, leere Zeile

**Mocking**: `WiFiClient` als `FakeStream` (ist ein `Stream`)  
**Nicht mockbar**: FreeRTOS-Mutex, WiFi-Connect, mDNS

**Integrationstests** (manuell, vor jedem Merge auf `main`):
- Alle 3 Shellys per mDNS gefunden
- MAC-Verifikation schlägt fehl bei unbekannter MAC
- Schalten funktioniert (on/off)
- Relay geht nach Timeout auf offline, wird durch `findStoredRelays` wiederhergestellt
- Kein Absturz nach >5 Minuten Laufzeit
