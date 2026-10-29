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
- **Verboten in wiederkehrenden Abläufen** (NetworkWorker-Zyklus, Controller-Tick, jede periodisch laufende Schleife): `String`, `new`, `malloc`, `DynamicJsonDocument`, `.c_str()` auf temporärem `String`
- **Erlaubt bei seltenen, User-getriggerten Aktionen** (Onboarding eines Sensors/Relays, Anlegen/Löschen einer Schaltregel): `new`/`delete` — kein wiederholtes Alloc/Free-Muster über die Laufzeit, da diese Aktionen selten und unregelmäßig auftreten, nicht in einer Schleife
  - `new` immer auf Fehlschlag prüfen (`new(std::nothrow)` + Nullpointer-Check) — anders als ein statisches Array kann eine Heap-Allokation fehlschlagen
- **Immer erlaubt**: `char[]` auf Stack, `StaticJsonDocument`, `snprintf`, `strlcpy`, `memcpy`
- Ausnahme: einmaliger `WiFi.SSID()`-Aufruf beim Boot tolerierbar
- Begründung: Fragmentierung entsteht durch wiederholte Alloc/Free-Zyklen unterschiedlicher Größe über Monate Laufzeit — seltene, unregelmäßige Allokationen (Onboarding, Schaltregeln) erzeugen dieses Muster nicht in relevantem Umfang; in jedem periodisch laufenden Codepfad bleibt Heap tabu (2026-08-31, siehe `learnings`-Diskussion zum SensorManager)
- **Geltungsbereich präzisiert (2026-09-26)**: die Regel gilt für den eigenen Code, nicht für Bibliotheken — lwIP (jeder TCP-Request: Verbindungsblock, Paketpuffer) und NVS (`nvs_open()` legt pro `NvsStorage::Session` ein Handle per `new(std::nothrow)` an, in der ESP-IDF-Bibliothek nachgeprüft) allozieren intern ohnehin. Beides ist hingenommen: immer gleiche Größe, sofort wieder freigegeben — genau das Muster, das kaum fragmentiert. NVS-Zugriffe im Zyklus trotzdem gering halten, siehe „NVS-Werte im RAM" unten. Langzeit-Kontrolle über Messung statt Regel: `heap_caps_get_largest_free_block()` neben `SYSTEM_HEAP_FREE` protokollieren — schrumpft der größte freie Block über Wochen bei ausreichend freiem Heap, fragmentiert es

### NVS-Werte im RAM (2026-09-26)
- **Jeder im NVS gespeicherte Wert wird zusätzlich als RAM-Variable gehalten** — NVS wird nur beim Boot (Laden), beim Onboarding und in Settern angesprochen, nie bei lesenden Zugriffen im laufenden Betrieb. Löst die frühere „kein RAM-Cache"-Linie für `ApDevice`/`EntityBase` ab
- **Setter-Reihenfolge**: erst NVS schreiben, nur bei Erfolg den RAM-Wert setzen (Muster wie `RelayBase::setFailsafeSeconds()`) — RAM und NVS laufen nie auseinander
- Nicht-atomare RAM-Werte (z.B. `char name[]`) sind über den Registry-Lock geschützt (siehe `ApDeviceRegistry`), Skalare über `std::atomic`
- **Ausnahme Geheimnisse** (Passwörter, später API-Keys/Tokens): bleiben nur im NVS, werden direkt vor Gebrauch in einen Stack-Puffer gelesen und danach per `SecureUtils::ensureZero()` gelöscht — siehe „RAM-Hygiene" unten. Einen geschützten RAM-Bereich gibt es auf dem klassischen ESP32 (MainUnit: ESP32-WROOM-32D, `esp32dev`) nicht: HMAC-/Digital-Signature-Peripherie erst ab S2/S3/C3, eFuse-Schlüssel nur für Flash Encryption nutzbar; Software-Verschlüsselung im RAM bräuchte den Schlüssel ebenfalls im RAM. `ha1` statt Passwort speichern wurde verworfen — passwortäquivalent für genau dieses Gerät, kein Sicherheitsgewinn

### Kein HTTPClient
- Alle TCP-Kommunikation über `WiFiClient` direkt — Stack-Buffer, kein Heap-Risiko

### JSON Parsing
- Flache JSON-Objekte: eigene `parseJSON()`-Funktion mit `strstr` + `char[]`
- Komplexes JSON: `StaticJsonDocument` (ArduinoJson) — `DynamicJsonDocument` verboten
- `extractValue()` in `DigestAuth.cpp` bleibt für HTTP-Header (`key="value"` und `key=value`)
- Escape-Handling bei JSON zwingend: `\"`, `\\`, `\n`, `\r`, `\t`, `\uXXXX`

### JSON-Erzeugung (Ausgabe)
- `StaticJsonDocument` + `serializeJson()` ist der Standard für erzeugtes JSON, auch bei flachen/einfachen Objekten — nicht nur bei echter Verschachtelung
- Grund für die Abkehr vom handgebauten `snprintf("{\"%s\":\"%s\"}", key, value)`-Muster: kein Escaping bei Interpolation von User-Eingaben (z.B. Sensor-/Relay-`name`) — ein Anführungszeichen im Namen bricht das erzeugte JSON oder ermöglicht JSON-Injection. `serializeJson()` escaped automatisch korrekt
- Ursprünglich war das Flash-/RAM-Budget durch die (inzwischen verworfene) Display-Option ein Grund, `StaticJsonDocument` sparsam einzusetzen — mit der Headless-Entscheidung entfällt dieser Grund, `StaticJsonDocument` ist jetzt der Standardweg (2026-09-01)

### Serial-Präfixe
- `[WARN]` — unerwarteter Zustand, Betrieb weiter möglich
- `[ERROR]` — Fehler, einzelne Funktion nicht verfügbar

### RAM-Hygiene für Zugangsdaten
- Klartext-Zugangsdaten (WLAN-Passwörter, Admin-Passwort) werden nach Gebrauch aktiv aus dem RAM gelöscht — Schreibzugriff über einen `volatile`-Pointer (oder eine äquivalente, vom Compiler nicht wegoptimierbare Methode), kein einfaches `memset` auf einer Variable, die direkt danach aus dem Scope fällt (der Compiler darf das sonst als toten Schreibzugriff wegoptimieren)
- Code wird so geschrieben, als wäre Flash Encryption (Schritt 12) bereits aktiv — kein Aufschub von RAM-Hygiene-Maßnahmen mit der Begründung "NVS ist eh noch unverschlüsselt"
- Begründung: schützt gegen ein Szenario, das unabhängig von physischem Gerätezugriff und unabhängig von Flash Encryption besteht — ein Crash-Dump (NOR-Flash-Log, per ApiWorker abrufbar) könnte ein zufällig noch im RAM stehendes Passwort enthalten; wer gültigen Admin-Zugriff auf die REST-API hat (aber keinen physischen Gerätezugriff), könnte diesen Log herunterladen (2026-09-13)

---

## Architektur

