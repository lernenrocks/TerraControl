#pragma once

// Onboarding templates for products with fixed hardware: the device type and its entities.
// MAC, username and password differ per device - the onboarding (later the wizard) adds them
// before the JSON goes to ApDeviceRegistry::create(). One template per product, not per
// DeviceType: SHELLY_GEN2 covers the Plug S as well as the Power Strip.
//
// The position of an entity in "entities" becomes its NVS index (e<idx>_ keys), and switching
// rules refer to it - only ever append at the end, never reorder or remove an entry.
// Type texts must match deviceTypeToString() / entityTypeToString().
//
// inline constexpr: one copy in flash, no matter how many .cpp files include this header.
namespace ApDeviceTemplates
{
    namespace Shelly
    {
        // Shelly Plug S Gen3: one switch output with power metering
        inline constexpr char PLUG_S_GEN3[] = R"json({
  "type": "Shelly Gen2/3",
  "entities": [
    { "type": "Shelly Gen3 Plug S", "name": "Relay 1",       "output": 0 },
    { "type": "Power",              "name": "Power 1",       "source": "switch:0/apower" },
    { "type": "Energy",             "name": "Energy 1",      "source": "switch:0/aenergy/total" },
    { "type": "Temperature",        "name": "Temperature 1", "source": "switch:0/temperature/tC" },
    { "type": "Uptime",             "name": "Uptime",        "source": "sys/uptime" },
    { "type": "RSSI",               "name": "RSSI",          "source": "wifi/rssi" }
  ]
})json";
    }
}
