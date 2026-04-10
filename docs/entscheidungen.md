# Architekturentscheidungen: terraControll ESP32

---

## Projektweite Entscheidungen (gelten für alle Versionen)

- Obsidian Vault Pfad: /home/lernenrocks/Synology Drive/LR Material/eigene Projekte/terraControll/TerraControlProject/


### Kein Arduino String — nur char[]
- Verboten: Arduino `String`, `new`, `malloc`, `DynamicJsonDocument`, `.c_str()` auf temporärem `String`
- Erlaubt: `char[]` auf Stack, `StaticJsonDocument`, `snprintf`, `strlcpy`, `memcpy`
- Ausnahme: einmaliger `WiFi.SSID()`-Aufruf beim Boot tolerierbar (kein Hotpath)

### Qualitätsprüfung am Ende jedes Meilensteins
- Code auf versteckte Heap-Allokationen prüfen

### Testing-Strategie

**Unit Tests (Google Test, native PlatformIO-Umgebung auf PC)**
- Framework: Google Test — passt besser zu C++ mit Namespaces als Unity
- Läuft in separater `[env:native]` in `platformio.ini` — kein Flash-Verbrauch auf dem ESP32
- Ziel: reine Logik ohne Hardware-Abhängigkeit

Direkt testbare Funktionen:
- `extractValue` — Quotes, kein Quotes, fehlender Key, Puffergrenze
- `normalizeMac` — Großbuchstaben, Kleinbuchstaben, Sonderzeichen
- `parseJson` — valides JSON, fehlendes Pflichtfeld, ungültige Syntax, `aenergy["total"]`
- `HttpUtils::readLine` — Zeilenende `\r\n`, Buffer voll, leere Zeile

**Mocking für hardware-nahe Funktionen**
- `WiFiClient` ist ein `Stream` — ersetzbar durch einen `FakeStream` mit vorgefertigtem Inhalt
- Damit testbar: `skipHeader`, `_updateRelayStatusInternal` (401→200 Ablauf), `parseJson` mit `shelly.json`
- Nicht sinnvoll mockbar: FreeRTOS-Mutex-Logik, WiFi-Connect, mDNS-Discovery

**FreeRTOS-Logik**
- FreeRTOS läuft nicht in der nativen PlatformIO-Umgebung — direkte Unit Tests nicht möglich
- Grundsätzlich testbar durch Stub-Implementierungen von `xSemaphoreTake`/`xSemaphoreGive`
  - Stub gibt immer `true` zurück → Normalfall testen
  - Stub gibt immer `false` zurück → Timeout-Verhalten testen (gibt Funktion korrekt `false` zurück?)
- Aufwand hoch (gesamte FreeRTOS-API nachbauen) — aktuell nicht priorisiert
- Mutex-Logik ist überschaubar und wird durch Integrationstests indirekt abgedeckt

**Integrationstests (manuell, mit echten Shellys)**
- Werden laufend durchgeführt — 3 Shellys mit hardcodierten MACs im Testaufbau
- Checkliste vor jedem Merge auf `main`:
  - Alle 3 Shellys werden per mDNS gefunden
  - MAC-Verifikation schlägt fehl bei unbekannter MAC
  - Schalten funktioniert (on/off, Timer)
  - Relay geht nach Timeout auf offline, wird durch `findStoredRelays` wiederhergestellt
  - Kein Absturz nach >5 Minuten Laufzeit

**Randfälle die explizit getestet werden sollen**
- `extractValue`: Key am Anfang / Ende der Zeile, leerer Value (`key=""`), Key nicht vorhanden
- `parseJson`: fehlendes `sys.mac`, fehlendes `wifi.sta_ip`, `switch:0` nicht vorhanden, `aenergy` kein Objekt
- `normalizeMac`: bereits lowercase, gemischt, mit Trennzeichen (sollte ignoriert werden)
- Mutex-Timeout: DataHub-Mutex nicht verfügbar → Funktion gibt `false` zurück, kein Absturz

