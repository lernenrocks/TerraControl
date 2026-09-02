# TerraControl — Projektkontext für Claude

## Was ist das hier?

ESP32-Firmware zur Steuerung von Shelly Gen2/3-Relays über SHA-256 Digest Auth. Shellys verbinden sich direkt mit dem ESP32-Soft-AP — kein mDNS. Sensoren steuern zeitgesteuert und schwellwertbasiert das Schalten — ein Messwert pro Schaltentscheidung, keine Kombinationslogik.

Kein Merge mit dem Prototyp (`esp32/terrasteuerung`) — Neuentwicklung.

---

## Zusammenarbeit

- Moderne C++-Ansätze bevorzugen und erklären (z.B. anonymer Namespace statt `static` auf Dateiebene, `nullptr` statt `NULL`)
- Der User lernt aktiv — neue Konzepte kurz erklären wenn sie eingesetzt werden
- **Handle-Architektur (FreeRTOS-Queues, Tasks, Semaphoren) ist für den User schwer greifbar** — jeden Schritt erklären warum er so aussieht, nicht nur wie er aussieht; Blackbox-Vertrauen kostet Überblick und führt langfristig zu Frustration
- **Kein mehrfacher Umbau** — Workarounds die beim nächsten Sprint ersetzt werden müssen aktiv vermeiden; saubere Lösung sofort, auch wenn der Sprint dadurch größer wird
- **Überblick hat Priorität** — wenn der User den roten Faden verliert ohne es zu merken, holt ihn das später ein; lieber früh warnen wenn ein Schritt einen späteren Umbau erzwingt

---

## Nicht verhandelbare Regeln

### Heap — nur außerhalb heißer Pfade
- **Verboten in wiederkehrenden Abläufen** (RequestWorker-Zyklus, Controller-Tick, jede periodisch laufende Schleife): `String`, `new`, `malloc`, `DynamicJsonDocument`, `.c_str()` auf temporärem `String`
- **Erlaubt bei seltenen, User-getriggerten Aktionen** (Onboarding eines Sensors/Relays, Anlegen/Löschen einer Schaltregel): `new`/`delete` — kein wiederholtes Alloc/Free-Muster über die Laufzeit, da diese Aktionen selten und unregelmäßig auftreten, nicht in einer Schleife
  - `new` immer auf Fehlschlag prüfen (`new(std::nothrow)` + Nullpointer-Check) — anders als ein statisches Array kann eine Heap-Allokation fehlschlagen
