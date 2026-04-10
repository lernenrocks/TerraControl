# Integration Tests — TerraControl

To be performed manually before every merge to `main`.  
Test setup: 3 Shelly Gen2/3 devices with hardcoded MACs, ESP32 in local network.

---

## Checklist

### 1. mDNS Discovery

**Precondition:** All 3 Shellys plugged in and connected to WiFi.

- [x] Start ESP32 — All 3 Shellys found via mDNS and registered in DataHub
- [x] Check Serial log — No `[ERROR]` during discovery

---

### 2. MAC Verification

**Precondition:** One Shelly with unknown MAC in the network (e.g. temporarily add a foreign Shelly).

- [x] ESP32 attempts to contact unknown Shelly — MAC check fails, device is not added to DataHub
- [x] Check Serial log — device shows as `OFFLINE / never seen`, not switched

---

### 3. Switching

**Precondition:** All 3 Shellys found via mDNS, `online == true`.

- [x] Switch relay on (`turn=on`) — Shelly switches on, `getStatus()` confirms `output == true`
- [x] Switch relay off (`turn=off`) — Shelly switches off, `getStatus()` confirms `output == false`
- [x] Switch all 3 Shellys in sequence — No connection errors, all respond with HTTP 200

---

### 4. Stability Test

**Precondition:** Normal operation, all Shellys online.

- [x] Run ESP32 for >5 minutes — No crash, no watchdog reset (tested 3h)
- [x] Check Serial log after 5 minutes — No unexpected `[ERROR]`
- [x] Compare `[HEAP]` entries in Serial log — Free heap remains stable — no continuous decline

---

## Not yet tested (deliberately omitted)

**Offline/Online cycle** (unplugging and replugging a Shelly) — will be documented once the user-facing Shelly management is stable.
