# Übertragbarkeit RemoteSensor → MainUnit

Zusammenfassung der Konventionen und Learnings aus diesem Projekt, gruppiert danach, wie direkt sie auf die MainUnit übertragbar sind. Bug-Muster im Detail: [stille-bugs.md](stille-bugs.md).

---

## A. Firmware-Konventionen (allgemein, wahrscheinlich 1:1 übertragbar)

- **Heap-Disziplin**: kein `String`/`malloc`/`DynamicJsonDocument`, stattdessen `char[]` + `snprintf`/`strlcpy`, `StaticJsonDocument`. `new` nur für Objekte, die die gesamte Laufzeit leben (keine zyklische Allokation).
- **RAII statt manueller Paarung** für alles, was zwingend zusammen auf-/abgebaut werden muss (`InternalStorage::Session` als Vorlage für NVS-Zugriffe) — verhindert vergessenes `end()` in Fehlerpfaden.
- **NVI-Pattern** für Geräte-/Sensorabstraktionen (`SensorBase`: `read()` public non-virtual, `isValid()`/`readRaw()` private pure virtual) — falls die MainUnit eine eigene Aktor-/Sensor-Abstraktion braucht, gleiches Muster sinnvoll.
- **Modultrennung nach Abhängigkeit, nicht nach Thema** — `DigestCrypto`/`DigestHeaderParser`/`DigestAuth`/`System`/`JsonEscape` sind getrennt, weil sie unterschiedliche Abhängigkeiten haben (mbedtls/WiFi/NVS/nichts), nicht weil's "anders klingende" Module sind. Kriterium fürs Aufteilen: eine Datei braucht eine Abhängigkeit, die eine andere nicht braucht.
- **Keine verfrühte Abstraktion** — Gemeinsamkeiten erst hochziehen, wenn ein zweites echtes Beispiel existiert (Beispiel `HX711Sensor::_pId`, bewusst nicht auf `SensorBase` verallgemeinert).
- **Gekoppelte Puffergrößen aus einer gemeinsamen Konstante ableiten**, nie zwei unabhängige Zahlen für dieselben Daten pflegen (Cheatsheet #4).
- **Truncation vs. Ablehnung**: Längen-Validierung *vor* dem Schreiben, ablehnen statt hinterher still kürzen (Cheatsheet #1) — gilt für jedes Freitextfeld (Namen, Passwörter, o. ä.).
- **Wortgrenzen-Prüfung bei String-Suche**, wenn ein Feldname Präfix/Suffix eines anderen sein kann (Cheatsheet #2, der `nonce`/`cnonce`-Bug) — relevant für jeden handgeschriebenen Parser.
- **Exaktes Routing** (`strcmp` auf schon geparstem Pfad) statt `strstr`-Präfixmatch auf dem Rohheader (Cheatsheet #3) — direkt relevant, falls die MainUnit einen eigenen HTTP-Server fährt.
- **`DEBUG_LOG(...)`-Makro** statt verstreuter `#ifdef`/`Serial.printf`/`#endif`-Blöcke — Makro, nicht Funktion, damit im Release-Build wirklich nichts übrig bleibt.
- **Ein Handler pro Route**, als eigene benannte Funktion — hält `handle()` kurz, unabhängig von der Zahl der Endpoints.
- **Digest Auth (SHA-256), MAC im Realm** — falls die MainUnit ihre eigene GUI jetzt auch schützen will (siehe unten, headless-Entscheidung), ist das hier komplett fertige, hardwaregetestete Referenzimplementierung, kein Neuentwurf nötig.

## B. GUI-Konventionen (JS/HTML — direkt relevant, da die MainUnit jetzt auch web-GUI-first wird)

- **Datengetrieben rendern** — GUI baut sich aus einer JSON-Response (`/calibrationinfo`) auf, kein hardcodiertes Wissen über Sensortypen/Anzahl im JS.
- **Kleinstmögliche Neu-Render-Fläche**: nicht die ganze Karte/Seite neu aufbauen, wenn nur ein Feld sich ändert — eigener State-Container pro unabhängig bearbeitbarem Feld (Lehre aus dem Name/Offset-Interferenz-Bug von heute Abend: ein Toggle hat ein anderes, gerade offenes Feld überschrieben, weil beide aus demselben vollständigen Re-Render kamen).
- **Neutrale Platzhalter statt falscher Default-Werte** — leer/`…` statt eines konkreten (kurzzeitig falschen) Namens, sonst "zuckt" die Anzeige beim Laden.
- **`document.title` und Favicon live aktualisieren** — beides unproblematisch per JS setzbar, kein Grund, das auszulassen.
- **Digest Auth im Browser läuft transparent** — kein JS-seitiger Nachbau möglich/nötig, Browser zeigt eigenen Login-Dialog, Zugangsdaten werden pro Origin automatisch mitgeschickt.
- **Für Weg 2 (embed_txtfiles) nicht mehr nötig**: die "Extended Embedded Languages"-VSCode-Extension, die hier für Syntax-Highlighting in `R"html(...)html"`-Strings genutzt wurde — bei echten `.css`/`.js`-Dateien via `embed_txtfiles` gibt's das Problem gar nicht erst.

## C. Test-/Verifikations-Workflow (Technik, nicht Konvention — aber praktisch bewährt)

- **Headless Firefox + gemockter `fetch()`** zum Testen der GUI ohne Hardware — mehrfach heute genutzt (Syntax-Check, Funktionsscreenshots, Interferenz-Bug reproduziert). Kein Node.js in dieser Umgebung verfügbar, Firefox war der verlässliche Ersatz.
- **`window.onerror`-Marker-Trick** für JS-Syntax-Checks ohne echten Parser/Node — eine Zeile am Ende desselben `<script>`-Blocks, die nur läuft, wenn alles davor sauber geparst hat.
- **PlatformIO `[env:native]` + `default_envs`** — reine Logik-Module (keine WiFi-/NVS-Abhängigkeit) hostnativ testbar, ohne dass `pio run`/IDE-Buttons den Test-Env aus Versehen mitziehen.

## D. MainUnit-spezifische Entscheidungen aus dem heutigen Gespräch (keine C3-Konvention, sondern schon MainUnit-Planung)

- **Headless-Entscheidung**: MainUnit bekommt kein Display mehr — die hier entwickelte Web-GUI hat sich als flexibel/brauchbar genug erwiesen, um die primäre Oberfläche zu sein. Spart auch Flash-Platz (keine Display-GUI-Lib).
- **HTML/CSS/JS-Einbettung**: bewusst Weg 2 (`board_build.embed_txtfiles`, echte Dateien) statt der hier genutzten Raw-String-Technik — mehr Tooling, dafür etwas mehr Umstellungsaufwand (`client.write(start, end-start)` statt `client.print()`, da nicht automatisch nullterminiert).
- **Log-Speicher**: externer SPI-NOR-Flash (W25Q-Familie), nicht NAND — bei ca. 5–10 MB/Jahr (10-Minuten-Intervall, Sensorwerte + Schaltzustände) ist NAND weder günstiger (echte Marktpreise: ~9–11 USD für 1-Gbit-SPI-NAND bei LCSC, ab 38 Stück) noch nötig, dafür aber komplexer (Bad-Block-Management, ECC, LittleFS hat keine ausgereifte NAND-Unterstützung). Bis 16 MB (128 Mbit) keine Adressierungs-Sonderfälle, darüber (32 MB+) braucht's 4-Byte-Adressierung — vorher prüfen, ob die genutzte Lib das unterstützt.
- **Mehrere NOR-Chips statt einem großen**: separate Chip-Select-Leitung pro Chip am selben SPI-Bus (MOSI/MISO/SCK geteilt) — günstiger und einfacher als ein einzelner großer/teurerer Chip. Zwei Wege, das Software-seitig zu nutzen: vereinheitlichte Block-Device-Schicht für LittleFS, oder (einfacher, passend zum Append-only-Log) einfach umschalten, welcher Chip gerade aktiv beschrieben wird.
- **Log-Rotation**: monatlich statt täglich (aktuelle SD-Praxis) — bei mehrjähriger Aufbewahrung deutlich weniger Dateien für die GUI-Liste (12/Jahr statt 365/Jahr).
- **Retention-Policy**: automatische FIFO-Rotation (ältester Monat wird gelöscht, wenn Platz für einen neuen gebraucht wird) als Sicherheitsnetz, GUI-Download+manuelles Löschen zusätzlich für gezieltes Archivieren — nicht rein manuell, sonst braucht's Nutzerpflege, damit das Gerät nicht "volläuft".
