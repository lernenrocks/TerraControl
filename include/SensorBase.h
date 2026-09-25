#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"

constexpr const size_t SENSOR_UNIT_LEN = 16;
constexpr const size_t SENSOR_NAME_LEN = 32;
constexpr const float DEFAULT_SCALE = 1.0f;
constexpr const float DEFAULT_OFFSET = 0.0f;
constexpr const char CONFIG_KEY_SCALE[] = "scale";
constexpr const char CONFIG_KEY_OFFSET[] = "offset";
constexpr const char CONFIG_KEY_UNIT[] = "unit";
constexpr const char CONFIG_KEY_PRECISION[] = "precision";
constexpr const char CONFIG_KEY_SENSOR_NAME[] = "name";
constexpr const char CONFIG_KEY_SENSOR_TYPE[] = "type";
constexpr const char CONFIG_KEY_SENSOR_ID[] = "id";

/** @brief Abstract base class for all sensors */
class SensorBase
{
public:
    SensorBase(uint8_t sensorId, const char *type, int defaultPrecision, const char *defaultUnit);
    virtual ~SensorBase() = default;

    bool read(float &value, bool raw);

    virtual bool getCalibrationJson(char *buffer, size_t len) = 0;
    virtual bool getCalibrationValuesJson(char *buffer, size_t len) = 0;
    virtual bool calibrateSensorHardware(JsonObjectConst data) = 0;
    virtual void reset() = 0;
    virtual const char *getDefaultUnit() const = 0;
    virtual int getDefaultPrecision() const = 0;
    const char *getName() const;
    bool setName(const char *name);
    float getScale() const;
    bool setScale(float scale);
    float getOffset() const;
    bool setOffset(float offset);
    int getPrecision() const;
    bool setPrecision(int precision);
    const char *getUnit() const;
    bool setUnit(const char *unit);

    const char *getType() const;
    uint8_t getSensorId() const;

protected:
    void nvsNamespace(char *buffer, size_t len) const;

private:
    uint8_t _sensorId;
    const char *_type;
    char _name[SENSOR_NAME_LEN] = {};
    float _scale = DEFAULT_SCALE;
    float _offset = DEFAULT_OFFSET;
    int _precision = 0;
    char _unit[SENSOR_UNIT_LEN] = {};
    virtual bool readRaw(float &buffer) = 0;
    void ensureDefaults(int defaultPrecision, const char *defaultUnit);
};
