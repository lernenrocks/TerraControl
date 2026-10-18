#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"
#include "SensorConfigKeys.h"
#include "EntityType.h"
#include "EntityBase.h"
// No CONFIG_KEY_SENSOR_TYPE/CONFIG_KEY_SENSOR_ID here - those are SensorBase's own
// id/type tracking; this class will get its index/type from EntityBase/ApDevice instead.

/** @brief Abstract base class for all sensors */
class ApDeviceSensor final: public EntityBase
{
public:
    ApDeviceSensor(const char *nvsNamespace, const uint8_t idx, const char *type, int defaultPrecision, const char *defaultUnit);
    ~ApDeviceSensor() = default;

    bool read(float &value, bool raw);

    void reset();
    const char *getDefaultUnit() const;
    int getDefaultPrecision() const;
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
    float _scale = DEFAULT_SCALE;
    float _offset = DEFAULT_OFFSET;
    int _precision = 0;
    char _unit[SENSOR_UNIT_LEN] = {};
    void ensureDefaults(int defaultPrecision, const char *defaultUnit);
};
