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
- **`SensorManager`**: Sensor-Orchestrator — iteriert über alle registrierten `SensorEntry`s; lokale Sensoren (DHT22, SEN0308) direkt lesen und in DataHub schreiben; Remote-Sensoren per Request in WiFiWorker-Queue anstoßen; einzige Komponente die weiß wie ein Sensor angebunden ist

### DataHub-Regeln
- Schreiben nur über API (`updateWifiStatus()`, `applyRelayUpdate()` etc.)
- Getter kopiert Daten — keine internen Pointer zurückgeben
- Lock im DataHub (`xSemaphoreTake/xSemaphoreGive`) — nie außerhalb
- Direkter Zugriff auf `dataHub.wifiRelay[]` ist verboten

### Schaltvorgang
`getStatus()` → `switchShelly()` → `getStatus()` (Verifikation)

### Relay-Zykluslogik
- **RelayMode** (Enum in `WifiRelay`): `AUTO` | `MANUAL_ON` | `MANUAL_OFF` | `FAULT`
  - `AUTO`: Schaltlogik entscheidet anhand Sensorwerten
  - `MANUAL_ON` / `MANUAL_OFF`: überschreibt Schaltlogik; Update-Funktion ignoriert Sensorwerte für dieses Relay
  - `FAULT`: Schaltlogik überspringt dieses Relay; Watchdog meldet Critical Warning; User muss manuell quittieren
- **Relay ON** (aktiv gehalten): `SWITCH_ON` + inline `getStatus` werden alle halbe Intervallzeit wiederholt — korrigiert unerwartetes Abfallen automatisch
- **Relay OFF**: `SWITCH_OFF` + inline `getStatus`; wenn bestätigt → nur noch periodisches `GET_STATUS`; wenn nicht bestätigt → 2 Wiederholungen (~3s Pause) → bei anhaltendem Mismatch zwischen DataHub-State und Relay-State: `mode = FAULT`

### FreeRTOS
- Aktuell: sequenziell und blockierend implementieren
- `delay()` in TCP-Schleifen erst ersetzen wenn WiFiWorker-Thread eingeführt wird
- Umbau-Trigger: Remote-Sensoren — blockierende HTTP-Requests an externe ESP32-Geräte machen einen eigenen WiFiWorker-Task erforderlich, bevor Remote-Sensoren implementiert werden
- `delay(1)` hält den gesamten Core an — nach Umbau: `vTaskDelay(1 / portTICK_PERIOD_MS)`

### WiFiWorker (geplant)
- Ein dedizierter FreeRTOS-Task
- **Mailbox pro Relay** (`QueueHandle_t`, Länge 1, `xQueueOverwrite`): Befehle `SWITCH_ON`, `SWITCH_OFF`, `GET_STATUS` — neuester Befehl überschreibt vorhandenen
- **STA-Queue**: `NTP_SYNC` (24h-Timer), `EXTERNAL_COMMAND` (event-driven, für spätere externe Steuerung)
- **Update-Funktion** (Producer, läuft im Haupt-Task): prüft DataHub, schreibt bei Bedarf in Relay-Mailboxen
- Priorität durch Abarbeitungsreihenfolge: Relay-Mailboxen → STA-Queue
- Post-Switch-Verifikation (`getStatus`) läuft inline im Worker nach jedem Switch-Befehl — kein separater Queue-Eintrag
- Alle Netzwerkoperationen laufen sequenziell im Worker — kein direktes TCP außerhalb
- Umbau ist chirurgisch möglich solange alle TCP-Calls im `WiFiManager` gebündelt bleiben

