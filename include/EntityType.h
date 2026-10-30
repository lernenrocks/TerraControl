#pragma once

#include <cstdint>
#include <cstring>

/**
 * @brief what an entity is, decides which class is built when a device starts
 * @note the values are stored in the NVS - never renumber or reuse one.
 *       Groups leave gaps (relay 1.., sensor 32.., system 64..) so new types can be added without touching old values.
 */
enum class EntityType : uint8_t
{
    NONE = 0, /** <@brief empty slot */

    SHELLY_GEN2_RELAY = 1,  /** <@brief switch output of a Shelly Gen2 */
    SHELLY_GEN3_RELAY = 2,  /** <@brief switch output of a Shelly Gen3 */
    SHELLY_GEN3_PLUG_S = 3, /** <@brief switch output of a Shelly Gen3 Plug S */
    DHT22_TEMPERATURE = 32, /** <@brief Temperature from DHT22 Sensor */
    DHT22_HUMIDITY = 33,    /** <@brief Humidity from DHT22 Sensor */
    SOIL_MOISTURE = 34,     /** <@brief capacitive Soil Moisture Sensor */

    // measurements of an ApDevice (ApDeviceSensor) - one type per measured quantity,
    // the type decides the default unit and precision
    POWER = 40,       /** <@brief active power in W (Shelly switch:N/apower) */
    ENERGY = 41,      /** <@brief energy counter in Wh (Shelly switch:N/aenergy/total) */
    TEMPERATURE = 42, /** <@brief temperature in °C (Shelly switch:N/temperature/tC) */
    UPTIME = 43,      /** <@brief device uptime in s (Shelly sys/uptime) */
    RSSI = 44,        /** <@brief WiFi signal strength in dBm (Shelly wifi/rssi) */

    SYSTEM_TEMPERATURE = 64, /** <@brief temperature of the MainUnit itself */
    SYSTEM_HEAP_FREE = 65,   /** <@brief free heap */
    SYSTEM_NVS_FREE = 66,    /** <@brief free space in the NVS */
    SYSTEM_NOR_FREE = 67,    /** <@brief free space in the NOR flash (logging) */

    RTC_TEMPERATURE = 90 /** <@brief temperature of the rtc */
};

constexpr const char *entityTypeToString(EntityType type)
{
    switch (type)
    {
    case EntityType::NONE:
        return "none";
    case EntityType::SHELLY_GEN2_RELAY:
        return "Shelly Gen2 Relay";
    case EntityType::SHELLY_GEN3_RELAY:
        return "Shelly Gen3 Relay";
    case EntityType::SHELLY_GEN3_PLUG_S:
        return "Shelly Gen3 Plug S";
    case EntityType::DHT22_TEMPERATURE:
        return "DHT22 Temperature";
    case EntityType::DHT22_HUMIDITY:
        return "DHT22 Humidity";
    case EntityType::SOIL_MOISTURE:
        return "Soil Moisture";
    case EntityType::POWER:
        return "Power";
    case EntityType::ENERGY:
        return "Energy";
    case EntityType::TEMPERATURE:
        return "Temperature";
    case EntityType::UPTIME:
        return "Uptime";
    case EntityType::RSSI:
        return "RSSI";
    case EntityType::SYSTEM_TEMPERATURE:
        return "System Temperature";
    case EntityType::SYSTEM_HEAP_FREE:
        return "Free Heap";
    case EntityType::SYSTEM_NVS_FREE:
        return "Free NVS";
    case EntityType::SYSTEM_NOR_FREE:
        return "Free NOR Flash";
    case EntityType::RTC_TEMPERATURE:
        return "Clock Temperature";
    }
    return "unknown";
}

// JSON uses the same text as entityTypeToString, so there is only one list of names.
// Returns NONE if the text is not a known type.
inline EntityType entityTypeFromString(const char *text)
{
    if (!text)
        return EntityType::NONE;
    for (int value = 1; value <= UINT8_MAX; value++)
    {
        const EntityType type = static_cast<EntityType>(value);
        const char *name = entityTypeToString(type);
        if (strcmp(name, "unknown") != 0 && strcmp(name, text) == 0)
            return type;
    }
    return EntityType::NONE;
}

constexpr const char *getDefaultUnit(EntityType type)
{
    switch (type)
    {
    case EntityType::NONE:
    case EntityType::SHELLY_GEN2_RELAY:
    case EntityType::SHELLY_GEN3_RELAY:
    case EntityType::SHELLY_GEN3_PLUG_S:
        return "";
    case EntityType::DHT22_TEMPERATURE:
    case EntityType::SYSTEM_TEMPERATURE:
    case EntityType::TEMPERATURE:
    case EntityType::RTC_TEMPERATURE:
        return "°C";
    case EntityType::DHT22_HUMIDITY:
    case EntityType::SOIL_MOISTURE:
        return "%";
    case EntityType::POWER:
        return "W";
    case EntityType::ENERGY:
        return "Wh";
    case EntityType::UPTIME:
        return "s";
    case EntityType::RSSI:
        return "dBm";
    case EntityType::SYSTEM_HEAP_FREE:
        return "B";
    case EntityType::SYSTEM_NVS_FREE:
        return "entries";
    case EntityType::SYSTEM_NOR_FREE:
        return "B";
    }
    return "";
}
constexpr int getDefaultPrecision(EntityType type)
{
    switch (type)
    {
    case EntityType::NONE:
    case EntityType::SHELLY_GEN2_RELAY:
    case EntityType::SHELLY_GEN3_RELAY:
    case EntityType::SHELLY_GEN3_PLUG_S:
    case EntityType::UPTIME:
    case EntityType::RSSI:
    case EntityType::SYSTEM_HEAP_FREE:
    case EntityType::SYSTEM_NVS_FREE:
    case EntityType::SYSTEM_NOR_FREE:
        return 0;
    case EntityType::DHT22_TEMPERATURE:
    case EntityType::SYSTEM_TEMPERATURE:
    case EntityType::TEMPERATURE:
    case EntityType::RTC_TEMPERATURE:
    case EntityType::DHT22_HUMIDITY:
    case EntityType::SOIL_MOISTURE:
    case EntityType::POWER:
    case EntityType::ENERGY:
        return 1;
    }
    return 0;
}
