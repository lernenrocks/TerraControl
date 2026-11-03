#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

/**
 * @brief which product a device is, decides its device-wide requests (status, later auth)
 * @note the values are stored in the NVS - never renumber or reuse one.
 *       Groups leave gaps (Shelly 1.., own nodes 32..) so new types can be added without touching old values.
 */
enum class DeviceType : uint8_t
{
    NONE = 0, /** <@brief empty slot */

    SHELLY_GEN2 = 1, /** <@brief Shelly with the Gen2/Gen3 RPC API (e.g. Plug S Gen3, Power Strip Gen4) */

    C3_NODE = 32 /** <@brief own ESP32-C3 sensor node */
};

constexpr const char *deviceTypeToString(DeviceType type)
{
    switch (type)
    {
    case DeviceType::NONE:
        return "none";
    case DeviceType::SHELLY_GEN2:
        return "Shelly Gen2/3";
    case DeviceType::C3_NODE:
        return "C3 Sensor Node";
    }
    return "unknown";
}

// JSON uses the same text as deviceTypeToString, so there is only one list of names.
// Returns NONE if the text is not a known type.
inline DeviceType deviceTypeFromString(const char *text)
{
    if (!text)
        return DeviceType::NONE;
    for (int value = 1; value <= UINT8_MAX; value++)
    {
        const DeviceType type = static_cast<DeviceType>(value);
        const char *name = deviceTypeToString(type);
        if (strcmp(name, "unknown") != 0 && strcmp(name, text) == 0)
            return type;
    }
    return DeviceType::NONE;
}

/**
 * @brief writes the device-wide status request (URI without host) for a device type into buffer
 * @return false if the type has no status request yet or the buffer is too small
 */
inline bool deviceTypeToStatusRequest(DeviceType type, char *buffer, size_t len)
{
    const char *request = nullptr;
    switch (type)
    {
    case DeviceType::SHELLY_GEN2:
        request = "rpc/Shelly.GetStatus";
        break;
    case DeviceType::NONE:
    case DeviceType::C3_NODE: // not implemented yet
        break;
    }
    if (!request || !buffer)
        return false;
    int written = snprintf(buffer, len, "%s", request);
    return written > 0 && static_cast<size_t>(written) < len;
}