- **Immer erlaubt**: `char[]` auf Stack, `StaticJsonDocument`, `snprintf`, `strlcpy`, `memcpy`
- Ausnahme: einmaliger `WiFi.SSID()`-Aufruf beim Boot tolerierbar
- Begründung: Fragmentierung entsteht durch wiederholte Alloc/Free-Zyklen unterschiedlicher Größe über Monate Laufzeit — seltene, unregelmäßige Allokationen (Onboarding, Schaltregeln) erzeugen dieses Muster nicht in relevantem Umfang; in jedem periodisch laufenden Codepfad bleibt Heap tabu (2026-08-31, siehe `learnings`-Diskussion zum SensorManager)

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
- **`HttpClient`**: generischer HTTP-Client — `get()` für authentifizierte und nicht-authentifizierte GET-Requests; konsumiert Response-Header intern; Stream liegt am Body bei Rückgabe; enthält `readLine` und `skipHeader`
- **`DigestAuth`**: SHA-256 Digest Auth — baut auf `HttpClient` auf; erster Request und WWW-Authenticate-Parsing selbst; zweiter Request über `HttpClient::get()` mit Authorization-Header; keine Shelly-Logik, keine MAC-Logik
- **`NetworkUtils`**: Konvertierungen für MAC und IP — `normalizeMac`, `macBytesToChar`, `ipToChar`; kein HTTP-Bezug; löst `HttpUtils` ab
- **`WiFiManager`**: alle TCP-Aufrufe gebündelt hier — nirgendwo sonst direktes TCP
- **`RequestWorker`** (Namen vorläufig, Nachfolger von `WiFiWorker`): dedizierter FreeRTOS-Task auf Core 0 — verwaltet Relay-Mailboxen und Remote-Sensor-Mailboxen (gleiche Struktur wie Relay-Mailboxen), periodischen AP-Sync und STA-Queue; einzige Stelle die `WiFiManager`-Funktionen aufruft; reine Client-Rolle (pollt Shellys und Remote-Sensoren)
- **`GuiWorker`** (Namen vorläufig, neu): dedizierter FreeRTOS-Task auf Core 0 — HTTP-Server für Konfiguration, Monitoring, manuelle Relay-Overrides, manuelle Zeiteinstellung (Standalone-Fallback ohne NTP) und Log-Download aus dem NOR-Flash; horcht auf AP- **und** STA-Interface gleichzeitig, damit die MainUnit auch ganz ohne Heimrouter bedienbar bleibt; eigene SHA-256 Digest Auth in Server-Rolle (Referenzimplementierung bereits im RemoteSensor-Projekt vorhanden, siehe `learnings/mainunit-transfer-notes.md`); reine Server-Rolle, kein Zugriff auf `WiFiManager`
- **`Controller`**: Schaltlogik-Orchestrator — läuft im Haupt-Task; prüft Relay-Zustände, schreibt Switch-Befehle in RequestWorker-Mailboxen; kennt keine Netzwerkdetails
- **`DataHub`**: einzige Datenschnittstelle — Getter/Setter, kein direkter Zugriff von außen
- **`JsonParser`**: Eingangs-/Mapping-Layer, erzeugt DTOs (`RelayUpdate`) für DataHub
- **`Watchdog`**: Laufzeitüberwachung, kein Eingriff in Schaltlogik — prüft inhaltliche Anomalien auf DataHub-Werten (z.B. Strom fließt obwohl `output == false`); zählt Parse-Fehler pro Relay und warnt ab Schwelle; Reaktionsstufen: `[WARN]` → Exception-Log (NVS/NOR-Flash) → später Notification; kein automatischer Neustart
- **`SensorManager`**: Sensor-Orchestrator — iteriert über alle registrierten `SensorEntry`s; lokale Sensoren (DHT22, SEN0308) direkt lesen und in DataHub schreiben; Remote-Sensoren per Request in RequestWorker-Mailbox anstoßen; einzige Komponente die weiß wie ein Sensor angebunden ist

### DataHub-Regeln
- Schreiben nur über API (`updateWifiStatus()`, `applyRelayUpdate()` etc.)
- Getter kopiert Daten — keine internen Pointer zurückgeben
- Lock im DataHub (`xSemaphoreTake/xSemaphoreGive`) — nie außerhalb
- Direkter Zugriff auf `dataHub.wifiRelay[]` ist verboten

### Schaltvorgang
`WiFiManager::switchRelay()` → RequestWorker queued `GET_STATUS` in Relay-Mailbox nach erfolgreichem Switch

### Relay-Zykluslogik
- **RelayMode** (Enum in `WifiRelay`): `AUTO` | `FORCED_ON` | `FORCED_OFF`
  - `AUTO`: Schaltlogik entscheidet anhand Sensorwerten
  - `FORCED_ON` / `FORCED_OFF`: überschreibt Schaltlogik; Controller ignoriert Sensorwerte für dieses Relay
- **`bool broken`** (Flag in `WifiRelay`, kommt im Watchdog-Sprint): gesetzt vom Watchdog bei anhaltendem Mismatch; Controller überspringt das Relay und loggt `[WARN]`; orthogonal zum RelayMode — der vorherige Mode bleibt erhalten; User muss manuell quittieren (GUI/API)
- **Relay ON** (aktiv gehalten): `SWITCH_ON` + inline `getStatus` werden alle halbe Intervallzeit wiederholt — korrigiert unerwartetes Abfallen automatisch
- **Relay OFF**: `SWITCH_OFF` + inline `getStatus`; wenn bestätigt → nur noch periodisches `GET_STATUS`; wenn nicht bestätigt → 2 Wiederholungen (~3s Pause) → bei anhaltendem Mismatch zwischen DataHub-State und Relay-State: `relay.broken = true`

