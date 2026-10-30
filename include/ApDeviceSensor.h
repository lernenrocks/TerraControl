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
class ApDeviceSensor final : public EntityBase
{
public:
    ~ApDeviceSensor() = default;

    bool read(float &value, bool raw);
    bool reset();
    static ApDeviceSensor *onboard(const char *nvsNamespace, const uint8_t idx, EntityType type, JsonObjectConst data);
    static ApDeviceSensor *recover(const char *nvsNamespace, const uint8_t idx, EntityType type);
    float getScale() const;
    bool setScale(float scale);
    float getOffset() const;
    bool setOffset(float offset);
    int getPrecision() const;
    bool setPrecision(int precision);
    const char *getUnit() const;
    bool setUnit(const char *unit);
    bool applyResponse(JsonObjectConst data) override;

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
