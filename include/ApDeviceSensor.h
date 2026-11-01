#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"
#include "SensorConfigKeys.h"
#include "EntityType.h"
#include "EntityBase.h"

constexpr const uint8_t SENSOR_SOURCE_LEN = 32;
constexpr const char CONFIG_KEY_ENTITY_SOURCE[] = "source";
static_assert(sizeof(CONFIG_KEY_ENTITY_SOURCE) - 1 <= ENTITY_KEY_NAME_MAX, "entity key name too long");

// One measured value of an ApDevice (Shelly: power, energy, temperature, uptime, RSSI;
// C3 node: one sensor). A single class for all of them - they differ only in data:
// the EntityType (default unit/precision), the name and the source path from the
// onboarding template, e.g. "switch:0/aenergy/total" or "sensor:0".
//
// Not related to SensorBase (local sensors), even though both offer the same read():
// - push instead of pull: the NetworkWorker hands the device response to applyResponse(),
//   which walks the source path and stores value + validity in RAM - read() only returns it
// - lives in the device's NVS namespace ("e<idx>_" keys), not in the "sensors" namespace
// - scale/offset/precision/unit are stored only once the user sets them; a missing key
//   means the default of the type
class ApDeviceSensor final : public EntityBase
{
public:
    ~ApDeviceSensor() = default;

    bool read(float &value, bool raw);
    bool reset();
    static ApDeviceSensor *onboard(const char *nvsNamespace, const uint8_t idx, EntityType type, JsonObjectConst data);
    static ApDeviceSensor *recover(const char *nvsNamespace, const uint8_t idx, EntityType type);
    bool applyResponse(JsonObjectConst data) override;
    float getScale() const;
    bool setScale(float scale);
    float getOffset() const;
    bool setOffset(float offset);
    int getPrecision() const;
    bool setPrecision(int precision);
    const char *getUnit() const;
    bool setUnit(const char *unit);
    

private:
    ApDeviceSensor(const char *nvsNamespace, const uint8_t idx, EntityType type);
    bool write(JsonObjectConst data) override;
    bool load() override;
    float _value = 0.0f;
    bool _valid = false;
    float _scale = DEFAULT_SCALE;
    float _offset = DEFAULT_OFFSET;
    int _precision = 0;
    char _unit[SENSOR_UNIT_LEN] = {};
    char _source[SENSOR_SOURCE_LEN]={};
};
