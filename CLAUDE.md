# TerraControl — Projektkontext für Claude

## Was ist das hier?

ESP32-Firmware zur Steuerung von Shelly Gen2/3-Relays über SHA-256 Digest Auth und mDNS-Discovery. Sensoren (DHT22, DS3231) steuern in der nächsten Version das Schalten zeitgesteuert.

Kein Merge mit dem Prototyp (`esp32/terrasteuerung`) — Neuentwicklung.

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
- **`Watchdog`**: Laufzeitüberwachung, kein Eingriff in Schaltlogik

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
- Umbau-Trigger: Display-Task blockiert spürbar durch WiFi-Calls

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