### WiFi-Netzwerkarchitektur
- ESP32 betreibt **Soft-AP und STA gleichzeitig** (AP+STA-Dual-Mode)
- **Soft-AP** (`ESP32_TerraControl_<ID>`): alle gesteuerten Geräte (Shellys, C3-Remote-Sensoren) verbinden sich direkt mit dem ESP32 — isoliert vom Heimnetz
- **STA**: Verbindung zum Heimrouter — ausschließlich für NTP-Sync, Logging, spätere externe Commands; bleibt dauerhaft offen
- Kein mDNS: Gerät-Discovery über `IP_EVENT_AP_STAIPASSIGNED` — liefert MAC + zugewiesene IP bei jedem Connect; MAC ist persistenter Identifier, IP wird im DataHub aktuell gehalten
- Channel-Binding: Soft-AP und STA teilen einen Kanal (Hardware-Constraint) — Kanalwechsel des Routers führt zu kurzer Unterbrechung der AP-Clients (Reconnect in wenigen Sekunden); auf 2,4 GHz selten
- Router-Ausfall unterbricht Kernfunktion nicht — Shellys und Sensoren bleiben über ESP32-AP erreichbar
- **SSID ist der persistente Anker** für alle gebundenen Geräte (Shellys, C3-Sensoren) — SSID umbenennen trennt alle Verbindungen und erfordert vollständiges Re-Onboarding; Web-Interface muss das mit einer harten Warnung versehen
- **Mehrere MainUnits** können denselben AP-Subnetzbereich nutzen — ihre APs sind vollständig isolierte L2-Netze mit unterschiedlichen SSIDs; IP-Konflikte zwischen MainUnits sind nicht möglich
- **AP-Subnetz** (optional manuell setzbar): Standard `192.168.4.x`; einziger relevanter Konfliktfall ist Überlappung mit dem Heimnetz-Subnetz des Routers — beim STA-Connect wird das eigene Subnetz mit dem Router-Subnetz verglichen; bei Überlappung Warnung im Web-Interface; wird in NVS gespeichert

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

Display-Bibliothek: `TFT_eSPI` (aktuell) — Wechsel zu LVGL wenn Display als Konfigurationstool ausgebaut wird; Flash-Budget ist der entscheidende Faktor.

---

## Sensoren

Jeder Sensor bekommt einen Wrapper, sodass Sensortypen je nach Terrarientyp ausgetauscht werden können.

| Messgröße | Prototyp | Langfristig / Alternativen |
|---|---|---|
| Temperatur + Luftfeuchte | **DHT22** (Breakout-Modul, Single-Wire) | SHT31 (weatherproof, I2C), SHT40 |
| Temperatur Wasser/Boden | – | DS18B20 (IP68, 1-Wire) |
| CO₂ | – | SCD41 (Sensirion, I2C) |
| Licht | – | BH1750 (Lux, I2C), SI1145 (UV+IR+VIS) |
| Bodenfeuchte | – | SEN0308 (kapazitiv, korrosionsresistent) |
| Füllstand Wassertank | **HX711 + Wägezelle** (Beregnungsanlage) | – |
| Wasserstand Aquarium | – | Wasserdrucksensor (z.B. MS5803 I²C, XGZP6847 analog) — misst Füllstand über Wassersäule |

Sensoren müssen räumlich vom ESP32 getrennt sein — der ESP32 erzeugt Eigenwärme, die nahe platzierte Sensoren verfälscht.

### Remote-Sensoren (ESP32-C3 Super Mini)
Sensoren die räumlich weit vom MainUnit entfernt sind werden an einen **ESP32-C3 Super Mini** angeschlossen, der **außerhalb** des Terrariums sitzt. Sensorkabel werden durch die Terrariumwand geführt (ca. 3mm Bohrung). Der MainUnit fragt den C3 per WiFi ab (Pull/On-Demand); der C3 liegt im Light Sleep zwischen den Abfragen. Kabelgebundene Stromversorgung — kein Akku. Die Steckverbindung zum Sensor folgt dem gleichen Phoenix-Contact-Schema (MSTB/MSTBT, 3-polig). Kabel zwischen C3 und Sensor: 20–30cm.