### JSON Parsing
- Für flache JSON-Objekte: eigene `parseJSON()`-Funktion mit `strstr` und `char[]`
- Für komplexes JSON: ArduinoJson mit `StaticJsonDocument` (kein Heap)
- Entscheidung 2026-03: Verwendung von `StaticJsonDocument` festgeschrieben, `DynamicJsonDocument` verboten.
- `extractValue()` in DigestAuth.cpp bleibt für HTTP-Header (`key="value"` und `key=value`)
### Escape-Handling bei JSON/DB-Anbindung
- Beim Parsen von JSON-Werten unbedingt ESCAPES behandeln (`\"`, `\\`, `\n`, `\r`, `\t`, `\uXXXX`).
- Testfälle für Datenbank-Import sollten überwachte Eingabetexte nutzen (nicht nur Live-Shelly-Streams).
- Fehlerquellen: `readJsonToken()` / `extractJSonValueFromClient()` (erstes `\`-Handling, String-Ende `"`, Buffer-Bounds, falsches `continue` oder fehlende `tokenSize`-Prüfung).
- Für Unit-Tests entkoppelte Funktionalität (`readJsonToken(Stream&, ...)`, optional `readJsonTokenFromString(const char* ...)`) einbauen.
### WiFiClient direkt (kein HTTPClient)
- Stack-Buffer, kein Heap-Risiko — gilt für alle TCP-Kommunikation zum Schalten

### Projekt-Kontext
- **Prototyp**: liegt in `esp32/terrasteuerung` — lauffähig, nicht mehr weiterentwickelt
- **Dieses Projekt** (`esp32/terraSteuerungShelly`): Neuentwicklung — kein Merge mit Prototyp

---

## Diese Version: WiFi/Shelly-Anbindung

### Ziel
Shelly Gen2/3 über SHA-256 Digest Auth zuverlässig schalten. Grundlage für die nächste Version.

### Fertig ✅
- `DigestAuth::digestAuthGET()` — zweistufiger Digest-Auth GET, gibt HTTP-Code zurück, `expectedMac` entfernt
- `namespace DigestAuth` (öffentlich) + anonymer Namespace (intern)
- `WiFiManager` mit `initWiFi()`, `updateWifiStatus()`
- `DataHub` mit `WiFiStatus` (ssid, rssi, ipV4Adress als char[])

### Zu implementieren (diese Version)
- **mDNS-Discovery**: Shellys per mDNS finden statt hart codierter IP
  - Bereits im Prototyp implementiert — hier neu aufsetzen
  - Ergebnis: IP zur Laufzeit ermitteln, nicht konfigurieren
- **MAC-Check via `Shelly.GetStatus`**: nach mDNS-Discovery MAC des Geräts prüfen
  - `GET /rpc/Shelly.GetStatus` (Digest Auth) → JSON enthält `sys.mac`
  - Schutz vor: Router verteilt IPs neu → falsche Shelly wird geschaltet
  - Entscheidung 2026-04: `GET /shelly` (ohne Auth) verworfen — `Shelly.GetStatus` liefert MAC im selben Request der ohnehin nötigen Status-Abfrage, kein zusätzlicher Request erforderlich
- **NVS-Anbindung**: persistente Speicherung folgender Settings:
  - Geräte-Settings (z.B. Bildschirmhelligkeit)
  - Eingebundene Shellys (MAC + Name + Sensor-Zuordnung)
  - Zu verwendende Sensorwerte pro Shelly
  - Schaltregeln (Schwellwert + Zeitfenster)
  - Fallback für SD-Karte bei Boot-Fehler

### Entschiedene Punkte

**Sensoren: DHT22 (1-Wire / Single-Bus)**
- Aktuell 3x DHT22 angeschlossen, Code für bis zu 4 vorgesehen
- Zusätzlich: ESP32-interne Temperatur + RTC-Temperatur (DS3231 über I²C)
- Protokoll-Mix: DHT22 (Single-Bus) + I²C (RTC) — beide parallel betreibbar

**Stromversorgung: MB102 + Elkos**
- 100µF Elko an 5V-Schiene verbaut

**Display: LVGL — ans Ende verschoben, Flash-Budget erst ermitteln**
- Erst Kernfunktion (WiFi, Sensoren, Regeln, NVS) fertigstellen und Flash-Verbrauch messen
- LVGL bevorzugt — bei 4MB Flash (Standard): OTA-Partitionen je ~1.9MB, kann eng werden
- Fallback: 16MB ESP32 vorhanden (preislich teurer → nur wenn 4MB nicht reicht)
- Entscheidung LVGL vs. TFT_eSPI nach Flash-Budget-Messung

### Architekturentscheidungen

**DigestAuth ist generisch:**
- Port `80` ist einziger Shelly-spezifischer Aspekt — bei Bedarf als Parameter ergänzbar
- SHA-256 ist Standard für Shelly Gen2/3
- Keine MAC-Logik in DigestAuth

**FreeRTOS: delay() vs. vTaskDelay()**
- `delay(1)` hält den gesamten Core an — kein anderer Task kann laufen
- `vTaskDelay(1 / portTICK_PERIOD_MS)` gibt Wartezeit an Scheduler zurück
- Regel: sobald WiFiWorker-Thread eingeführt wird, alle `delay()` in TCP-Schleifen ersetzen

**WiFi-Worker-Thread: Zeitpunkt des Umbaus**
- Entscheidung 2026-04: Zunächst sequenziell und blockierend implementieren
- Trigger für den Umbau: Display-Task blockiert spürbar durch WiFi-Calls (2–3s Freeze)
- Voraussichtlich beim Einführen des Display-Tasks
- Konzept dann: ein WiFiWorker-Task mit FreeRTOS-Queue, alle Netzwerkoperationen sequenziell
  - Queue-Commands: SWITCH_RELAY, GET_STATUS, NTP_SYNC, LOG_EVENT, NOTIFY
  - Zwei Queues für Prioritäten (high: Schalten/Status, low: Logging/Notifications)
- Umbau ist chirurgisch möglich, wenn bis dahin gilt:
  - Alle TCP-Aufrufe bleiben im `WiFiManager` gebündelt
  - `DataHub` bleibt einzige Datenschnittstelle
  - Kein direktes TCP außerhalb des `WiFiManager`

**WiFi Relay: Shelly Gen2/3 als Primärziel**
- Shelly Gen2/3: SHA-256 Digest Auth, stabile API, `GET /shelly` für MAC-Check ✅
- Shelly Gen1: MD5-Digest — bei Bedarf nachrüstbar, kein Scope dieser Version

**DataHub Lock/DTO-Verhalten**
- Einzige Schreib-API in den DataHub: z. B. `updateWifiStatus()`, `applyRelayUpdate()`.
- Getter kopiert Daten (z. B. `getWiFiStatus(out)`), keine direkt zurückgegebenen internen Pointers.
- Lock wird im DataHub gehalten (`xSemaphoreTake/xSemaphoreGive`), um Race Conditions zu vermeiden.
- JSON-Parser erzeugt DTO (`RelayUpdate`) und übergibt diesen an DataHub-Service; DataHub entscheidet, welche Felder übernommen werden.
- Direkter Zugriff auf `dataHub.wifiRelay[]` aus externen Modulen ist verboten (API-Kapselung).

**DataHub / Parser-Architektur**
- `JsonParser` ist Eingangs-/Mapping-Layer; er erzeugt strukturierte Relay-Updates
- `DataHub` ist der Speicher und erhält Änderungen ausschließlich über Getter/Setter-Funktionen
- Bei Zugriffen aus Multi-Task-Umgebung (`WifiWorker`, Hauptloop) Mutex/Lock verwenden
- Zustand wird nicht direkt in externen Tasks verändert, nur über DataHub-API

**Schaltvorgang (empfohlen)**
- Ablauf: `getStatus()` → `switchShelly()` → `getStatus()`
  - `getStatus()` sorgt alle ~30s für aktuelle IP/online-Flags
  - `switchShelly()` nutzt gesicherte IP aus `dataHub.wifiRelay` + `digestAuthGET`
  - Direkt danach `getStatus()` als Verifikation und Wiederholung bei Routing/Lease-Wechseln
- IP-Flip zwischen Status & Schaltvorgang gilt als sehr unwahrscheinlich, dennoch ist Rückabfrage defensiv.

---

## Modul: Watchdog (Monitor)

### Zweck
Eigenständiges Modul zur Laufzeitüberwachung — kein Neustart, kein Eingriff in Schaltlogik.
Ergänzt `checkRelayTimeouts()` im WiFiManager um Plausibilitätsprüfungen auf DataHub-Ebene.

### Abgrenzung zu WiFiManager
- `WiFiManager::checkRelayTimeouts()` — zuständig für Verbindungsverlust:
  - Relay `inUse`, aber `lastUpdate == 0` länger als `SHORT_TIMEOUT` → `findStoredRelays()`
  - Relay `inUse`, `lastUpdate > 0`, aber seit länger als `LONG_TIMEOUT` kein Update → `findStoredRelays()`
  - gibt `return` direkt nach erstem Treffer aus, da `findStoredRelays()` alle Einträge prüft
- `Watchdog` — zuständig für inhaltliche Anomalien auf DataHub-Werten

### Anforderungen

**Plausibilitätsprüfungen (geplant)**
- Strom (`current`) fließt an einer Shelly, obwohl `output == false`
- Weitere Regeln folgen mit Schaltlogik-Implementierung

**Reaktion auf Anomalien**
- Stufe 1: `[WARN]`-Log auf Serial (einheitliches Präfix, sofort)
- Stufe 2: Exception-Log (persistent, z.B. in NVS oder SD — Ziel noch offen)
- Stufe 3 (spätere Version): Notification an User (Push/MQTT o.ä.)
- Kein automatischer Neustart — Hardware-Watchdog (FreeRTOS TWDT) ist dafür zuständig

**Parse-Fehler-Behandlung**
- Kaputte JSON-Responses von Shelly lassen `lastUpdate` unverändert alt
- Dadurch triggert `checkRelayTimeouts()` → `findStoredRelays()` → erneuter Fehler → Schleife
- Watchdog zählt Parse-Fehler pro Relay und gibt ab einer Schwelle eine Warnung aus
- Verhindert Überlastung durch wiederholte sinnlose Anfragen

**Präfix-Konvention (gilt projektübergreifend)**
- `[WARN]` — unerwarteter Zustand, Betrieb weiter möglich
- `[ERROR]` — Fehler, einzelne Funktion nicht verfügbar
- Gilt für alle Serial-Ausgaben im Watchdog (und empfohlen für alle Module)

### Noch offen
- Persistenz von Exception-Logs (NVS vs. SD)
- Konkrete Notification-Implementierung (spätere Version)
- Schwellwert für Parse-Fehler-Häufung

---

## Nächste Version: Sensor-gesteuertes Schalten

### Scope
Sensordaten steuern Shellys zeit- und temperaturgesteuert. Jede Shelly ist genau einem Messwert zugeordnet.

### Basis aus Prototyp übernehmen (Konzept, nicht Code)
- Sensordaten, Regel-Engine, Display-Anbindung, SD-Logging

### Kernfunktionen
- **Shelly-Zuordnung**: 1 Sensor : 1 Shelly — keine n:m-Verknüpfung
- **Regeln**: Schwellwert + Zeitfenster → Shelly ein/aus
- **RTC** (z.B. DS3231): Zeitquelle — läuft ohne WLAN
- **NTP-Sync**: RTC beim Connect synchronisieren — RTC bleibt Master, NTP korrigiert Drift
- **TFT Touch Display**: Pflichtbestandteil — Eingabe von Regeln und Shelly-Zuordnung
- **NVS-Persistierung**: MAC + Shelly-Zuordnung in NVS speichern

### Architekturentscheidungen

**Regel-Speicherung: SD (CSV) + NVS als Fallback**
- SD-Karte startet nicht immer zuverlässig → SD allein nicht ausreichend
- Ablauf beim Boot: SD lesen → in NVS schreiben (Sync) → bei SD-Fehler: NVS als Fallback
- Regeländerung über Display: gleichzeitig auf SD und NVS schreiben

**System-Settings: NVS**
- Shelly-Zuordnung (MAC + Name), WLAN-Credentials → NVS
- Kein CSV für Settings

**Logging-Format: JSON auf SD-Karte**
- Datenbank-Entscheidung noch offen → kein Line Protocol vorgreifen
- JSON ist flexibel für späteren DB-Import
- Ein JSON-Objekt pro Eintrag, append-only pro Tagesdatei

---

## Spätere Version

### Server-Kommunikation: TLS + API Key
Wenn Logging und Settings optional über eigenen Server laufen sollen (DynDNS, Synology NAS):

- **TLS**: Let's Encrypt + ISRG Root X1 im ESP32 eingebettet
  - Certbot erneuert Leaf-Cert automatisch — kein Firmware-Update nötig
  - ESP prüft nur Root-CA → Leaf-Erneuerung transparent
- **Authentifizierung**: API Key pro Gerät, im NVS gespeichert, über TLS übertragen
- **Server-Seite**: Nginx + Rate Limiting + Fail2ban
- **Abgrenzung**: ESP→Shelly bleibt Digest Auth, ESP→Server über WiFiClientSecure

**⚠ Ablauf ISRG Root X1: 30. September 2035**
- Vor Ablauf OTA-Update auf ISRG Root X2 einplanen
- OTA muss vor erstem Produktiveinsatz bei Freunden/Kunden implementiert sein
- Erinnerung für 2034 anlegen

### Fernzugriff
- **Empfohlen**: WireGuard VPN auf Synology NAS
- **Alternativ**: Reverse Proxy (Synology) — HTTPS nach außen, HTTP intern
- **Nicht empfohlen**: Direktzugriff per Port-Forward

### Datenbank
- **InfluxDB**: Zeitreihendaten → Line Protocol
- **CouchDB oder SQLite+REST**: Konfigurationen, Setups
- Logging ab dann direkt als InfluxDB Line Protocol statt JSON

### OTA
- HTTP OTA vom Synology NAS
- Voraussetzung für: ISRG Root X2 Wechsel, Remote-Bugfixes bei Kunden

### ESP als primärer AP für Shellys
- ESP32 spannt eigenes WLAN auf: `ESP32_TerraControl_<ID>`, Passwort aus `ESP.getEfuseMac()` (stabil nach Reboot)
- Shellys verbinden sich direkt mit ESP — Router-Ausfall unterbricht Kernfunktion nicht
- STA-Verbindung (Heimnetz) nur für: Logging, NTP-Sync, OTA
- mDNS funktioniert im AP-Netz weiterhin → MAC-Check unverändert

### Shelly-Onboarding direkt vom ESP
- Shelly (Werkseinstellungen) öffnet AP ohne Passwort → ESP verbindet sich, konfiguriert per RPC-API
  - `POST /rpc/WiFi.SetConfig` → WLAN-Credentials
  - `POST /rpc/Sys.SetConfig` → Passwort setzen
- Nutzer-Flow: Shelly auf Werkseinstellungen → "Neues Gerät" am Display → fertig
- Schutz vor Karpern: nur Werkszustand-Shellys bindbar

---

## Nice to have (kein festgelegter Zeitplan)

### Tasmota-Unterstützung
- Sonoff und viele günstige ESP-basierte Relays mit Tasmota-Firmware
- Einfaches HTTP-GET: `GET /cm?cmnd=Power%20On` — kein Digest Auth
- Eigene Funktion `tasmotaGET()`, unabhängig von DigestAuth
- Tuya-natives Protokoll: nicht empfohlen — proprietär, instabil

### Shelly Gen1
- Verwendet MD5-Digest statt SHA-256
- Kleiner Code-Umbau in `digestAuthGET()` wenn nötig