### FreeRTOS
- **RequestWorker** und **GuiWorker** laufen beide auf **Core 0** — alle Netzwerk-Tasks gebündelt auf einem Core (Modultrennung nach Abhängigkeit: beide brauchen den WiFi-Stack), Core 1 bleibt exklusiv für Controller/SensorManager
- Arduino loop() läuft auf Core 1 — Controller und SensorManager laufen dort
- **Akzeptierter Tradeoff**: Die Schaltentscheidung läuft auf Core 1, ihre Ausführung (RequestWorker-Mailbox → TCP) auf Core 0 — durch die Mailbox-Architektur ohnehin entkoppelt, bewusst hingenommen statt einen gemeinsamen Core zu erzwingen
- GuiWorker bekommt eine niedrigere Priorität als RequestWorker — ein langer Log-Download (bis zu 16 MB aus dem NOR-Flash) darf die Relay-Steuerung nicht verzögern; der FreeRTOS-Scheduler erzwingt den Vorrang automatisch
- `delay()` in TCP-Schleifen → `taskYIELD()` (gibt CPU ab ohne feste Wartezeit)
- `vTaskDelay(pdMS_TO_TICKS(50))` am Ende jedes Worker-Zyklus — gibt anderen Tasks CPU-Zeit

### RequestWorker (Namen vorläufig, Nachfolger von WiFiWorker)
- Dedizierter FreeRTOS-Task, gestartet am Ende von `initWiFi()`; keine WiFi-Event-Handler
- **`WIFI_DEVICE_MAX`**: eine gemeinsame Obergrenze für Relays + Remote-SensorNodes zusammen (nicht getrennt pro Typ), weil beide sich denselben SoftAP-`max_connection`-Topf teilen (ESP32: Default 4, bis 10 konfigurierbar) — `wifiDeviceCount` zählt beide zusammen, auch für die GUI-Anzeige ("X von 10 Geräten gebunden"). Gilt nicht für lokal verkabelte Sensoren (DHT22, SoilMoisture) — die sind keine AP-Clients
- **STA-Reconnect-Backoff (30s)**: nach einem STA-Disconnect (z.B. Heimrouter weg) wartet der Worker 30s vor dem nächsten `esp_wifi_connect()`-Versuch, statt sofort zu reconnecten — verhindert, dass wiederholte Reconnect-Versuche den LWIP-Stack so auslasten, dass AP-seitige Verbindungen (Shellys, SensorNodes) darunter leiden (dokumentiertes ESP32-Problem, 2026-08-31 recherchiert)
- **`syncApClients()`**: liest via `esp_wifi_ap_get_sta_list()` + `esp_netif_get_sta_list()` alle verbundenen AP-Clients; vergleicht MAC mit DataHub; aktualisiert IP wenn geändert; setzt `online = false` wenn MAC verschwunden — kein Queuing von `GET_STATUS`
- **Periodischer AP-Sync**: `syncApClients()` läuft alle `relayStatusInterval_ms` im Worker-Zyklus — kein Event-Trigger
- **`online`-Regeln**: `online = true` kommt ausschließlich vom JsonParser nach erfolgreichem GET_STATUS (HTTP 200); `online = false` bei GET_STATUS result == -1 (TCP-Fehler) oder wenn MAC in `syncApClients` nicht mehr sichtbar
- **`GET_STATUS_BACKOFF_MS = 5000`**: nach fehlgeschlagenem GET_STATUS (result ≠ 200) pausiert der Worker 5s vor dem nächsten Versuch für dieses Relay
- **Mailbox pro Relay** (`QueueHandle_t`, Länge 1, `xQueueOverwrite`): Befehle `SWITCH_ON`, `SWITCH_OFF`, `GET_STATUS` — neuester Befehl überschreibt vorhandenen; SWITCH nur ausgeführt wenn `online = true`; GET_STATUS auch bei `online = false` (Reconnect-Erkennung)
- **Mailbox pro Remote-Sensor** (gleiche Struktur wie Relay-Mailboxen, eigenes Postfach pro C3-Knoten) — ersetzt die frühere Idee, Remote-Sensoren über die STA-Queue abzuwickeln; C3-Knoten hängen am AP wie die Shellys, nicht am STA
- **STA-Queue** (`QueueHandle_t`, Länge 8, `xQueueSend`): `NTP_SYNC`, `EXTERNAL_COMMAND` — für spätere Erweiterungen
- Abarbeitungsreihenfolge: periodischer AP-Sync → Relay-Mailboxen → Remote-Sensor-Mailboxen → STA-Queue → `vTaskDelay(50ms)`
- Post-Switch-Verifikation: Worker queued `GET_STATUS` in Relay-Mailbox nach erfolgreichem `switchRelay()`
- Alle Netzwerkoperationen laufen sequenziell im Worker — kein direktes TCP außerhalb

