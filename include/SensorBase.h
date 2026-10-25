#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"
#include "SensorConfigKeys.h"
#include <atomic>

// Specific to SensorBase's own id/type tracking - not shared with RelaySensorBase,
// which gets its index/type from EntityBase/the owning ApDevice instead.
constexpr const char CONFIG_KEY_SENSOR_TYPE[] = "type";
constexpr const char CONFIG_KEY_SENSOR_ID[] = "id";
// NVS layout: all local sensors share one namespace, every key prefixed with the sensor id
// ("s<id>_<key>", e.g. "s3_scale") - the number of namespaces does not grow with the sensors.
constexpr const char SENSOR_NVS_NAMESPACE[] = "sensors";

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

    unsigned long getLastValid() const;

protected:
    // Builds this sensor's prefixed key ("s<id>_<key>") into a buffer of NvsStorage::NVS_KEY_LEN.
    // Cannot fail: every key name is checked by a static_assert (SensorConfigKeys.h).
    void nvsKey(const char *key, char *buffer, size_t len) const;

private:
    uint8_t _sensorId;
    const char *_type;
    char _name[SENSOR_NAME_LEN] = {};
    float _scale = DEFAULT_SCALE;
    float _offset = DEFAULT_OFFSET;
    int _precision = 0;
    char _unit[SENSOR_UNIT_LEN] = {};
    std::atomic<unsigned long> _lastValid=0ul;
    virtual bool readRaw(float &buffer) = 0;
    void ensureDefaults(int defaultPrecision, const char *defaultUnit);

};