### Sensorgehäuse
Sensoren werden in individuellen 3D-gedruckten Gehäusen verbaut, die sich optisch in die Inneneinrichtung des Terrariums integrieren (z.B. als Stein, Ast, Fels).

### Validprüfung analoger Sensoren
`analogRead()` gibt immer einen Wert zurück — es gibt kein Read-Error wie bei DHT22. Validprüfung in `AnalogSensor::readValue()`:
- **Einfach**: Rail-Werte `0` und `4095` als ungültig werten (`value > 0 && value < 4095`) — deckt Kurzschluss und offenen Pin ab
- **Margin**: Nur nötig wenn der Sensor den vollen Spannungsbereich ausnutzt (z.B. Poti). Sensoren mit definiertem Arbeitsbereich (SEN0308: ~1200–3500) kommen nie in die Nähe der Rails — `> 0 && < 4095` reicht
- **Kein Schalten bei ungültigem Wert**: `sensor.online` wird durch `SensorManager` auf `false` gesetzt — Schaltlogik prüft `online` vor jedem Schaltvorgang

---

## Steckverbindungen

- Kabel wird fest am Sensor gelötet
- Kabel führt durch ein kleines Loch (~3mm, nur Kabeldurchmesser) im Terrarium
- Steckverbindung sitzt **außen** am ESP32-Gehäuse
- **Phoenix Contact MSTB / MSTBT**, 3-polig (VCC, GND, DATA)
  - Buchse (MSTB) auf Lochraster / PCB, 2,54mm Raster
  - Stecker (MSTBT) am Kabelende: Draht abisolieren und eindrücken, kein Crimpen
- Remote-Knoten (ESP32-C3): gleiches Steckersystem, Kabel 20–30cm

---

## Roadmap und Architekturentscheidungen

### Implementierungsreihenfolge
1. **DHT22** — lokal, synchron, kein Threading
2. **WiFiWorker + Soft-AP** — ESP32 spannt eigenen AP auf; Shellys migrieren auf ESP32-AP; mDNS entfällt; asynchroner WiFi-Task mit Relay-Mailboxen + STA-Queue
3. **Remote-Sensoren** — generische Sensorwerte von externen ESP32-C3-Geräten; zunächst hardcoded Credentials; C3 im Light Sleep, wake-on-TCP; Pull/On-Demand-Modell konsistent mit verkabelten Sensoren
4. **RTC (DS3231)** — eigener Struct, getrennt von SensorData
5. **Schaltlogik** — ein Messwert pro Schaltentscheidung; zunächst hardcoded in `setup()`
6. **JSON Config Layer** — Serialisierung/Deserialisierung aller Settings; zunächst hardcoded, dann dateibasiert
7. **SD-Karte** — liest und schreibt Config als JSON; primärer Config-Eingabeweg
8. **Onboarding-Wizard** — Komfortfunktion für Endnutzer; automatisches Binden neuer Shellys und C3-Sensoren ab Werksreset; liest MAC aus Gerät, konfiguriert WiFi + Auth; setzt SD-Karte voraus (Config-Persistenz)
9. **NVS** — speichert letzte valide Config über Reboot; Umfang abhängig davon ob Display implementiert wurde
10. **Display** *(optional — Flash-Budget entscheidet nach Schritt 7)*
    - On-Device-Konfigurationstool: WLAN-Credentials, MACs und Namen für Shellys/Sensoren, Schaltregeln, Schwellwerte
    - Operator-Interface: Sensorwerte, Relay-Zustände, manuelle Relay-Overrides
    - Benötigt: Mehrfenster-Navigation, Bildschirmtastatur → LVGL
    - LVGL ~300KB Flash — nur auf ESP32-Varianten mit ausreichend Flash
    - **Fallback bei 4MB-Variante**: kein Display, stattdessen Monitoring per REST API — DataHub wird als JSON serialisiert und ausgeliefert; Konfiguration über SD-Karte
    - **Weitere Optionen**: SD-Slot-Modul ohne Display, oder externer NAND-Speicher am ESP32