### GuiWorker (Namen vorläufig, neu — ersetzt die frühere Display-Planung)
- Dedizierter FreeRTOS-Task auf Core 0, eigener HTTP-Server; horcht auf AP- und STA-Interface gleichzeitig (`0.0.0.0`-Bindung) — Standalone-Betrieb ohne Heimrouter bleibt dadurch möglich, inklusive manueller Zeiteinstellung als NTP-Fallback
- Zuständig für: Konfiguration (WLAN, Shelly/Sensor-Bindung, Schaltregeln), Monitoring (Sensorwerte, Relay-Zustände), manuelle Relay-Overrides, Log-Download aus dem NOR-Flash
- Eigene SHA-256 Digest Auth in Server-Rolle — fertige Referenzimplementierung aus dem RemoteSensor-Projekt übertragbar (siehe `learnings/mainunit-transfer-notes.md`, Abschnitt A)
- Konventionen aus dem RemoteSensor-Review direkt anwendbar, da hier zum ersten Mal ein eigener HTTP-Server auf der MainUnit entsteht: exaktes Routing (`strcmp` auf geparstem Pfad, kein `strstr`-Präfixmatch), ein benannter Handler pro Route, datengetriebenes Rendering im Frontend (siehe `learnings/stille-bugs.md` #03, `learnings/mainunit-transfer-notes.md` Abschnitt B)
- Kein direkter Zugriff auf `WiFiManager` oder die Relay-Mailboxen — Overrides laufen über den DataHub, den der Controller ohnehin ausliest

### WiFi-Netzwerkarchitektur
- ESP32 betreibt **Soft-AP und STA gleichzeitig** (AP+STA-Dual-Mode)
- **Soft-AP** (`ESP32_TerraControl_<ID>`): alle gesteuerten Geräte (Shellys, C3-Remote-Sensoren) verbinden sich direkt mit dem ESP32 — isoliert vom Heimnetz
- **STA**: Verbindung zum Heimrouter — für NTP-Sync, Logging, spätere externe Commands sowie GUI-Zugriff aus dem Heimnetz; bleibt dauerhaft offen
- **GuiWorker horcht auf AP und STA gleichzeitig** — die Web-GUI ist die einzige Bedienoberfläche (kein Display), muss also auch ohne Heimrouter erreichbar sein (Standalone-Betrieb); Zeit wird in diesem Fall manuell über die GUI gesetzt statt per NTP
- Kein mDNS: Gerät-Discovery über periodisches `syncApClients()`-Polling im RequestWorker — liest MAC + IP aller verbundenen AP-Clients via `esp_netif_get_sta_list`; MAC ist persistenter Identifier, IP wird im DataHub aktuell gehalten; keine WiFi-Event-Handler
- Channel-Binding: Soft-AP und STA teilen einen Kanal (Hardware-Constraint) — Kanalwechsel des Routers führt zu kurzer Unterbrechung der AP-Clients (Reconnect in wenigen Sekunden); auf 2,4 GHz selten
- Router-Ausfall unterbricht Kernfunktion nicht — Shellys und Sensoren bleiben über ESP32-AP erreichbar
- **SSID ist der persistente Anker** für alle gebundenen Geräte (Shellys, C3-Sensoren) — SSID umbenennen trennt alle Verbindungen und erfordert vollständiges Re-Onboarding; Web-Interface muss das mit einer harten Warnung versehen
- **Mehrere MainUnits** können denselben AP-Subnetzbereich nutzen — ihre APs sind vollständig isolierte L2-Netze mit unterschiedlichen SSIDs; IP-Konflikte zwischen MainUnits sind nicht möglich
- **AP-Subnetz** (optional manuell setzbar): Standard `192.168.4.x`; einziger relevanter Konfliktfall ist Überlappung mit dem Heimnetz-Subnetz des Routers — beim STA-Connect wird das eigene Subnetz mit dem Router-Subnetz verglichen; bei Überlappung Warnung im Web-Interface; wird in NVS gespeichert

---

## Hardware (MainUnit v1)

**Headless-Entscheidung (2026-08-26):** Kein Display, kein Touch, keine SD-Karte — Display und SD-Slot wurden physisch vom Gerät entfernt. Die im RemoteSensor-Projekt entstandene Web-GUI hat sich als flexibel genug erwiesen, um die alleinige Bedienoberfläche zu sein; spart zusätzlich das LVGL-Flash-Budget und die entsprechende Verkabelung. Begründung und Details: `learnings/mainunit-transfer-notes.md`.

| Komponente | Interface | Pins |
|---|---|---|
| DS3231 RTC | I²C | SDA=21, SCL=22 |
| DHT22 #0–#3 | Single-Bus | GPIO 25, 26, 27, 33 |
| NOR-Flash (mehrere W25Q-Chips, je 128 Mbit / 16 MB) | SPI, eigene CS-Leitung pro Chip | MOSI/MISO/SCK geteilt |

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
1. ✅ **DHT22** — lokal, synchron, kein Threading
2. ✅ **WiFiWorker + Soft-AP** — ESP32 spannt eigenen AP auf; Shellys verbinden sich direkt; mDNS entfällt; WiFiWorker-Task mit Relay-Mailboxen + STA-Queue; Controller-Modul für Schaltlogik (in Arbeit; wird durch Schritt 3a in RequestWorker + GuiWorker aufgeteilt)
3. **Remote-Sensoren** — generische Sensorwerte von externen ESP32-C3-Geräten; zunächst hardcoded Credentials; C3 im Light Sleep, wake-on-TCP; Pull/On-Demand-Modell konsistent mit verkabelten Sensoren; Polling läuft über eine eigene Mailbox pro Remote-Sensor im RequestWorker (gleiche Struktur wie Relay-Mailboxen, nicht die STA-Queue)
3a. **RequestWorker / GuiWorker-Split** — WiFiWorker wird in zwei Tasks aufgeteilt: RequestWorker (Client-Rolle, Shelly- + Remote-Sensor-Polling) und GuiWorker (Server-Rolle, eigener HTTP-Server für die Web-GUI); beide auf Core 0, GuiWorker mit niedrigerer Priorität; siehe Abschnitte RequestWorker/GuiWorker und FreeRTOS oben
4. **RTC (DS3231)** — eigener Struct, getrennt von SensorData; GUI bietet zusätzlich manuelle Zeiteinstellung als Fallback für Standalone-Betrieb ohne NTP
5. **Schaltlogik** — ein Messwert pro Schaltentscheidung; zunächst hardcoded in `setup()`
6. **JSON Config Layer** — Serialisierung/Deserialisierung aller Settings; zunächst hardcoded, dann dateibasiert
7. **NOR-Flash Logging** — externer SPI-NOR (W25Q-Familie, mehrere Chips à 128 Mbit/16 MB mit eigener Chip-Select-Leitung statt einem großen adressierten Chip); monatliche Rotation, FIFO-Retention (ältester Monat weicht bei Platzmangel), GUI-Download + manuelles Löschen für gezieltes Archivieren; ersetzt die ursprünglich geplante SD-Karte vollständig (Begründung: `learnings/mainunit-transfer-notes.md`)
8. **Onboarding-Wizard** — Komfortfunktion für Endnutzer; automatisches Binden neuer Shellys und C3-Sensoren ab Werksreset; liest MAC aus Gerät, konfiguriert WiFi + Auth; setzt den NVS-Konfigurationsspeicher voraus (Config-Persistenz)
9. **NVS** — speichert letzte valide Config über Reboot
10. **GUI-Ausbau** — Konfiguration (WLAN-Credentials, MACs und Namen für Shellys/Sensoren, Schaltregeln, Schwellwerte) und Operator-Interface (Sensorwerte, Relay-Zustände, manuelle Relay-Overrides) über den GuiWorker; ersetzt die ursprünglich geplante Display/LVGL-Lösung vollständig (kein Display mehr verbaut, siehe Hardware-Abschnitt)
11. **Eigenverbrauch** — INA219 (I2C) misst Spannung, Strom und Leistung von MainUnit und C3-Nodes; Werte landen im DataHub und sind über REST API abrufbar; Voraussetzung für glaubwürdige Stromverbrauchs-Transparenz gegenüber Endnutzern
12. **Flash Encryption** — NVS-Verschlüsselung als erster Schritt (AES-256, Schlüssel in eFuse, Hardware-Beschleuniger); für Marktreife: Secure Boot + vollständige Flash-Verschlüsselung; verhindert Credential-Extraktion bei gestohlenen Geräten

### Sensor-Architektur
- `SensorEntry` ist generisch — ein Eintrag, ein Messwert (`float`), egal ob DHT22, Wägezelle oder Lichtsensor
- `lastUpdate = 0` als Default ist bewusst — der erste Read erfolgt erst nach dem ersten abgelaufenen Intervall; gibt lokalen Sensoren Aufwärmzeit und Remote-Sensoren Zeit für WiFi-Verbindungsaufbau nach Stromausfall
- Registrierung analog zu Shellys: API-Funktion, zunächst in `setup()` aufgerufen, später über die GUI
- Lokale Sensoren schreiben direkt in DataHub; Remote-Sensoren legen Request in ihrer RequestWorker-Mailbox ab
- `valid` und `lastUpdate` in `SensorEntry` sind load-bearing — Schaltlogik muss Staleness prüfen


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
- Remote-Knoten (ESP32-C3) werden analog zu Shellys über ihre **MAC-Adresse** identifiziert; IP wird dynamisch via `syncApClients()` aktuell gehalten
- **Pull/On-Demand-Modell**: MainUnit fragt den C3 ab — konsistent mit verkabelten Sensoren und Shellys; kein Push
- **Antwortformat** `GET /sensors`: `{"sensor:0": {"value": 58500}, "sensor:1": {"value": 217}}` — Rohwerte; Kalibrierung und Einheit liegen im MainUnit
- **Antwortformat** `GET /status`: `{"mac": "AA:BB:CC:DD:EE:FF", "uptime": 3600, "rssi": -65}` — MAC ist persistenter Identifier, zwingend für Onboarding
- C3 betreibt **Light Sleep** zwischen Abfragen: WiFi-Assoziation bleibt aktiv, eingehende TCP-Verbindung weckt den C3 sofort; spart Strom auch bei kabelgebundener Versorgung
- C3 sitzt **außerhalb** des Terrariums; Sensorkabel wird durch die Terrariumwand geführt; kabelgebundene Stromversorgung
- Der MainUnit fragt den Knoten ab; der Knoten trifft keine Schaltentscheidungen; Kalibrierung liegt vollständig im MainUnit

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
- **NOR-Flash**: ausschließlich Logging — keine Config-Datenhaltung, keine Synchronisation (ersetzt die ursprünglich geplante SD-Karte, siehe Hardware-Abschnitt)
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
- **Companion-App triggert OTA** — REST-Endpoint auf MainUnit; MainUnit lädt Firmware vom NAS (via STA), aktualisiert sich selbst und serviert C3-Firmware lokal über den AP; C3-Nodes pullen und updaten sich selbst
- ESP32-Standard-Partition-Scheme ist OTA-fähig (zwei App-Slots) — gilt für MainUnit und C3; kein physisches Reflashen nötig für zukünftige Updates
- Voraussetzung: REST-API-Layer (GuiWorker) und NOR-Flash (Firmware-Storage) müssen stehen

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
**Nicht mockbar**: FreeRTOS-Mutex, WiFi-Connect, AP-Client-Events

**GUI-Tests ohne Hardware**: Headless Firefox + gemockter `fetch()` — bewährter Ansatz aus dem RemoteSensor-Projekt (Syntax-Check, Funktionsscreenshots, Interferenz-Bugs reproduzierbar); relevant sobald der GuiWorker eine eigene Web-GUI bekommt. Details: `learnings/mainunit-transfer-notes.md`, Abschnitt C.

**Integrationstests** (manuell, vor jedem Merge auf `main`):
- Alle 3 Shellys verbinden sich mit ESP32-AP und werden als `online` markiert
- Unbekannte MAC wird ignoriert (nicht in DataHub eingetragen)
- Schalten funktioniert (on/off), `getStatus` nach Switch bestätigt Zustand
- Shelly ziehen → TCP-Fehler → `setWifiRelayOfflineByIP` setzt `online = false`
- Shelly wieder einstecken → AP-Connect-Event → `syncApClients` → `online = true` + `GET_STATUS`
- Kein Absturz nach >5 Minuten Laufzeit