### Module
- **`HttpClient`**: generischer HTTP-Client — `get()` für authentifizierte und nicht-authentifizierte GET-Requests; konsumiert Response-Header intern; Stream liegt am Body bei Rückgabe; enthält `readLine` und `skipHeader`
- **`DigestAuth`**: SHA-256 Digest Auth — baut auf `HttpClient` auf; erster Request und WWW-Authenticate-Parsing selbst; zweiter Request über `HttpClient::get()` mit Authorization-Header; keine Shelly-Logik, keine MAC-Logik
- **`NetworkUtils`**: Konvertierungen für MAC und IP — `normalizeMac`, `macBytesToChar`, `macCharToBytes`, `ipToChar`; kein HTTP-Bezug; löst `HttpUtils` ab
- **`WiFiManager`**: reine Konnektivität — `initWiFi`, `syncStaConnection`, `getRssi`, `getLocalIp`, `resolveIp`; Schalt-/Status-Funktionen sind seit 2026-09-13 entfernt. Requests an Geräte laufen über `HttpClient`/`DigestAuth` und werden ausschließlich aus dem NetworkWorker aufgerufen
- **`ApDeviceRegistry`** (2026-09-16, Stand Code 2026-10-25): hält bis zu `DEVICE_MAX` `ApDevice*`, Index = `deviceId`. `init()` (Boot, `ApDevice::recover()` pro Slot), `create(data)` (Onboarding über `ApDevice::onboard()`, vorher zentrale Prüfung freier NVS-Einträge), `remove(id)` (nur vom User), `find(lock, id)`/`find(lock, mac)`, `getMac()`, `getFreeSlots()`. Freier Slot = `nullptr` und `!ApDevice::hasData(id)`; ein Slot mit NVS-Daten, der nicht ladbar ist, bleibt belegt (fehlerhaft, `[ERROR]` beim Boot). Zugangsdaten liegen am `ApDevice`, nicht an der Registry. Kennt keine Relais- oder Sensor-Domänenlogik — analog zu Home Assistants "Device"-Konzept (siehe „Device/Entity-Prinzip" unten). Bewusst beschränkt auf lokale, AP-gebundene Geräte mit Digest Auth (Shellys, eigene C3-Knoten) — Cloud-Sensoren (Netatmo etc.) fallen explizit nicht darunter, andere Auth-Form
  - **Registry-Lock (2026-09-26)**: NetworkWorker, Controller und ApiWorker greifen auf dieselben `ApDevice`-Objekte zu, der ApiWorker kann sie per `remove()` löschen — ohne Schutz wäre ein gehaltener `ApDevice*`/`EntityBase*` danach ein hängender Zeiger (Use-after-free). Lösung: ein RAII-`ApDeviceRegistry::Lock` (Muster wie `NvsStorage::Session`), der Aufrufer hält ihn, keine Registry-Funktion lockt selbst (sonst Deadlock, FreeRTOS-Mutex ist nicht rekursiv). `find()` (beide Überladungen) verlangt den Lock als Parameter (`find(lock, id)`) — einziger Weg an einen Geräte-/Entity-Zeiger, der Compiler verweigert also jeden Zugriff ohne Lock. `create()`/`remove()`/`getMac()`/`getFreeSlots()` setzen ihn ebenfalls voraus, aber nur per Kommentar erzwungen (bewusst auf `find()` begrenzt)
  - **Regeln**: ein Zeiger aus der Registry gilt nur, solange der Lock gehalten wird; **nie über ein Warten hinweg** (TCP-Request, `vTaskDelay`) — sonst blockiert ein Shelly-Timeout Controller und ApiWorker für Sekunden. Reihenfolge immer erst Registry-Lock, dann NVS-`Session`, nie umgekehrt. Lock benannt anlegen (`Lock lock;`), nicht `Lock();` (temporär, sofort wieder frei) oder `Lock lock();` (deklariert eine Funktion)
  - **Verworfen**: Lock im `ApDevice`-Objekt selbst — stirbt mit dem Objekt, schützt nicht gegen dessen Löschung; `std::shared_ptr` — verspäteter Destruktor würde NVS eines inzwischen neu belegten Slots löschen, Lebensdauer im Code unsichtbar
- **`ApDevice`** (2026-09-24, Stand Code 2026-10-25): physisches AP-gebundenes Gerät (Shelly, künftig C3). Fabrikfunktionen `recover(id)` (Boot, aus NVS) und `onboard(id, data)` (validiert und schreibt; bei Fehlschlag `erase()` und `nullptr`), privater Konstruktor, keine Kopie. Der Destruktor gibt nur frei — NVS gelöscht wird ausschließlich über `erase()` (Onboarding-Fehlschlag, `ApDeviceRegistry::remove()`). NVS-Namespace `apDev_<id>`; MAC, Username, Name und `DeviceType` als RAM-Spiegel, das Passwort nur im NVS (`getPassword()` in einen Aufruferpuffer, danach `ensureZero()`). `online`/`lastSeen` sind RAM-Atomics. Hält seine Entities per Komposition (`std::vector<EntityBase*>`, Position = NVS-Index, nicht ladbare Entity als `nullptr`). `applyResponse()` bekommt nur bereits verifizierte Antworten (MAC-Prüfung im NetworkWorker) und verteilt sie an die Entities. **Schaltmodell B (Code steht aus)**: berechnet auf Anfrage des Workers die fälligen Requests, hält `updateTime` (NVS) und `lastTry`, wertet `broken` und `lastDecision == 0` aus
- **`EntityBase`**: generische Basis für jede Entity (Relais, künftig Sensor-Entities). Bekommt vom Gerät dessen NVS-Namespace und die eigene Position; Keys `e<idx>_<key>` (Key-Name ≤ 10 Zeichen, per `static_assert` geprüft) — kennt die eigene Device-ID nie. Fabrik-/`load()`/`write()`-Kette wie beim `ApDevice`, Name als RAM-Spiegel. `applyResponse(JsonObjectConst)` ist pure virtual
- **`RelayBase`** (erbt `EntityBase`): gemeinsame Basis für jeden schaltbaren Ausgang, unabhängig vom Hersteller. Hält `actualOn` (von der konkreten Klasse gesetzt), `desiredOn` (vom Controller gesetzt), `RelayMode`, `failsafeSeconds` und `outputIdx` (Pflichtfeld, Shelly-`id`). `actualOn`/`desiredOn` RAM-only, nach Reboot `false`; `failsafeSeconds` (Minimum 30 s), `outputIdx` und (**2026-10-29 entschieden, Code steht aus**) `RelayMode` liegen im NVS. Dazu kommen für Schaltmodell B (Code steht aus) die RAM-Werte `lastSwitchSent`, `lastDecision` und `broken`. `buildSwitchRequest()` ist pure virtual
- **`ShellyGen2Relay`** (neu, erbt `RelayBase`): deckt alle Shelly Gen2/3-Varianten ab (Plug S, Power Strip, ...) — dünn, nur `buildSwitchRequest()` (RPC-Text) und `applyResponse()` (`actualOn = data["output"]`)
- **`DeviceType`/`EntityType`**: Enums, in NVS gespeichert (nie umnummerieren/wiederverwenden, Gruppen mit Lücken für künftige Typen). `EntityType` deckt schon mehr ab als aktuell gebaut ist — auch lokale Sensoren (DHT22 etc.) und MainUnit-interne Metriken (Heap/NVS/NOR-Restplatz, RTC-Temperatur) sind als künftige Entity-Typen vorgesehen, noch nicht angebunden
- **`SecureUtils`**: `ensureZero(buffer, len)` — RAM-Hygiene-Helfer, siehe „RAM-Hygiene für Zugangsdaten" oben
- **`System`**: MainUnit-eigene (nicht Device-gebundene) Einstellungen — eigener Gerätename, Admin-Passwort (+ vorberechneter `ha1`), STA-SSID/Passwort, STA-Modus-Umschalter, Versionsstring; Factory-Defaults dafür in `InitalCredentials.h`
- **Kein separates `RelayRegistry`** (Stand 2026-09-25, korrigiert): war als eigene Registry für Relais-Ausgänge geplant, wurde aber nie als eigene Klasse gebaut — Relais-Entities hängen direkt in der `entities`-Liste ihres `ApDevice` und werden über `ApDevice::getEntity(idx)` angesprochen, keine separate Registry-Ebene dafür. `RelayRegistry` taucht nur noch im alten, nicht migrierten `NetworkWorker.cpp` auf
- **`NetworkWorker`** (Nachfolger von `WiFiWorker`): dedizierter FreeRTOS-Task auf Core 0 — fragt pro Zyklus jedes `ApDevice`, ob etwas fällig ist, und arbeitet die gelieferten Requests ab (Schalten, Status, bei C3 Messwerte); dazu NTP-Sync-Check und STA-Reconnect. Einzige Stelle mit Netzwerkverkehr zu den Geräten und einzige Stelle, die `WiFiManager`-Funktionen aufruft; reine Client-Rolle, trifft keine Schaltentscheidung. Ablauf siehe Abschnitt „NetworkWorker“ und „Schaltmodell B“. **Aktuell noch alter Stand** (`NetworkWorker.cpp` referenziert `RelayRegistry`/`DataHub`), kompiliert nicht
- **`ApiWorker`** (neu): dedizierter FreeRTOS-Task auf Core 0 — HTTP-Server für Konfiguration, Monitoring, manuelle Relay-Overrides, manuelle Zeiteinstellung (Standalone-Fallback ohne NTP) und Log-Download aus dem NOR-Flash; horcht auf AP- **und** STA-Interface gleichzeitig, damit die MainUnit auch ganz ohne Heimrouter bedienbar bleibt; eigene SHA-256 Digest Auth in Server-Rolle (Referenzimplementierung bereits im RemoteSensor-Projekt vorhanden, siehe `learnings/mainunit-transfer-notes.md`); reine Server-Rolle, kein Zugriff auf `WiFiManager`
- **`Controller`**: Schaltlogik-Orchestrator — läuft im Loop-Task (Core 1, Takt 1 s); wertet pro Relais den `RelayMode` bzw. die Schaltregel aus und schreibt bei jeder gültigen Entscheidung `desiredOn` + `lastDecision` an die Relais-Entity (Schaltmodell B); kein Netzwerk-Timing, keine Befehle, kennt keine Netzwerkdetails. **Aktuell noch alter Stand** (referenziert `DataHub`/`WifiRelay`, die es nicht mehr gibt) — nicht auf `ApDevice`/`EntityBase` migriert, kompiliert nicht (Teil von Schritt 3a)
- **Kein `DataHub` mehr** (entfernt, Stand 2026-09-25): `DataHub.h`/`.cpp` sind gelöscht. Die Rolle als "einzige Datenschnittstelle" ist nicht durch ein Nachfolgemodul ersetzt, sondern aufgelöst — jede Klasse hält ihren eigenen Zustand selbst: `ApDevice`/`EntityBase`-Ableitungen lesen/schreiben direkt über `NvsStorage`, kein zentraler Datenknoten, kein zentrales Lock mehr. `Controller.cpp`/`main.cpp`/`NetworkWorker.cpp` referenzieren `DataHub` aber noch (alter, nicht migrierter Code)
- **`JsonParser`**: nach `debricated/JsonParser.cpp`/`.h` verschoben (deprecated, 2026-09-16) — kein aktives Modul mehr, siehe „GET_STATUS/SWITCH-Ablauf" unten für den Nachfolgepfad (`deserializeJson()` direkt im `NetworkWorker`)
- **`Monitoring`** (Arbeitsname, 2026-10-26 umbenannt von „Watchdog“ — Verwechslung mit dem ESP-Task-Watchdog vermeiden): inhaltliche Laufzeitüberwachung, kein Eingriff in Schaltlogik — prüft Anomalien auf Entity-Werten (z.B. Strom fließt obwohl `output == false`); zählt Parse-Fehler pro Relay und warnt ab Schwelle; Reaktionsstufen: `[WARN]` → Exception-Log (NVS/NOR-Flash) → später Notification; kein automatischer Neustart. Noch nicht gebaut
- **ESP-Task-Watchdog** (Hardware-/IDF-Funktion, getrennt vom `Monitoring`, 2026-10-26 besprochen, noch nicht eingebaut): überwacht, ob Tasks noch laufen — Controller (Loop-Task: `enableLoopWDT()`/`feedLoopWDT()`) und Worker (`esp_task_wdt_add()`/`esp_task_wdt_reset()`) füttern ihn pro Takt; bei Ausbleiben Neustart (explizit per `esp_task_wdt_init(timeout, true)` setzen, Arduino-Default ungeprüft). Erkennt hängende Tasks, nicht falsch entscheidende. **Gegen Neustart-Schleifen (2026-10-29)**: jeder Netzwerkaufruf hat einen Timeout; der NetworkWorker füttert den Watchdog nach jedem einzelnen Netzwerkaufruf, nicht nur pro Durchlauf; Watchdog-Timeout > längster Einzelaufruf mit Reserve (z.B. ≤ 5 s / 30 s) — sonst triggert ein langer Durchlauf nach Boot oder Netzausfall (alle Geräte fällig, mehrere offline) Neustart um Neustart. Hängender Worker ist für die Relais sicher (keine Erneuerung → `toggle_after`)
- **`SensorManager`**: Sensor-Orchestrator — hält `std::vector<SensorBase *>` (kein generisches `SensorEntry`-DTO, das gibt es nicht); lokale Sensoren (DHT22, SoilMoisture) direkt lesen, Wert per `readSensor(value, raw, sensorId)` abrufbar (Pull, `false` bei ungültigem Wert; Zeitpunkt des letzten gültigen Werts über `SensorBase::getLastValid()`); feste `sensorId`s, nichts rutscht nach. Messwerte von C3-Knoten und Shelly-Leistungsmessung laufen **nicht** über den SensorManager, sondern als Entities am jeweiligen `ApDevice` (2026-10-29) — eine einheitliche Abfrage beider Quellen für den Controller ist offen (Controller-Sprint)

### Relay-Zykluslogik
- **RelayMode** (Enum in `RelayBase`): `AUTO` | `FORCED_ON` | `FORCED_OFF`
  - `AUTO`: Schaltregel entscheidet anhand des Messwerts
  - `FORCED_ON` / `FORCED_OFF`: überschreibt die Schaltregel; der Controller ignoriert den Messwert und schreibt `desiredOn` + `lastDecision` trotzdem pro Takt
  - Mode wird im NVS gespeichert und überlebt einen Neustart (2026-10-29, Code steht aus); geschrieben nur vom ApiWorker (User)
- **`bool broken`** (RAM, kommt im Monitoring-Sprint): gesetzt vom Monitoring bei anhaltender Abweichung trotz gesendeter Befehle; ausgewertet nur vom `ApDevice` (keine Schalt-Requests mehr für diese Entity); orthogonal zum `RelayMode`; quittiert vom User. Nach einem Neustart wird ein bleibender Defekt erneut erkannt
- **Einschalten, Erneuern, Ausschalten, Wiederholen**: Regeln siehe „Schaltmodell B“ (Failsafe-Erneuerung, Frist, Neustart). Der frühere Controller-getriebene Ablauf (`SWITCH_ON`-Wiederholung, 2 Ausschalt-Wiederholungen, dann `broken`) ist seit 2026-10-26 abgelöst

### FreeRTOS
- **NetworkWorker** und **ApiWorker** laufen beide auf **Core 0** — alle Netzwerk-Tasks gebündelt auf einem Core (Modultrennung nach Abhängigkeit: beide brauchen den WiFi-Stack), Core 1 bleibt exklusiv für Controller/SensorManager
- Arduino loop() läuft auf Core 1 — Controller und SensorManager laufen dort
- **Akzeptierter Tradeoff**: Die Schaltentscheidung läuft auf Core 1, ihre Ausführung (Abgleich im NetworkWorker → TCP) auf Core 0 — durch den Abgleich über `desiredOn` ohnehin entkoppelt, bewusst hingenommen statt einen gemeinsamen Core zu erzwingen
- ApiWorker bekommt eine niedrigere Priorität als NetworkWorker — ein langer Log-Download (bis zu 16 MB aus dem NOR-Flash) darf die Relay-Steuerung nicht verzögern; der FreeRTOS-Scheduler erzwingt den Vorrang automatisch
- `delay()` in TCP-Schleifen → `taskYIELD()` (gibt CPU ab ohne feste Wartezeit)
- `vTaskDelay(pdMS_TO_TICKS(50))` am Ende jedes Worker-Zyklus — gibt anderen Tasks CPU-Zeit

### NetworkWorker (Nachfolger von WiFiWorker)
- Dedizierter FreeRTOS-Task, gestartet am Ende von `initWiFi()`; keine WiFi-Event-Handler
- **`DEVICE_MAX`** (`ApDevice.h`, 10): eine gemeinsame Obergrenze für Shellys + C3-Knoten zusammen (nicht getrennt pro Typ), weil beide sich denselben SoftAP-`max_connection`-Topf teilen (ESP32: Default 4, bis 10 konfigurierbar). Gilt nicht für lokal verkabelte Sensoren (DHT22, SoilMoisture) — die sind keine AP-Clients
- **STA-Reconnect-Backoff (30s)**: nach einem STA-Disconnect (z.B. Heimrouter weg) wartet der Worker 30s vor dem nächsten `esp_wifi_connect()`-Versuch, statt sofort zu reconnecten — verhindert, dass wiederholte Reconnect-Versuche den LWIP-Stack so auslasten, dass AP-seitige Verbindungen (Shellys, SensorNodes) darunter leiden (dokumentiertes ESP32-Problem, 2026-08-31 recherchiert)
- **Keine Mailboxen**: weder pro Relais (abgelöst 2026-10-26) noch pro Remote-Sensor (abgelöst 2026-10-29) — der Worker holt sich die fälligen Requests beim `ApDevice` ab (Schaltmodell B). Die frühere Planung mit `nextStatusDue[]` im Worker und engem/lockerem Intervall ist ebenfalls abgelöst (`updateTime` + `lastTry` am Gerät)
- **Zyklus** (2026-10-29), für jedes Gerät nacheinander:
  1. Registry-Lock, `find(lock, id)`, Fällig-Prüfung: fällig, wenn ein Schalt-Request ansteht oder `now - lastTry >= (online ? updateTime : OFFLINE_RETRY)`. Requests, MAC, Username, Passwort auf den Stack kopieren, Lock freigeben
  2. Ohne Lock: `resolveIp()`, Requests abarbeiten, ggf. ~50 ms Pause, ein `GetStatus` für das ganze Gerät; Watchdog nach jedem Netzwerkaufruf füttern; Passwort per `ensureZero()` löschen
  3. Registry-Lock, Gerät neu suchen, Ergebnis eintragen (Regeln siehe „Schaltmodell B“, gelöschtes Gerät)
  4. Nach allen Geräten: NTP-Sync-Check, dann `vTaskDelay(50 ms)`
- **Timeouts**: jeder Netzwerkaufruf (Connect, Lesen, `deserializeJson()` auf dem Stream) hat einen Timeout (Start: 5 s) — siehe ESP-Task-Watchdog im Modul-Abschnitt
- **Keine STA-Queue vorerst** (2026-09-13 entschieden): NTP-Sync läuft als intern getakteter Check — kein externer Trigger nötig. Telegram-Bot (Monitoring) und optionale Datenbank sind zurückgestellt; eine echte Queue dafür ist später rein additiv nachrüstbar
- Alle Netzwerkoperationen laufen sequenziell im Worker — kein direktes TCP außerhalb

### Device/Entity-Prinzip (2026-09-16, geräteübergreifende Leitlinie)
- Erkenntnis aus der Redesign-Sitzung: praktisch jede etablierte Smart-Home-Plattform (Home Assistant, Zigbee2MQTT, openHAB) trennt **Device** (physische Identität + Zugangsdaten, ein gebündelter Zugang) von **Entity** (einzelner steuer-/beobachtbarer Punkt am Gerät, mehrere pro Device möglich) — Home Assistants `DataUpdateCoordinator`-Muster macht genau das: ein gebündelter Poll pro Device, mehrere Entities lesen aus dem gemeinsamen Ergebnis
- Bei uns: `ApDevice` (verwaltet von `ApDeviceRegistry`) = Device-Ebene (MAC, Benutzername, Passwort, Poll-Zeitplan), `EntityBase`-Ableitungen (`RelayBase`/`ShellyGen2Relay`, künftig Sensor-Entities wie PowerMetering oder C3-Messwerte) = Entity-Ebene, adressiert über `deviceId` + Entity-Position. Ein C3-Knoten ist ein Device mit mehreren Sensor-Entities
- Die Trennung entspricht ungefähr HTTP Header (Auth/Verbindung, Device-Ebene) vs. Body/Anfrageinhalt (Domänenwissen, Entity-Ebene): ändert sich das Auth-Verfahren, betrifft das `ApDevice`; ändert sich der Umfang der Abfragen/Befehle, betrifft die konkrete Entity-Klasse
- **Gilt auch fürs Onboarding**: ein `ApDeviceRegistry::create(data)` legt das Gerät (MAC, Benutzername, Passwort, einmal pro physischem Gerät) und alle Entities aus dem Onboarding-JSON an — bei einem Power Strip ein Aufruf, vier Relais-Entities (jede mit eigenem `outputIdx`); C3 analog mit Sensor-Entities
- Zugriffsmuster einzeln, Befehle einzeln: `Switch.Set` ist zwingend pro Ausgang — kein Batching mehrerer Befehle in einem HTTP-Request, laut Shelly-API-Doku dafür kein Beleg (jeder `/rpc`-Aufruf zeigt nur einzelne Calls). Nur die *Beobachtung* (`GET_STATUS`) ist geräteweit bündelbar, weil `Shelly.GetStatus` von sich aus alle Ausgänge auf einmal zurückgibt

### Referenz-Stabilität (2026-10-24, in Arbeit)
- **Grundsatz**: eine Referenz (Schaltregel → Gerät/Entity/Sensor), die nicht mehr sicher stimmt, läuft ins Leere, nie auf etwas anderes. Die betroffene Regel wird ungültig und für den User sichtbar markiert; eine automatische Reparatur gibt es höchstens als Vorschlag. Ungültige Referenz = ungültiger Sensorwert im Sinne der Schaltlogik (nicht schalten, Failsafe läuft aus)
- Fokus sind stille Nebeneffekte; vom User ausgelöste Änderungen verantwortet der User, die MainUnit markiert die davon betroffenen Regeln selbst ungültig
- **Entity-Position = NVS-Index**: `ApDevice::load()` legt eine nicht ladbare Entity als `nullptr` an ihrer Position ab, statt sie zu überspringen — sonst rutschen die folgenden Entities nach (z.B. nach Firmware-Rollback mit unbekanntem `EntityType`) und eine Regel liest still die falsche. `getEntity()` kann daher auch für `idx < getEntityCount()` `nullptr` liefern; Aufrufer behandeln das als fehlende Entity
- **Entity-Reihenfolge beim Onboarding**: ergibt sich aus der Position im Onboarding-JSON. Für Produkte mit fester Hardware (Shelly) liegt das Onboarding-JSON als Vorlage hart im Code — nur hinten erweitern, nie umsortieren (gleiche Regel wie bei den NVS-Enums). Beim C3 baut der Wizard es aus dessen Info-JSON, sortiert nach Anschlussnummer (`sensor:N`)
- **Geräte-Slots (2026-10-24)**: kein stilles Löschen — jede Löschung eines `ApDevice` kommt vom User. Ein Gerät, das beim Boot nicht geladen werden kann, wird als fehlerhaft markiert und bleibt stehen (NVS unangetastet), bis der User es sich ansieht; ein fehlerhafter Slot gilt nicht als frei. Freie Slots dürfen von neuen Geräten belegt werden; bei `DEVICE_MAX` Geräten wird nichts mehr eingebunden, bis der User eines löscht. MAC als Regel-Identität verworfen (Ursprung: Postbox-Planung pro Relais; mit Device/Entity-Trennung nicht mehr zwingend, aber Regeln sollen ein Re-Onboarding ohnehin nicht überleben)
- Noch offen: wie Regeln eine Neubelegung eines Slots erkennen (Ungültig-Markierung bei jeder Löschung vs. Generationszähler pro Slot) (Slot muss recycelt werden, Regeln dürfen nicht auf den Slot zeigen) und lokale Sensoren

### Schaltmodell B: Abgleich (entschieden 2026-10-26, Szenarien 1–11 durchgespielt 2026-10-28/29)
- **Entscheidung**: Schaltbefehle laufen als Abgleich von gewünschtem und gemeldetem Zustand, nicht als Befehle in Mailboxen. Löst „SWITCH-Pfad bleibt Controller-getrieben“ (2026-09-16) und die Mailbox pro Relais ab — die alte Entscheidung beruhte auf dem Stand mit `DataHub` (Zustand gespiegelt/doppelt gelesen); heute liegt der Zustand an genau einer Stelle (Entity unter Registry-Lock)
- **Controller**: setzt pro Relais-Entity nur `desiredOn` (+ Bestätigung, siehe unten); kein Timing, keine Befehle, keine Mailbox
- **`ApDevice`**: liefert auf Anfrage des Workers einen oder mehrere fertige Requests, berechnet aus dem Stand seiner Relais-Entities (`desiredOn`/`actualOn`, Failsafe-Erneuerung fällig) — die „Postbox“ ist berechnet, nicht gespeichert. Ob gebündelt oder einzeln, ist Sache der Geräteklasse (Shelly laut Doku: `Switch.Set` nur pro Ausgang, JSON-RPC-Liste über `/rpc` nicht dokumentiert → beim Hardware-Test mit einem POST prüfen)
- **NetworkWorker**: arbeitet die Requests eines Geräts ab und fragt danach **einmal** `GetStatus` für das ganze Gerät ab (statt einmal pro Ausgang). Bleibt eine Abweichung, wird im nächsten Durchlauf erneut geschaltet (`Switch.Set` ist idempotent)
- **Monitoring**: entscheidet über `broken` bei dauerhafter Abweichung von `desiredOn`/`actualOn` — der Worker braucht dafür keine eigene Wiederholungs-/Aufgabelogik
- **Sicherheitsregel 18.10. (ungültiger Sensor → Failsafe nicht verlängern)** braucht ein zweites Signal vom Controller — entschieden 2026-10-28: Gültigkeitsfrist `lastDecision`, siehe unten
- Recherche-Grundlage (2026-10-26): Trennung gewünscht/gemeldet (openHAB, AWS IoT Device Shadow), „nur der neueste Wunsch zählt“, Statusabfrage direkt nach dem Befehl (Home Assistant `async_request_refresh`), Anfragen sequenziell
- **Timing (2026-10-28)**: der Worker führt keinen Zeitplan — er fragt pro Zyklus jedes Gerät „liegt was an?“ (unter Lock, nur Vergleiche, überlauffest `now - t >= interval`) und holt sich die fertigen Requests ab; Zeitstempel liegen an Gerät/Entity. Pollzeit: eine `updateTime` pro `ApDevice` (Default je `DeviceType`, später GUI, NVS + RAM) + `lastTry`; jedes `GetStatus` (planmäßig oder nach einer Schaltung) setzt den Timer neu. Kein Intervall pro Entity — Pollen von außen liefert ohnehin alle Entities auf einmal (Muster wie Home Assistant `DataUpdateCoordinator`). Remote-Sensoren liefern ihre Messwerte im selben Intervall
- **Frist statt Flag (2026-10-28)**: Controller schreibt pro Relais `lastDecision` bei jeder gültigen Entscheidung (an und aus); Worker erneuert den Failsafe nur bei frischem Wert — hängender Controller lässt den Failsafe auslaufen, unabhängig vom Task-Watchdog. `now - lastDecision` = Dauer der Störung; seit wann ein Sensor ungültig ist, liefert `SensorBase::getLastValid()`. Verworfen: Enum `OFF/ON/HOLD`, Erneuerung im Controller, `invalidSince`. Die Frist sperrt nur Einschalten und Erneuern — Ausschalten läuft immer (sichere Richtung)
- **Zeitstempel (2026-10-28, alle RAM-only, nach Boot 0)**: `ApDevice::lastSeen` (letzter Erfolg, Gültigkeit), `ApDevice::lastTry` (letzter Poll-Versuch, Zeitplan), `RelayBase::lastSwitchSent` (letztes erfolgreiches `Switch.Set` → Erneuerung `failsafe/2`, Wiederholpause ~3 s), `RelayBase::lastDecision` (Controller, Frist). Worker schreibt alle außer `lastDecision`
- **Abweichung = Schaltbefehl (2026-10-28)**: jede Abweichung `desiredOn`/`actualOn` löst einen Schaltbefehl aus, egal woher sie kommt (auch Taster an der Shelly — für TerraControl nicht relevant). Kein `MANUAL`-Mode; Überschreiben nur über `FORCED_ON`/`FORCED_OFF`. Wartung am Verbraucher nur stromlos (Stecker/Sicherung) — Warnhinweis in Anleitung und GUI; Anleitung zusätzlich: TerraControl arbeitet nur zuverlässig, wenn der Taster an der Shelly nicht benutzt wird
- **C3 mit demselben Interface** (2026-10-28, bestätigt 2026-10-29): Requests tragen eine Kennung, die Antwort geht mit Kennung zurück ans Gerät; der Worker kennt keine Endpunkte. **Die Remote-Sensor-Mailbox entfällt** — ein C3 ist ein `ApDevice` wie jedes andere
- **Schaltbefehl und `GetStatus` im selben Durchlauf (2026-10-29)**, eventuell ~50 ms Pause dazwischen (`vTaskDelay`), damit das Relais angezogen hat — beim Hardware-Test prüfen, ob nötig
- **Gerät offline (2026-10-28)**: kein gespeicherter Befehl, nach Rückkehr zählt nur der aktuelle Wunsch. Offline-Geräte werden mit festem `OFFLINE_RETRY` (~30 s) statt `updateTime` erneut versucht (Basis `lastTry`) — ein noch in der AP-Client-Liste stehendes, nicht antwortendes Gerät würde sonst bei jedem Versuch den Worker um den TCP-Timeout bremsen
- **Gelöschtes Gerät mitten im Durchlauf (2026-10-28)**: Schritt 3 des Lock-Ablaufs — `nullptr` → verwerfen; `lastTry` immer; `lastSeen`/`lastSwitchSent`/`applyResponse()` nur nach erfolgreichem `GetStatus` mit passender `sys.mac` (deckt auch einen neu belegten Slot ab — unabhängig davon, wer das Onboarding ausführt)
- **Re-Onboarding derselben Shelly (2026-10-28)**: fürs Schalten abgedeckt (neues `ApDevice` startet mit RAM-Anfangswerten, nichts schaltet ein); Anforderung „unterbrechungsfrei während des Onboardings“ siehe „Onboarding-Wizard“
- **Failsafe-Erneuerung (2026-10-29)**: Einschalten bei `desiredOn && !actualOn`, frischem `lastDecision` und `now - lastSwitchSent >= RETRY_PAUSE`; Erneuern bei `desiredOn && actualOn`, frischem `lastDecision` und `now - lastSwitchSent >= failsafe/2` — beide mit `Switch.Set on, toggle_after = failsafe`. Die zweite Hälfte ist Reserve für Fehlschläge (`lastSwitchSent` bleibt alt, Erneuerung bleibt fällig; offline alle `OFFLINE_RETRY`). Sehr kurzer `failsafe` → Relais fällt bei Ausfall ab (sicher). `FORCED_ON`: Controller schreibt `lastDecision` ebenfalls pro Takt. Voraussetzung (im Prototyp bestätigt, Pflicht-Hardwaretest pro Relaistyp): erneutes `Switch.Set on` startet `toggle_after` neu
- **Frische von Messwerten (2026-10-29)**: ein Wert gilt als gültig, solange `now - lastSeen <= 3 × updateTime` (fester Faktor, nicht vom User einstellbar); lokale Sensoren analog mit eigenem Leseintervall und `SensorBase::getLastValid()`. Ungültig/zu alt → Controller entscheidet nicht → `lastDecision` altert → Failsafe läuft aus. Offen für den Controller-Sprint: einheitliche Abfrage von Gültigkeit/Frische über alle Messwert-Quellen (lokale Sensoren, Entities an `ApDevice`)
- **`broken` vs. Verbraucher-Anomalie (2026-10-29)**: `broken` nur, wenn das Relais nicht folgt (`desiredOn ≠ actualOn` trotz Befehlen) — bleibt bis zur Quittierung durch den User. Nur das `ApDevice` kennt `broken`: für eine defekte Relais-Entity berechnet es keinen Schalt-Request mehr (weder ein noch aus); Worker und Controller wissen nichts davon, `GetStatus` läuft weiter. Von uns eingeschaltete Relais fallen dann über `toggle_after` ab. `broken` bleibt RAM-only (2026-10-29): nach einem Neustart wird das Relais wieder angesteuert, ein bleibender Defekt erneut erkannt und erneut gemeldet — kein NVS nötig, ein defektes Relais ist ohnehin nicht steuerbar. Auffälligkeiten des Verbrauchers (Leistungsmessung: an aber 0 W, aus aber Strom) → kein `broken`, nur Mitteilung, verschwindet von selbst. `actualOn` ist der Relaiszustand, eine kaputte Birne erzeugt keine Abweichung
- **Neustart (2026-10-29)**: solange `lastDecision == 0` (seit dem Boot noch keine Entscheidung für dieses Relais), berechnet das `ApDevice` keinen Schalt-Request — `desiredOn = false` nach dem Boot ist nur Startwert, sonst würde nach jedem Neustart alles kurz aus- und wieder eingeschaltet. Gilt für jeden Mode; `GetStatus` läuft normal, Worker startet sofort. Laufende Verbraucher bleiben auf ihrem `toggle_after`, bis der Controller entscheidet. Anleitung: Taster an der Shelly nicht benutzen. **`RelayMode` wird im NVS gespeichert** (2026-10-29) — ein ohne Regel auf `FORCED_ON` gestellter Verbraucher (Heizung im Winter) wäre sonst nach jedem Neustart aus; Setter erst NVS, dann RAM (Code-Änderung steht aus)
- **Kurze Läufe über dieselbe Logik (2026-10-29)**: kein Sonderpfad — eine Beregnung „1 min an“ ist eine Regel, der Controller setzt `desiredOn = true` und nach Ablauf `false` (Laufzeit gehört zur Entscheidung, nicht zum Netzwerk-Timing); Worker schaltet nach den normalen Regeln. Sicherheit über einen kurzen `failsafe` am Relais (z.B. 90 s, Minimum 30 s). Verworfen: einzelner `Switch.Set` mit `toggle_after = Laufzeit` ohne Erneuerung — bräuchte die Laufzeit im `ApDevice`, einen Erneuerungs-Ausschluss und würde nach Ablauf wegen `desiredOn = true` erneut einschalten
- **Startwerte für die Tests (2026-10-29, Konstanten, nach Hardware-Test anpassen)** — bewusst wenige verschiedene Zeiten: Timeout pro Netzwerkaufruf 5 s; Controller-Takt 1 s; **10 s** für Fristschwelle (`lastDecision`), `RETRY_PAUSE` und `updateTime` (alle `ApDevice`s gleich); **30 s** für `OFFLINE_RETRY`, Minimum `failsafe` (Untergrenze im Setter prüfen) und Task-Watchdog. Bedingung: Fristschwelle < kleinstes `failsafe/2` (10 s < 15 s). Frische der Messwerte damit 3 × 10 s = 30 s
- **Manueller Override (2026-10-29)**: ApiWorker schreibt den Mode, die Konsequenz daraus schreibt der Controller. Verzögerung: ein Controller-Takt + ein Worker-Durchlauf. Auch ein `FORCED_ON` fällt bei hängendem Controller über `toggle_after` ab
- **Schreibrechte — jeder Wert hat genau einen Schreiber (2026-10-29)**: `RelayMode` → ApiWorker (User); `desiredOn`, `lastDecision` → Controller; `actualOn`, `lastSwitchSent`, `lastTry`, `lastSeen` → NetworkWorker; `broken` → Monitoring setzt, ApiWorker quittiert (User)
- Herleitung der einzelnen Punkte: Szenarien 1–11 in `docs/review-open-items.md`

### GET_STATUS/SWITCH-Ablauf (Stand 2026-09-16, angepasst an Schaltmodell B 2026-10-29)
- **Kein eigenes `DeviceClient`-Modul nötig** (2026-09-13 verworfen), **und kein `digestAuthRequestJson` in `DigestAuth`** (2026-09-13 ebenfalls verworfen): `DigestAuth::digestAuthRequest()` bleibt für SWITCH und GET_STATUS gleichermaßen rein transportseitig — nur Status-Code, Stream liegt am Body. Geparst wird direkt im `NetworkWorker` per `deserializeJson(doc, client)` auf den Stream, ganz ohne eigene Wartefunktion — genau wie im alten, jetzt nach `debricated/JsonParser.cpp` verschobenen `JsonParser::parseJson()`, das denselben Aufruf schon so macht und dabei nie an einem unvollständigen Body gescheitert ist
- **`WiFiManager::resolveIp(mac, buffer, len)`**: löst eine MAC zur aktuellen AP-IP auf — zustandslos, fragt bei jedem Aufruf frisch `esp_wifi_ap_get_sta_list()`/`esp_netif_get_sta_list()` ab, kein Cache. Wird direkt vor jedem Request aufgerufen
- **Kein MAC↔IP-Cache**: würde Invalidierungs-Disziplin an mehreren Fehlerpfaden voraussetzen — bewusst verworfen zugunsten von "immer frisch auflösen"
- **`esp_wifi_ap_get_sta_list()` ist für Disconnect-Erkennung ungeeignet**: kann bis zu 5 Minuten brauchen, bis ein getrenntes Gerät als "weg" erkannt wird. `online` bleibt deshalb ausschließlich über echte TCP/HTTP-Ergebnisse gesetzt (GET_STATUS/SWITCH-Fehlschlag)
- **MAC-Verifikation statt Cache-Invalidierung** (Platzierung präzisiert 2026-09-25): schützt gegen IP-Wiederverwendung (DHCP vergibt eine freigewordene IP an ein anderes Gerät) — läuft im `NetworkWorker`, **bevor** die Antwort an `ApDeviceRegistry`/`ApDevice::applyResponse()` weitergereicht wird, nicht als Teil von `applyResponse()` selbst. War beim Bau von `ApDevice`/`EntityBase` (24.09.) versehentlich in `applyResponse()` hineingewandert, ohne diesen Absatz gegenzulesen — hiermit korrigiert, `applyResponse()` bekommt nur noch bereits verifizierte Daten. Jede Shelly-Antwort enthält `sys.mac`; ein Mismatch zählt wie ein Fehlschlag
- **Ablauf pro Gerät mit Registry-Lock (2026-09-26)**: (1) Lock nehmen, `find(lock, id)`, fällige Requests/MAC/Username/Passwort auf den Stack kopieren, Lock freigeben — (2) ohne Lock und ohne Zeiger: Challenge + Switch, Challenge + GetStatus, Passwort danach per `ensureZero()` löschen — (3) Lock nehmen, Gerät per `find(lock, id)` **neu suchen** (`nullptr` → inzwischen gelöscht, Antwort verwerfen), `sys.mac` gegen die Geräte-MAC prüfen, `applyResponse()`, Lock freigeben. Lock-Dauer damit unabhängig von Netzqualität/Timeouts. Wird der Slot zwischen (1) und (3) neu belegt, scheitert schon die Digest Auth mit dem kopierten Passwort, sonst greift die MAC-Prüfung
- **Kein Ersatz für Digest Auth, sondern zusätzliche Schicht** (2026-09-25): `ha1` wird aus `user:realm:password` gebildet, `password` ist pro physischem Gerät individuell beim Onboarding generiert — ein Schaltbefehl an ein zwischenzeitlich falsches Gerät an derselben IP würde also schon an der Digest-Auth scheitern (401), außer das fremde Gerät hätte zufällig exakt dasselbe Passwort. Digest Auth ist damit die eigentliche Schranke gegen Fehlschaltungen am falschen Gerät. Die MAC-Prüfung bleibt trotzdem sinnvoll: Verteidigung in der Tiefe (Passwort-Eindeutigkeit ist eine Annahme, keine Garantie — z.B. bei manuellen Tests mit handgebautem Onboarding-JSON) und Datenkorrektheit bei GET_STATUS (ein zufällig erfolgreich authentifiziertes fremdes Gerät würde sonst fälschlich dem eigenen Device zugeschrieben)
- **MAC auch aus dem Digest-Auth-Realm verfügbar** (2026-09-25, noch nicht genutzt): Shellys WWW-Authenticate-Realm hat die Form `<produktname>-<mac>` (z.B. `realm="shellyplugsg3-8cbfeaa03350"`, siehe Beispiel in `DigestAuth.cpp`) — die MAC steckt schon in der ersten, unauthentifizierten Challenge-Antwort, vor dem zweiten Request. `DigestAuth::digestAuthRequest()` gibt `realm` aktuell nicht nach außen (rein lokale Variable, nur für `ha1` verwendet) — bräuchte einen Out-Parameter, falls die Prüfung schon vor dem authentifizierten Request laufen soll, statt erst über den Response-Body (`sys.mac`). C3-Realm-Format bewusst an Shelly angelehnt, unbestätigt bis zum ersten echten C3-Test
- **Naming (2026-09-16, korrigiert 2026-10-29)**: `deviceId` (Registry-Index eines physischen Geräts, 0–9), Entity-Position (`getEntity(idx)`, = NVS-Index `e<idx>_`, verschiebt sich nie) und `outputIdx` (Shelly-`id` für `Switch.Set?id=X`, Pflichtfeld pro Relais, muss nicht der Entity-Position entsprechen). ~~`relayId`~~ entfällt — es gibt keine Relais-Registry
- **Referenz Regel → Relais (2026-10-29)**: `deviceId` + Entity-Position. `nullptr` oder eine Entity, die kein Relais ist → Regel ungültig, nicht schalten. Erkennen einer Slot-Neubelegung bleibt offen für den Schaltregel-Sprint (siehe „Referenz-Stabilität“)
- **Abgelöst 2026-10-26**: der Controller-getriebene SWITCH-Pfad (Controller entscheidet + Timing, Befehle in Relais-Mailbox, Worker reiner Ausführer). Das Gegenargument vom 2026-09-16 (zwei Interpretationen desselben Zustands) beruhte auf dem `DataHub`-Stand; heute liegt der Zustand an genau einer Stelle (Entity unter Registry-Lock), jeder Wert hat einen Schreiber — siehe „Schaltmodell B“
- **GET_STATUS läuft geräteweit, nicht pro Relais** (2026-09-16): `Shelly.GetStatus` einmal pro Gerät statt `Switch.GetStatus?id=X` pro Ausgang; jede Relais-Entity liest ihren Teil (`switch:<outputIdx>`)
- **Auth bleibt für Relais fest auf `DigestAuth::digestAuthRequest()` verdrahtet** (2026-09-14, bewusst entschieden): kein virtueller Auth-Hook auf `RelayBase` vorerst, obwohl künftige Relaistypen (Tasmota, Shelly Gen1 — siehe „Nice to have") andere Verfahren bräuchten. Eingeschätztes Umbaurisiko: klein genug, um bei Bedarf später nachgezogen zu werden. Benutzername und Passwort kommen vom `ApDevice` (`getUsername()`/`getPassword()`), nicht als Literal im Worker — wichtig, weil der Benutzername geräteabhängig ist (Shelly: fest `admin`; eigene C3-Knoten: eigenes Schema)
- **`ApDevice::getPassword()`**: liest das Passwort aus dem NVS in einen aufrufereigenen Puffer — der Aufrufer muss den Puffer nach Gebrauch selbst mit `SecureUtils::ensureZero()` löschen, siehe RAM-Hygiene-Regel oben

### ApiWorker (ersetzt die frühere Display-Planung)
- Dedizierter FreeRTOS-Task auf Core 0, eigener HTTP-Server; horcht auf AP- und STA-Interface gleichzeitig (`0.0.0.0`-Bindung) — Standalone-Betrieb ohne Heimrouter bleibt dadurch möglich, inklusive manueller Zeiteinstellung als NTP-Fallback
- Zuständig für: Konfiguration (WLAN, Shelly/Sensor-Bindung, Schaltregeln), Monitoring (Sensorwerte, Relay-Zustände), manuelle Relay-Overrides, Log-Download aus dem NOR-Flash
- Eigene SHA-256 Digest Auth in Server-Rolle — fertige Referenzimplementierung aus dem RemoteSensor-Projekt übertragbar (siehe `learnings/mainunit-transfer-notes.md`, Abschnitt A)
- Konventionen aus dem RemoteSensor-Review direkt anwendbar, da hier zum ersten Mal ein eigener HTTP-Server auf der MainUnit entsteht: exaktes Routing (`strcmp` auf geparstem Pfad, kein `strstr`-Präfixmatch), ein benannter Handler pro Route, datengetriebenes Rendering im Frontend (siehe `learnings/stille-bugs.md` #03, `learnings/mainunit-transfer-notes.md` Abschnitt B)
- Kein direkter Zugriff auf `WiFiManager` — Overrides laufen über denselben Zustand, den der Controller ohnehin ausliest. **Entschieden 2026-10-29**: der ApiWorker schreibt nur den `RelayMode` (`setMode()` unter Registry-Lock, NVS + RAM), nie `desiredOn` — die Konsequenz aus dem Mode (`desiredOn`, `lastDecision`) schreibt der Controller im nächsten Takt. Außerdem quittiert der ApiWorker `broken` (User). Siehe „Schaltmodell B“, Schreibrechte

### WiFi-Netzwerkarchitektur
- ESP32 betreibt **Soft-AP und STA gleichzeitig** (AP+STA-Dual-Mode)
- **Soft-AP** (`ESP32_TerraControl_<ID>`): alle gesteuerten Geräte (Shellys, C3-Remote-Sensoren) verbinden sich direkt mit dem ESP32 — isoliert vom Heimnetz
- **STA**: Verbindung zum Heimrouter — für NTP-Sync, Logging, spätere externe Commands sowie GUI-Zugriff aus dem Heimnetz; bleibt dauerhaft offen
- **ApiWorker horcht auf AP und STA gleichzeitig** — die Web-GUI ist die einzige Bedienoberfläche (kein Display), muss also auch ohne Heimrouter erreichbar sein (Standalone-Betrieb); Zeit wird in diesem Fall manuell über die GUI gesetzt statt per NTP
- Kein mDNS: vor jedem Request löst `WiFiManager::resolveIp()` die MAC frisch über die AP-Client-Liste (`esp_netif_get_sta_list`) zur IP auf; MAC ist persistenter Identifier, IP wird nicht zwischengespeichert (siehe „Kein MAC↔IP-Cache"); keine WiFi-Event-Handler, kein separates `syncApClients()`
- Channel-Binding: Soft-AP und STA teilen einen Kanal (Hardware-Constraint) — Kanalwechsel des Routers führt zu kurzer Unterbrechung der AP-Clients (Reconnect in wenigen Sekunden); auf 2,4 GHz selten
- Router-Ausfall unterbricht Kernfunktion nicht — Shellys und Sensoren bleiben über ESP32-AP erreichbar
- **SSID ist der persistente Anker** für alle gebundenen Geräte (Shellys, C3-Sensoren) — SSID umbenennen trennt alle Verbindungen und erfordert vollständiges Re-Onboarding; Web-Interface muss das mit einer harten Warnung versehen
- **Mehrere MainUnits** können denselben AP-Subnetzbereich nutzen — ihre APs sind vollständig isolierte L2-Netze mit unterschiedlichen SSIDs; IP-Konflikte zwischen MainUnits sind nicht möglich
- **AP-Subnetz** (optional manuell setzbar): Standard `192.168.4.x`; einziger relevanter Konfliktfall ist Überlappung mit dem Heimnetz-Subnetz des Routers — beim STA-Connect wird das eigene Subnetz mit dem Router-Subnetz verglichen; bei Überlappung Warnung im Web-Interface; wird in NVS gespeichert
- **AP-Kanal** (optional manuell setzbar, ToDo für Schritt 10/GUI-Ausbau): Ohne STA gibt es keinen Router, dessen Kanal der ESP32 automatisch übernehmen könnte (siehe Channel-Binding oben) — der AP bleibt fest auf dem konfigurierten Kanal stehen. Bei mehreren MainUnits standalone im selben Raum sonst Kollisionsgefahr, wenn alle denselben Default-Kanal nutzen. Keine automatische Kanalsuche/-wechsel nötig (würde verbundene AP-Clients bei jedem Wechsel kurz trennen) — reicht als manuelles Feld, analog zum AP-Subnetz; GUI sollte auf die nicht überlappenden 2,4-GHz-Kanäle 1/6/11 hinweisen oder sie zur Auswahl stellen, wird in NVS gespeichert (2026-09-24 besprochen, noch nicht umgesetzt)

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
Sensoren die räumlich weit vom MainUnit entfernt sind werden an einen **ESP32-C3 Super Mini** angeschlossen, der **außerhalb** des Terrariums sitzt. Sensorkabel werden durch die Terrariumwand geführt (ca. 3mm Bohrung). Der MainUnit fragt den C3 per WiFi ab (Pull/On-Demand); der C3 liegt im Light Sleep zwischen den Abfragen. Kabelgebundene Stromversorgung — kein Akku; auch bewusste Diebstahlsicherung: ein gestohlener C3 verliert beim Abziehen sofort die Stromversorgung, kein Weiterlaufen im Zugriff eines Diebs möglich (2026-09-13). Die Steckverbindung zum Sensor folgt dem gleichen Phoenix-Contact-Schema (MSTB/MSTBT, 3-polig). Kabel zwischen C3 und Sensor: 20–30cm.

### Sensorgehäuse
Sensoren werden in individuellen 3D-gedruckten Gehäusen verbaut, die sich optisch in die Inneneinrichtung des Terrariums integrieren (z.B. als Stein, Ast, Fels).

### Validprüfung analoger Sensoren
`analogRead()` gibt immer einen Wert zurück — es gibt kein Read-Error wie bei DHT22. Validprüfung in `AnalogSensor::readValue()`:
- **Einfach**: Rail-Werte `0` und `4095` als ungültig werten (`value > 0 && value < 4095`) — deckt Kurzschluss und offenen Pin ab
- **Margin**: Nur nötig wenn der Sensor den vollen Spannungsbereich ausnutzt (z.B. Poti). Sensoren mit definiertem Arbeitsbereich (SEN0308: ~1200–3500) kommen nie in die Nähe der Rails — `> 0 && < 4095` reicht
- **Kein Schalten bei ungültigem Wert**: die Sensorklasse erkennt ungültige Werte in `readRaw()`, `readSensor()` liefert dann `false` — der Controller entscheidet nicht, `lastDecision` altert, der Failsafe läuft aus (siehe „Schaltmodell B“)

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
2. ✅ **WiFiWorker + Soft-AP** — ESP32 spannt eigenen AP auf; Shellys verbinden sich direkt; mDNS entfällt; WiFiWorker-Task mit Relay-Mailboxen + STA-Queue (Prototyp-Stand; Mailboxen inzwischen durch Schaltmodell B abgelöst); Controller-Modul für Schaltlogik (wird mit Schritt 3a neu gebaut)
3. **Remote-Sensoren** — generische Sensorwerte von externen ESP32-C3-Geräten; zunächst hardcoded Credentials; C3 im Light Sleep, wake-on-TCP; Pull/On-Demand-Modell konsistent mit verkabelten Sensoren; ein C3 ist ein `ApDevice` mit Sensor-Entities und wird über denselben Abgleich wie die Shellys abgefragt (2026-10-29, keine eigene Mailbox)
3a. **NetworkWorker / ApiWorker-Split** — WiFiWorker wird in zwei Tasks aufgeteilt: NetworkWorker (Client-Rolle, Shelly- + Remote-Sensor-Polling) und ApiWorker (Server-Rolle, eigener HTTP-Server für die Web-GUI); beide auf Core 0, ApiWorker mit niedrigerer Priorität; siehe Abschnitte NetworkWorker/ApiWorker und FreeRTOS oben
3b. **RelayBase (NVI)** — Umbau von `WifiRelay` analog zu `SensorBase` (Basis gebaut: `RelayBase`/`ShellyGen2Relay`); Basisklasse deckt reinen Schaltteil ab (an/aus, Fail-Safe-Timer via `toggle_after`-Äquivalent — Pflicht für unterstützte Relaistypen, funktioniert ohne NTP/RTC); `LedIndicator` als optionales Mixin (Multiple Inheritance) für Hersteller mit dieser Fähigkeit; `PowerMetering` dagegen als eigene Sensor-Entity am Gerät (`SensorEntityBase`, in der Shelly-Vorlage Position 1 hinter dem Relais — Arbeitsreihenfolge 2026-10-25, Schritt 6); Minimalziel: die 3 vorhandenen Shellys über das neue Pattern steuern, vor dem Onboarding-Wizard (Schritt 8). `LedIndicator` mit Nachtmodus kommt erst nach RTC/lokalem SNTP-Server (Schritt 4), da Shelly dafür eine korrekte Uhrzeit braucht — `PowerMetering` und der Fail-Safe-Timer sind davon unabhängig
4. **RTC (DS3231) + lokaler SNTP-Server** — eigener Struct, getrennt von SensorData; GUI bietet zusätzlich manuelle Zeiteinstellung als Fallback für Standalone-Betrieb ohne NTP; zusätzlich ein minimaler lokaler SNTP-Server auf der MainUnit nötig — Shellys hängen isoliert am Soft-AP ohne Internetzugang, ein öffentlicher NTP-Server ist für sie unerreichbar; Voraussetzung für `LedIndicator`s Nachtmodus (Schritt 3b)
5. **Schaltlogik** — ein Messwert pro Schaltentscheidung; zunächst hardcoded in `setup()`
6. **JSON Config Layer** — Serialisierung/Deserialisierung aller Settings; zunächst hardcoded, dann dateibasiert
7. **NOR-Flash Logging** — externer SPI-NOR (W25Q-Familie, mehrere Chips à 128 Mbit/16 MB mit eigener Chip-Select-Leitung statt einem großen adressierten Chip); monatliche Rotation, FIFO-Retention (ältester Monat weicht bei Platzmangel), GUI-Download + manuelles Löschen für gezieltes Archivieren; ersetzt die ursprünglich geplante SD-Karte vollständig (Begründung: `learnings/mainunit-transfer-notes.md`)
8. **Onboarding-Wizard** — Komfortfunktion für Endnutzer; automatisches Binden neuer Shellys und C3-Sensoren ab Werksreset; liest MAC aus Gerät, konfiguriert WiFi + Auth; setzt den NVS-Konfigurationsspeicher voraus (Config-Persistenz)
9. **NVS** — speichert letzte valide Config über Reboot
10. **GUI-Ausbau** — Konfiguration (WLAN-Credentials, MACs und Namen für Shellys/Sensoren, Schaltregeln, Schwellwerte) und Operator-Interface (Sensorwerte, Relay-Zustände, manuelle Relay-Overrides) über den ApiWorker; ersetzt die ursprünglich geplante Display/LVGL-Lösung vollständig (kein Display mehr verbaut, siehe Hardware-Abschnitt)
11. **Eigenverbrauch** — INA219 (I2C) misst Spannung, Strom und Leistung von MainUnit und C3-Nodes; Werte analog zu `PowerMetering` als Sensor-Entity abrufbar, kein zentraler Datenknoten (siehe Modul-Abschnitt); Voraussetzung für glaubwürdige Stromverbrauchs-Transparenz gegenüber Endnutzern
12. **Flash Encryption** — NVS-Verschlüsselung als erster Schritt (AES-256, Schlüssel in eFuse, Hardware-Beschleuniger); für Marktreife: Secure Boot + vollständige Flash-Verschlüsselung; verhindert Credential-Extraktion bei gestohlenen Geräten

### Sensor-Architektur
- Kein generisches `SensorEntry`-DTO (gibt es nicht) — `SensorManager` hält `std::vector<SensorBase *>`, jeder Sensor ist ein eigenes `SensorBase`-Objekt (DHT22Temperature, DHT22Humidity, SoilMoisture, ...), abgefragt über `SensorManager::readSensor(value, raw, sensorId)` (Pull)
- Registrierung analog zu Shellys: API-Funktion, zunächst in `setup()` aufgerufen, später über die GUI
- Lokale Sensoren werden direkt gelesen, kein Schreiben in einen zentralen Hub (den gibt es nicht mehr); Messwerte von C3-Knoten kommen als Entities am `ApDevice` (siehe Modul-Abschnitt `SensorManager`)
- **Gültigkeit (entschieden 2026-10-25/29)**: Prüfung pro Sensorklasse in `readRaw()` (nicht zentral), Zeitpunkt des letzten gültigen Werts über `SensorBase::getLastValid()`; bei Entities am `ApDevice`: Gerät frisch (`now - lastSeen <= 3 × updateTime`) + Gültigkeits-Flag pro Entity, kein Zeitstempel pro Entity


### Onboarding-Wizard
- Gilt für Shellys und C3-Remote-Sensoren — ein gemeinsamer Mechanismus, gerätespezifisch nur der letzte Konfigurationsschritt
- **Ablauf**: ESP32-STA verbindet temporär mit dem Factory-AP des Geräts (bekannter IP-Bereich, dokumentiert) → liest MAC automatisch via RPC → konfiguriert WiFi + Auth → Gerät verbindet sich mit ESP32-AP → MAC wird in Config gespeichert
- **Shelly-Auth (korrigiert 2026-09-16)**: Shelly Gen2/3 erlaubt keinen eigenen Account — der Digest-Auth-Benutzername ist fest `admin`, nicht änderbar (bestätigt über die Shelly-API-Doku, Andys eigene Erfahrung deckt sich damit: "hatte bis jetzt keine Chance, diesen zu ändern"). Nur das Passwort ist einstellbar, über die RPC-Methode `Shelly.SetAuth` — die erwartet aber keinen Klartext, sondern einen vorberechneten `ha1`-Hash (`SHA256(user:realm:password)`, `user` dabei zwingend `"admin"`). MainUnit generiert das Passwort, berechnet den `ha1` selbst und schickt nur den Hash an die Shelly — das Passwort bleibt nur der MainUnit bekannt, nie angezeigt. Frühere Annahme eines eigenen `terracontrol`-Accounts war falsch, nie gegen die echte API geprüft
- **Gerät und Entities in einem Schritt (Stand Code 2026-10-25, siehe „Device/Entity-Prinzip")**: `ApDeviceRegistry::create(data)` legt das Gerät (MAC/Benutzername/Passwort, einmal pro physischem Gerät) und alle Entities aus dem Onboarding-JSON an — Reihenfolge = Position im JSON (siehe „Referenz-Stabilität“). Power Strip Gen4: ein Aufruf, vier Relais-Entities. Das frühere `bind()` + `RelayRegistry::add()` pro Ausgang gibt es nicht mehr
- User hat keinen direkten Zugriff auf die Shelly — alle Konfiguration (inkl. LED) läuft ausschließlich über TerraControl
- Shelly aus dem Verbund nehmen: Factory Reset am Gerät; kein automatisches Entfernen (2026-10-24) — das Gerät erscheint als offline, der User löscht es selbst (siehe „Referenz-Stabilität“)
- **Shelly-Härtung** (automatisch beim Onboarding): Zugriffs-/Kontrollkanäle neben TerraControl werden deaktiviert — Cloud, BLE und MQTT (`Cloud.SetConfig`, `BLE.SetConfig`, `Mqtt.SetConfig`), zusätzlich Script (alle Script-Instanzen gelöscht/deaktiviert — ein Script auf der Shelly könnte sonst autonom schalten), Matter (kein Fremdzugriff über Apple/Google-Hub) und BTHome (kein BLE-Broadcast an Dritte); dazu geräteseitige `Switch.SetConfig`-Timer (`auto_on`/`auto_off`) explizit auf `false` setzen — kein Fremdzugriffsrisiko, aber Konfliktpotential mit der eigenen Schaltlogik, falls ein Vorbesitzer/Techniker die mal gesetzt hatte. `Ws` (Outbound Websocket) braucht eine explizite Ziel-URL, um etwas zu tun — reicht, die nie zu setzen, statt aktiv abzuschalten. Zusätzlich `Switch.SetConfig` → `initial_state` explizit auf `"off"` setzen, nie `"restore_last"` — nach einem Stromausfall soll nie automatisch wieder Licht/Wärme anlaufen, auch nicht kurzzeitig; das ist unabhängig vom `RelayMode` (`AUTO`/`FORCED_ON`/`FORCED_OFF`), der eine bewusste User-Übersteuerung der Schaltlogik beschreibt, kein Boot-Sicherheitsmechanismus (2026-09-01 recherchiert)
- **LED-Konfiguration**: Farbe + Helligkeit für AN/AUS-Zustand; zeitgesteuerter Nachtmodus (Zeitfenster + reduzierte Helligkeit) läuft über Shellys eigenen internen Timer, nicht aktiv von TerraControl gesteuert — nur beim Onboarding einmal konfiguriert. Voraussetzung: Shelly braucht eine korrekte Uhrzeit dafür, siehe SNTP-Punkt unten. Locate-Funktion (temporärer Blink per RPC zur physischen Identifikation)
- **Shelly braucht eigene Zeitquelle für den Nachtmodus** — Zeit kommt per SNTP (`Sys.SetConfig`, `sntp.server`), der Server kann auf eine reine IP gesetzt werden. Shellys hängen isoliert am Soft-AP ohne Internetzugang, ein öffentlicher NTP-Server ist für sie unerreichbar — die MainUnit muss deshalb selbst einen minimalen lokalen SNTP-Server anbieten und jede Shelly beim Onboarding auf die eigene AP-IP zeigen lassen, sonst bleibt die Uhr falsch/undefiniert und der Nachtmodus schaltet zur falschen Zeit. Noch nicht in der Roadmap erfasst, wird vor der LED-Anbindung nötig (2026-09-01 recherchiert)
- **RPC-Proxy** für erweiterte Shelly-Features: `shellyRPC(mac, method, params, response, len)` — TerraControl übernimmt Auth und Routing, Caller liefert Methode und Parameter als Strings
- **JSON an die Shelly ist modellunabhängig** — gleicher RPC für Plug S und Mehrfachdosen; Unterschied nur in der Anzahl der Ausgänge/Entities (ein Slot pro Switch-Komponente, gleiche MAC, unterschiedliche `id`); bestätigt durch die Shelly-API-Doku — `Switch.Set`/`Switch.GetStatus`/`Switch.SetConfig` sind bei Plug S Gen3 und Power Strip Gen4 identische Aufrufe, nur die Zahl gültiger `id`-Werte unterscheidet sich (1 vs. 4). Für den reinen Schaltteil reicht daher voraussichtlich eine gemeinsame Klasse für beide Produkte
- **RelayBase/LED/Leistungsmessung-Trennung**: LED-Steuerung und Leistungsmessung sind keine abgeschwächten Relais-Eigenschaften (anders als "keine Kalibrierung nötig" bei Sensoren), sondern orthogonale Hardware-Fähigkeiten, die nur zufällig bei manchen Geräten mitverbaut sind — deshalb kein Virtual mit No-op-Default auf `RelayBase`, sondern eigene Schnittstellen (Multiple Inheritance, z.B. `class ShellyPlugSGen3 : public RelayBase, public LedIndicator, public PowerMetering`); Zugriff über `dynamic_cast<LedIndicator*>`/`dynamic_cast<PowerMetering*>`, `nullptr` heißt "hat diese Fähigkeit nicht". Zusätzlich verschieden benannte RPC-Namespaces je Produkt für LED (`PLUGS_UI` bei Plug S Gen3, `POWERSTRIP_UI` beim Power Strip Gen4) — jede konkrete `LedIndicator`-Implementierung kapselt ihren eigenen Namespace; Leistungsdaten kommen bei Shelly direkt in der `Switch.GetStatus`-Antwort mit, kein separater Request nötig. RelayBase selbst wird zuerst gebaut, LED-/Leistungs-Anbindung kommt danach (2026-09-01). **Geändert 2026-10-25 für die Leistungsmessung**: `PowerMetering` wird kein Mixin, sondern eine eigene Sensor-Entity am Gerät (`SensorEntityBase`, Shelly-Vorlage: Relais Position 0, PowerMetering Position 1) — der Mixin-Ansatz gilt nur noch für `LedIndicator`
- **Fail-Safe-Pflicht für Relaistypen**: RelayBase bleibt architektonisch offen für andere Hersteller, aber TerraControl unterstützt nur Relaistypen mit netzwerkunabhängigem Auto-Aus (bei Shelly: `toggle_after` auf `Switch.Set`) — schützt vor dem Fall, dass die MainUnit selbst komplett ausfällt/unerreichbar wird, was die Erneuerung durch den NetworkWorker nicht abdecken kann. Kein Hersteller ohne diese Garantie wird tatsächlich onboardbar gemacht, auch wenn die Klasse technisch baubar wäre (2026-09-01)
- **Anforderung: Betrieb läuft während des Onboardings unterbrechungsfrei weiter (2026-10-28)** — zwei bekannte Hindernisse, im Onboarding-Sprint zu lösen: (1) Onboarding-TCP läuft über den NetworkWorker („alles TCP im Worker“) — blockiert es ihn, erneuert er bei keinem anderen Gerät den Failsafe; (2) STA-Verbindung zum Werks-AP der Shelly zieht den eigenen AP auf dessen Kanal (Channel-Binding) — alle AP-Clients kurz weg. Zu prüfen: Onboarding in kurzen Schritten zwischen den regulären Worker-Durchläufen. **Kriterium (2026-10-29)**: die Unterbrechung muss kürzer sein als das kleinste `failsafe/2` (z.B. Beregnung mit 90 s → 45 s); ist sie länger, fällt das Relais vorzeitig ab (sichere Richtung). Abschätzung: Worker-Blockade ~5–15 s, im Sprint messen
- C3-Onboarding: analoger Ablauf über C3-eigenen Provisioning-AP; übergibt AP-Credentials und Sensor-Konfiguration (Typ, ID, Intervall)

### Remote-Sensor-Protokoll
- Remote-Knoten (ESP32-C3) werden analog zu Shellys über ihre **MAC-Adresse** identifiziert; die IP wird vor jedem Request per `WiFiManager::resolveIp()` aufgelöst
- **Pull/On-Demand-Modell**: MainUnit fragt den C3 ab — konsistent mit verkabelten Sensoren und Shellys; kein Push
- **Digest Auth (SHA-256) auch für den C3**: jede eingehende Anfrage an den C3 (inkl. `GET /sensors`/`GET /status`) verlangt gültige Digest-Auth-Credentials, geprüft global vor dem Routing — bestätigt im echten RemoteSensor-Projekt (`HttpServer.cpp`, `DigestAuth::verify()` vor jedem Handler); Realm wird aus der Geräte-MAC abgeleitet. Die MainUnit muss den C3 also genau wie einen Shelly per Digest Auth ansprechen, nicht mit einem unauthentifizierten GET (2026-09-13)
- **Auth-Verfahren muss hier von Anfang an pro Sensor-Typ abstrahiert sein (2026-09-14, bewusster Unterschied zur Relay-Entscheidung oben)**: anders als bei Shelly gibt es hier schon heute einen konkreten, absehbaren Anwendungsfall mit komplett anderem Verfahren — kommerzielle Cloud-Sensoren wie Netatmo laufen über deren Cloud-API mit OAuth2 (Internet/STA, TLS), nicht über lokales Digest Auth am Soft-AP wie die eigenen C3-Knoten. Eigene C3-Knoten laufen als `ApDevice` über Digest Auth wie die Shellys (2026-10-29); Cloud-Sensoren wie Netatmo sind kein `ApDevice` und brauchen einen eigenen, austauschbaren Weg — beim Bau festlegen, nicht fest verdrahten. Noch nicht umgesetzt, nur als Vorgabe festgehalten
- **Antwortformat** `GET /sensors`: `{"sensor:0": {"value": 58500}, "sensor:1": {"value": 217}}` — Rohwerte; Kalibrierung und Einheit liegen im MainUnit
- **Für die C3-Überarbeitung (2026-10-25, noch nicht umgesetzt)**:
  - **Gültigkeit pro Wert in `/sensors`**: Fehlerbilder mit formal gültigem, aber falschem Wert (z.B. DHT22 ohne Versorgung → Nullbyte-Telegramm, Feuchte exakt 0,0) erkennt nur die Sensorklasse auf dem C3 — die MainUnit sieht nur den gerundeten Wert, nicht den Rohdatensatz. Der C3 muss „ungültig“ pro Wert mitliefern; Prüfung in der jeweiligen Sensorklasse (wie auf der MainUnit in `readRaw()`), nicht zentral
  - **Typ pro Wert in `/sensors`** (2026-10-24): fängt Umkonfiguration oder Firmware-Update am C3 ab — die MainUnit vergleicht mit dem erwarteten Typ der Entity, Abweichung → Entity ungültig (siehe „Referenz-Stabilität“)
- **Antwortformat** `GET /status`: `{"mac": "AA:BB:CC:DD:EE:FF", "uptime": 3600, "rssi": -65}` — MAC ist persistenter Identifier, zwingend für Onboarding
- C3 betreibt **Light Sleep** zwischen Abfragen: WiFi-Assoziation bleibt aktiv, eingehende TCP-Verbindung weckt den C3 sofort; spart Strom auch bei kabelgebundener Versorgung
- C3 sitzt **außerhalb** des Terrariums; Sensorkabel wird durch die Terrariumwand geführt; kabelgebundene Stromversorgung
- Der MainUnit fragt den Knoten ab; der Knoten trifft keine Schaltentscheidungen; Kalibrierung liegt vollständig im MainUnit
- **Voraussetzung für Außenplatzierung**: bevor ein C3-Knoten außerhalb eines nicht öffentlich zugänglichen Bereichs (z.B. Garten) platziert wird, muss Flash Encryption (Schritt 12) für dieses Gerät bereits aktiv sein — ein gestohlener, unverschlüsselter Knoten (auch im ausgeschalteten Zustand) gibt sonst das AP-Passwort der MainUnit preis, das er für die eigene Wiederverbindung zwangsläufig in NVS halten muss (2026-09-13)

### Schaltlogik
- Generisch — kennt keinen Sensortyp, nur einen `float`-Wert
- Pro Schaltentscheidung ein Messwert, ein Relay
- Parameter pro Regel: `sensorId`, `threshold`, `hysteresis`, `activeAbove` (bool)
- `threshold` ist immer der sicherheitskritische Schaltpunkt — `hysteresis` verschiebt nur die unkritische Gegenseite
- `activeAbove = true`: Relay ON wenn `value > threshold`, OFF wenn `value < threshold - hysteresis`
- `activeAbove = false`: Relay ON wenn `value < threshold`, OFF wenn `value > threshold + hysteresis`
- **Sicherheitsregel: Schaltlogik entscheidet nur mit gültigem, frischem Messwert — kein Schalten bei ungültigem Sensorwert.** Hintergrund: Ein ausgefallener Bodenfeuchtesensor darf nicht dauerhaft die Beregnungsanlage aktivieren.
- **Zeitgesteuerte Läufe** (z.B. Beregnung 1 min): ebenfalls eine Regel — der Controller hält `desiredOn` für die Laufzeit, kein Sonderpfad (siehe „Schaltmodell B“)
- **Verhalten bei ungültigem Sensorwert (präzisiert 2026-10-18, umgesetzt über `lastDecision` in Schaltmodell B, Code steht aus)**: kein sofortiges Abschalten, aber auch kein Weiterhalten — der Controller entscheidet nicht (schreibt kein `lastDecision`), der Worker **verlängert den Failsafe dadurch nicht mehr**. Ein kurzer „Schluckauf" wird so vom laufenden Failsafe überbrückt: kommen innerhalb dieser Zeit wieder gültige Werte, läuft die Regel normal weiter. Bleibt der Sensor ungültig, fällt das Relais über den Failsafe (`toggle_after`) von selbst ab, und der User bekommt eine Mitteilung (Monitoring/Notification). Ersetzt die frühere Formulierung „Relay bleibt im letzten validen Zustand, bis `online` wieder `true`" — die hätte bei laufender Heizung und ausgefallenem Sensor den Failsafe endlos verlängert

### Config-Persistenz
- **NVS ist der primäre Config-Store** — einzige Wahrheitsquelle für alle Settings
- **NOR-Flash**: ausschließlich Logging — keine Config-Datenhaltung, keine Synchronisation (ersetzt die ursprünglich geplante SD-Karte, siehe Hardware-Abschnitt)
- **Config-Update**: JSON POST via WiFi-Endpoint → Setter der jeweiligen Objekte (erst NVS, dann RAM; Muster wie `setMode()`, siehe ApiWorker-Abschnitt) — Details mit dem ApiWorker-Sprint; kein physischer Zugriff nötig
- **Config-Export**: JSON GET via WiFi-Endpoint — Download als Backup; Wiederherstellung per POST
- **NVS-Vollsicherung auf NOR-Flash** (Idee, noch nicht terminiert, 2026-09-09): zusätzlich zum JSON-Export ein roher Komplett-Snapshot der NVS-Partition (`esp_partition_read()`/`write()`) als eigener NOR-Log-Eintrag — Rollback-Möglichkeit bei NVS-Korruption, ohne vollständiges Re-Onboarding. Beide Richtungen (Sichern **und** Wiederherstellen) ausschließlich manuell vom User ausgelöst, nie automatisch/geplant — sonst Risiko, frischere legitime Änderungen versehentlich mit einer älteren Sicherung zu überschreiben. Muss auf dem NOR verschlüsselt abgelegt werden, sobald Flash Encryption (Schritt 12) steht, sonst wäre die NVS-Verschlüsselung durch eine unverschlüsselte Kopie im NOR wirkungslos
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
- Voraussetzung: REST-API-Layer (ApiWorker) und NOR-Flash (Firmware-Storage) müssen stehen
- **Firmware der Shellys** (2026-10-25, noch nicht durchdacht): am isolierten AP ohne Internet gibt es keine Shelly-Updates — Antwortstruktur ändert sich also nicht unbemerkt. Ein Update müsste die MainUnit durchreichen; zu gegebener Zeit bedenken

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
- `parseJson` — veraltet, `JsonParser::parseJson()` ist nach `debricated/` verschoben (deprecated); Nachfolgepfad ist `deserializeJson()` direkt auf den Stream, kein eigener Test dafür nötig
- `HttpClient::readLine` — `\r\n`, Buffer voll, leere Zeile (nicht mehr `HttpUtils`, siehe Modul-Abschnitt)

**Mocking**: `WiFiClient` als `FakeStream` (ist ein `Stream`)  
**Nicht mockbar**: FreeRTOS-Mutex, WiFi-Connect, AP-Client-Events

**GUI-Tests ohne Hardware**: Headless Firefox + gemockter `fetch()` — bewährter Ansatz aus dem RemoteSensor-Projekt (Syntax-Check, Funktionsscreenshots, Interferenz-Bugs reproduzierbar); relevant sobald der ApiWorker eine eigene Web-GUI bekommt. Details: `learnings/mainunit-transfer-notes.md`, Abschnitt C.

**Integrationstests** (manuell, vor jedem Merge auf `main`):
- Alle 3 Shellys verbinden sich mit ESP32-AP und werden als `online` markiert
- Unbekannte MAC wird ignoriert (kein `ApDevice` dafür angelegt)
- Schalten funktioniert (on/off), `getStatus` nach Switch bestätigt Zustand
- Shelly ziehen → TCP-Fehler → `online` wird `false` (`ApDevice::setOnline(false)`), danach Versuche nur noch alle `OFFLINE_RETRY`
- Shelly wieder einstecken → nächster Versuch nach `OFFLINE_RETRY` → `GetStatus` erfolgreich → `online = true`, Abweichung wird im nächsten Durchlauf geschaltet
- Neustart der MainUnit bei eingeschalteter Shelly → kein kurzes Aus/An (Regel `lastDecision == 0`); `FORCED_ON` überlebt den Neustart
- Kein Absturz nach >5 Minuten Laufzeit
- **Failsafe-Erneuerung** (2026-10-29): `Switch.Set on` mit `toggle_after` an ein bereits eingeschaltetes Relais startet den Timer neu — Relais mit Erneuerung bleibt dauerhaft an, ein zweites ohne Erneuerung fällt nach `toggle_after` ab. Im Prototyp mit Shelly bestätigt. Gehört zum Pflichtumfang für jeden neuen Relaistyp/Hersteller bzw. nach Firmware-/Herstellerwechsel (Shelly-Übernahme durch Schneider, weitere Entwicklung offen) — ohne diese Eigenschaft funktioniert Schaltmodell B nicht (siehe „Fail-Safe-Pflicht für Relaistypen“)
