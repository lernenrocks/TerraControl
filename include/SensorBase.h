#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"

constexpr const size_t UNIT_LEN = 16;
constexpr const size_t NAME_LEN = 32;
constexpr const char VALUE_KEY_SCALE[] = "vScale";
constexpr const char VALUE_KEY_OFFSET[] = "vOffset";
constexpr const char VALUE_KEY_UNIT[] = "vUnit";
constexpr const char VALUE_KEY_PRECISION[] = "vPrecision";
constexpr const char SENSOR_NAME_KEY[] = "sensorName";

/** @brief Abstract base class for all sensors */
class SensorBase
{
public:
    SensorBase(uint8_t id, const char *type);
    virtual ~SensorBase() = default;

    bool read(float &value, bool raw);

    virtual bool getCalibrationJson(char *buffer, size_t len) = 0;
    virtual bool getCalibrationValuesJson(char *buffer, size_t len) = 0;
    virtual bool calibrate(JsonObjectConst data) = 0;
    virtual void reset() = 0;

    const char *getType() const;

    void getName(char *buffer, size_t len);
    bool setName(const char *name, size_t len);

    bool getScale(float &scale) const;
    bool setScale(float scale);
    bool getOffset(float &offset) const;
    bool setOffset(float offset);
    bool getPrecision(int &precision) const;
    bool setPrecision(int precision);
    bool getUnit(char *buffer, size_t len) const;
    bool setUnit(const char *unit);

protected:
    void nvsNamespace(char *buffer, size_t len) const;

private:
    uint8_t _id;
    const char *_type;
    virtual bool isValid() = 0;
    virtual void readRaw(float &buffer) = 0;
    void ensureDefaults();
};
