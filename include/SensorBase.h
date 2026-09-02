#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"

constexpr const size_t SENSOR_UNIT_LEN = 16;
constexpr const size_t SENSOR_NAME_LEN = 32;
constexpr const float DEFAULT_SCALE = 1.0f;
constexpr const float DEFAULT_OFFSET = 0.0f;
constexpr const char CONFIG_KEY_SCALE[] = "configScale";
constexpr const char CONFIG_KEY_OFFSET[] = "configOffset";
constexpr const char CONFIG_KEY_UNIT[] = "configUnit";
constexpr const char CONFIG_KEY_PRECISION[] = "configPrecision";
constexpr const char CONFIG_KEY_NAME[] = "configName";
constexpr const char CONFIG_KEY_TYPE[] = "configType";
constexpr const char CONFIG_KEY_ID[] = "id";

/** @brief Abstract base class for all sensors */
class SensorBase
{
public:
    SensorBase(uint8_t id, const char *type);
    virtual ~SensorBase() = default;

    bool read(float &value, bool raw);

    virtual bool getCalibrationJson(char *buffer, size_t len) = 0;
    virtual bool getCalibrationValuesJson(char *buffer, size_t len) = 0;
    virtual bool calibrateSensorHardware(JsonObjectConst data) = 0;
    virtual void reset() = 0;
    virtual const char *getDefaultUnit() const = 0;
    virtual int getDefaultPrecision() const = 0;
    bool getName(char *buffer, size_t len);
    bool setName(const char *name, size_t len);
    bool getScale(float &scale) const;
    bool setScale(float scale);
    bool getOffset(float &offset) const;
    bool setOffset(float offset);
    bool getPrecision(int &precision) const;
    bool setPrecision(int precision);
    bool getUnit(char *buffer, size_t len) const;
    bool setUnit(const char *unit, size_t len);

    const char *getType() const;
    uint8_t getId() const;

protected:
    void nvsNamespace(char *buffer, size_t len) const;


private:
    uint8_t _id;
    const char *_type;
    virtual bool readRaw(float &buffer) = 0;
    void ensureDefaults();
};
