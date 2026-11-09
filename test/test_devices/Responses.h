#pragma once
// Device responses as the NetworkWorker will hand them to applyResponse().

// Shelly Plug S Gen3, Shelly.GetStatus (FW 1.7.x), recorded from a real device
// (../terraControll/esp32/terraSteuerungShelly/data/shelly.json). Changed for the tests:
// MAC, IPs, SSID/BSSID replaced; apower and aenergy.total set to non-zero values, so a test
// can tell a read value from a default 0.
inline constexpr char PLUG_S_GEN3_STATUS[] = R"json({
    "ble": {},
    "bthome": { "errors": [ "bluetooth_disabled" ] },
    "cloud": { "connected": false },
    "knx": {},
    "matter": { "num_fabrics": 0, "commissionable": false },
    "mqtt": { "connected": false },
    "plugs_ui": {},
    "switch:0": {
        "id": 0,
        "source": "init",
        "output": false,
        "apower": 12.5,
        "voltage": 237.9,
        "freq": 50.0,
        "current": 0.054,
        "aenergy": { "total": 345.678, "by_minute": [ 0.000, 0.000, 0.000 ], "minute_ts": 1774641120 },
        "ret_aenergy": { "total": 0.000, "by_minute": [ 0.000, 0.000, 0.000 ], "minute_ts": 1774641120 },
        "temperature": { "tC": 43.4, "tF": 110.2 }
    },
    "sys": {
        "mac": "8CBFEAA03350",
        "restart_required": false,
        "time": "20:52",
        "unixtime": 1774641146,
        "last_sync_ts": 1774640630,
        "uptime": 16887,
        "ram_size": 259572,
        "ram_free": 140788,
        "ram_min_free": 121844,
        "fs_size": 917504,
        "fs_free": 458752,
        "cfg_rev": 12,
        "kvs_rev": 0,
        "schedule_rev": 0,
        "webhook_rev": 0,
        "btrelay_rev": 0,
        "bthc_rev": 0,
        "available_updates": { "stable": { "version": "1.7.5" } },
        "reset_reason": 4,
        "utc_offset": 3600
    },
    "wifi": {
        "sta_ip": "192.168.4.2",
        "status": "got ip",
        "ssid": "ESP32_TerraControl_1",
        "bssid": "aa:bb:cc:dd:ee:ff",
        "rssi": -40
    },
    "ws": { "connected": false }
})json";

// C3 sensor node, GET /sensors (format from the C3 code, SensorManager.cpp:122)
inline constexpr char C3_SENSORS[] = R"json({
    "sensor:0": { "value": 58500, "valid": true },
    "sensor:1": { "value": null, "valid": false }
})json";
