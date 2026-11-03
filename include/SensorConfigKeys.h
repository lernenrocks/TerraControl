#pragma once

// Shared between SensorBase (local, wired sensors) and RelaySensorBase (AP-bound remote
// sensors) - pure data, no shared behavior/base class. Safe to reuse the same key strings
// across both: each prefixes them with its own id/index ("s<id>_" / "e<idx>_"), so there is
// no collision risk.

// An NVS key has at most 15 characters. The longest prefix is "s254_"/"e254_" (5), so a key
// name may have 10. Every key constant gets a static_assert - a too long name is a compile
// error instead of a silently truncated key at runtime.
constexpr const size_t SENSOR_KEY_NAME_MAX = 10;

constexpr const size_t SENSOR_UNIT_LEN = 16;
constexpr const size_t SENSOR_NAME_LEN = 32;
constexpr const float DEFAULT_SCALE = 1.0f;
constexpr const float DEFAULT_OFFSET = 0.0f;
constexpr const char CONFIG_KEY_SCALE[] = "scale";
static_assert(sizeof(CONFIG_KEY_SCALE) - 1 <= SENSOR_KEY_NAME_MAX, "sensor key name too long");
constexpr const char CONFIG_KEY_OFFSET[] = "offset";
static_assert(sizeof(CONFIG_KEY_OFFSET) - 1 <= SENSOR_KEY_NAME_MAX, "sensor key name too long");
constexpr const char CONFIG_KEY_UNIT[] = "unit";
static_assert(sizeof(CONFIG_KEY_UNIT) - 1 <= SENSOR_KEY_NAME_MAX, "sensor key name too long");
constexpr const char CONFIG_KEY_PRECISION[] = "precision";
static_assert(sizeof(CONFIG_KEY_PRECISION) - 1 <= SENSOR_KEY_NAME_MAX, "sensor key name too long");
constexpr const char CONFIG_KEY_SENSOR_NAME[] = "name";
static_assert(sizeof(CONFIG_KEY_SENSOR_NAME) - 1 <= SENSOR_KEY_NAME_MAX, "sensor key name too long");
