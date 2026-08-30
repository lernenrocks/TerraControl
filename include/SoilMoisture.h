#pragma once

#include "SensorBase.h"

constexpr const char CALIBRATE_KEY_DRY[] = "cDry";
constexpr const char CALIBRATE_KEY_WET[] = "cWet";

class SoilMoisture : public SensorBase
{
public:
    SoilMoisture(uint8_t pin, uint8_t id);
    ~SoilMoisture()=default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    bool getCalibrationValuesJson(char *buffer, size_t len) override;
    bool calibrate(JsonObjectConst data) override;
    void reset() override;

private:
    uint8_t pin;
    float lastValue = 0;
    bool lastValid = false;
    const char *defaultUnit = "%";
    int defaultPrecision = 1;

    bool readRaw(float &buffer) override;
};
