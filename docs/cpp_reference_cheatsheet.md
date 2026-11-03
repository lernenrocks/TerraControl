# C++ Parameter- / Objekt-Access Cheatsheet

## 1) Call-by-Value vs Call-by-Reference vs Pointer

### Call-by-Value
`void f(WiFiStatus x)`
- beim Aufruf wird `x` kopiert (tief für Plain-Old-Data)
- Änderungen in `f` wirken nicht auf den Caller
- TTL: läuft im Scope von `f`

### Call-by-Reference
`void f(WiFiStatus &x)`
- `x` ist Alias auf Originalobjekt
- Änderungen wirken im Aufrufer sichtbar
- `x` kann nicht null sein

### Call-by-Pointer
`void f(WiFiStatus *x)`
- `x` ist Adresse (`nullptr` möglich)
- Zugriff via `x->...`
- ggf. `if(!x) return;`

## 2) Struct member assignment vs Copy

```cpp
WiFiStatus a, b;
a = b; // Werte von b in a kopieren (für POD memberwise)
```

- kein Call-by-Value hier, sondern Zuweisung (assignment)!
- Bools, int etc. werden Feld-für-Feld kopiert

## 3) FreeRTOS Lock-Timeout

`xSemaphoreTake(mutex, pdMS_TO_TICKS(100))`
- `pdTRUE` = Lock erhalten
- `pdFALSE` = Timeout (hier 100ms)
- `portMAX_DELAY` = unbegrenzt warten (nicht für UI empfohlen)

## 4) Pointer vs Referenz Key-Differenzen
- Referenz: muss gebunden sein, kann nicht null, kein Rebind
- Pointer: kann null, kann auf anderes Objekt zeigen, pointer arithmetic möglich

## 5) Empfehlung
- Kapsle Daten in API (`get*/set*` / `apply*`)
- Nutze defaults im Struct (z.B. `Status=UNKNOWN`)
- C++ ist nicht Java: Verhalten (Kopie vs Ref) muss immer explizit sein