### Sensor-Architektur
- `SensorEntry` ist generisch — ein Eintrag, ein Messwert (`float`), egal ob DHT22, Wägezelle oder Lichtsensor
- `lastUpdate = 0` als Default ist bewusst — der erste Read erfolgt erst nach dem ersten abgelaufenen Intervall; gibt lokalen Sensoren Aufwärmzeit und Remote-Sensoren Zeit für WiFi-Verbindungsaufbau nach Stromausfall
- Registrierung analog zu Shellys: API-Funktion, zunächst in `setup()` aufgerufen, später von SD/GUI
- Lokale Sensoren schreiben direkt in DataHub; Remote-Sensoren legen Request in WiFiWorker-Queue
- `valid` und `lastUpdate` in `SensorEntry` sind load-bearing — Schaltlogik muss Staleness prüfen

### Sensor-Aufteilung MainUnit vs. Remote
Entscheidungskriterium: Sensoren ohne Library-Overhead bleiben am MainUnit, alle anderen wandern auf ESP32-C3-Knoten — spart Flash für LVGL.

**MainUnit (verdrahtet):**
- DHT22 — Single-Wire, bereits auf Platine
- DS3231 RTC — I²C, bereits auf Platine
- SEN0308 (Bodenfeuchte) — reiner ADC-Read, keine Library

**Remote (ESP32-C3):**
- HX711 + Wägezelle — eigenes serielles Protokoll, Library nötig
- DS18B20 — 1-Wire, Dallas-Library
- SCD41 — I²C, Sensirion-Library
- BH1750 / SI1145 — I²C, Library nötig (BH1750 klein ~2KB, Grenzfall)

### Onboarding-Wizard
- Gilt für Shellys und C3-Remote-Sensoren — ein gemeinsamer Mechanismus, gerätespezifisch nur der letzte Konfigurationsschritt
- **Ablauf**: ESP32-STA verbindet temporär mit dem Factory-AP des Geräts (bekannter IP-Bereich, dokumentiert) → liest MAC automatisch via RPC → konfiguriert WiFi + Auth → Gerät verbindet sich mit ESP32-AP → MAC wird in Config gespeichert
- **Shelly-Auth**: ein Account (`terracontrol`) mit generiertem Passwort, das nie angezeigt wird — MainUnit ist einziger Zugangspunkt
- User hat keinen direkten Zugriff auf die Shelly — alle Konfiguration (inkl. LED) läuft ausschließlich über TerraControl
- Shelly aus dem Verbund nehmen: Factory Reset am Gerät; TerraControl erkennt die MAC nicht mehr und entfernt den Eintrag
- **Shelly-Härtung** (automatisch beim Onboarding): Cloud, BLE und MQTT werden deaktiviert (`Cloud.SetConfig`, `BLE.SetConfig`, `Mqtt.SetConfig`)
- **LED-Konfiguration**: Farbe + Helligkeit für AN/AUS-Zustand, Nachtmodus (Zeitfenster + Helligkeit); Locate-Funktion (temporärer Blink per RPC zur physischen Identifikation)
- **RPC-Proxy** für erweiterte Shelly-Features: `shellyRPC(mac, method, params, response, len)` — TerraControl übernimmt Auth und Routing, Caller liefert Methode und Parameter als Strings
- **JSON an die Shelly ist modellunabhängig** — gleicher RPC für Plug S und Mehrfachdosen; Unterschied nur in der Anzahl der WifiRelay-Slots (ein Slot pro Switch-Komponente, gleiche MAC, unterschiedliche `id`)
- C3-Onboarding: analoger Ablauf über C3-eigenen Provisioning-AP; übergibt AP-Credentials und Sensor-Konfiguration (Typ, ID, Intervall)

