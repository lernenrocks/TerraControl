#pragma once

#include "SensorBase.h"

constexpr const char CALIBRATE_KEY_DRY[] = "cDry";
static_assert(sizeof(CALIBRATE_KEY_DRY) - 1 <= SENSOR_KEY_NAME_MAX, "sensor key name too long");
constexpr const char CALIBRATE_KEY_WET[] = "cWet";
static_assert(sizeof(CALIBRATE_KEY_WET) - 1 <= SENSOR_KEY_NAME_MAX, "sensor key name too long");

class SoilMoisture : public SensorBase
{
public:
    SoilMoisture(uint8_t pin, uint8_t sensorId);
    ~SoilMoisture()=default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    bool getCalibrationValuesJson(char *buffer, size_t len) override;
    bool calibrateSensorHardware(JsonObjectConst data) override;
    void reset() override;
    const char *getDefaultUnit() const override;
    int getDefaultPrecision() const override;

private:
    uint8_t pin;
    unsigned long lastReadTime = 0;
    float lastValue = 0;
    bool lastValid = false;
    // calibration mirrored in RAM - NVS only at boot and in calibrateSensorHardware()
    bool _calibrated = false;
    float _calDry = 0.0f;
    float _calWet = 0.0f;
    static constexpr const char *defaultUnit = "%";
    static constexpr int defaultPrecision = 1;

    bool readRaw(float &buffer) override;
};