### Remote-Sensor-Protokoll
- Remote-Knoten (ESP32-C3) werden analog zu Shellys über ihre **MAC-Adresse** identifiziert; IP wird dynamisch via `IP_EVENT_AP_STAIPASSIGNED` aktuell gehalten
- **Pull/On-Demand-Modell**: MainUnit fragt den C3 ab — konsistent mit verkabelten Sensoren und Shellys; kein Push
- Antwortformat pro Poll: `[ { id, value }, ... ]`
- C3 betreibt **Light Sleep** zwischen Abfragen: WiFi-Assoziation bleibt aktiv, eingehende TCP-Verbindung weckt den C3 sofort
- C3 sitzt **außerhalb** des Terrariums; Sensorkabel wird durch die Terrariumwand geführt; kabelgebundene Stromversorgung
- Der MainUnit fragt den Knoten ab; der Knoten trifft keine Schaltentscheidungen

### Schaltlogik
- Generisch — kennt keinen Sensortyp, nur einen `float`-Wert aus dem DataHub
- Pro Schaltentscheidung ein Messwert, ein Relay
- Parameter pro Regel: `sensorId`, `threshold`, `hysteresis`, `activeAbove` (bool)
- `threshold` ist immer der sicherheitskritische Schaltpunkt — `hysteresis` verschiebt nur die unkritische Gegenseite
- `activeAbove = true`: Relay ON wenn `value > threshold`, OFF wenn `value < threshold - hysteresis`
- `activeAbove = false`: Relay ON wenn `value < threshold`, OFF wenn `value > threshold + hysteresis`
- **Sicherheitsregel: Schaltlogik prüft `sensor.online` bevor sie schaltet — kein Schalten bei ungültigem Sensorwert.** Hintergrund: Ein ausgefallener Bodenfeuchtesensor darf nicht dauerhaft die Beregnungsanlage aktivieren. Relay bleibt im letzten validen Zustand bis `online` wieder `true`.

### Config-Persistenz
- **NVS ist der primäre Config-Store** — einzige Wahrheitsquelle für alle Settings
- **SD-Karte**: ausschließlich Logging — keine Config-Datenhaltung, keine Synchronisation
- **Config-Update**: JSON POST via WiFi-Endpoint → DataHub → NVS; kein physischer Zugriff nötig
- **Config-Export**: JSON GET via WiFi-Endpoint — Download als Backup; Wiederherstellung per POST
- **Initialer Bootstrap**: `initDefaults()` beim ersten Boot wenn NVS leer — schreibt hardcodete Startwerte
- NVS-Scope wächst mit den Features (WiFi-Credentials, Relay-Config, Sensor-Config, AP-Subnetz ...)

### Factory Reset
- **MainUnit**: löscht NVS vollständig → alle Config weg (WiFi-Credentials, gebundene Geräte, Schaltregeln) → startet mit `initDefaults()` neu → alle Shellys und C3-Sensoren müssen neu ongeboardet werden, da Auth-Credentials gelöscht sind
- **Shelly**: Factory Reset am Gerät (Hardware-Taste) → geht in Werkszustand, öffnet eigenen AP → bereit für neues Onboarding
- **C3-Remote-Sensor**: Factory Reset (Hardware-Taste oder Firmware-Flag) → startet Provisioning-AP → bereit für neues Onboarding
- Web-Interface zeigt harte Warnung vor MainUnit-Reset: alle gebundenen Geräte müssen neu eingerichtet werden

---

## Spätere Versionen (kein festgelegter Zeitplan)

### Server-Kommunikation
- TLS via `WiFiClientSecure`: Let's Encrypt + ISRG Root X1 im ESP32 eingebettet
- ESP prüft nur Root-CA → Leaf-Erneuerung durch Certbot transparent
- Authentifizierung: API Key pro Gerät, NVS-gespeichert, über TLS übertragen
- **⚠ ISRG Root X1 läuft ab: 30. September 2035 — OTA-Update auf Root X2 einplanen**

### OTA
- HTTP OTA vom Synology NAS — Voraussetzung für Remote-Bugfixes und Zertifikatswechsel

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
