#pragma once

#include "SensorBase.h"
#include <DHT.h>

class DHT22Temperature : public SensorBase
{
public:
    DHT22Temperature(DHT *dht, uint8_t id);
    ~DHT22Temperature()=default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    bool getCalibrationValuesJson(char *buffer, size_t len) override;
    bool calibrate(JsonObjectConst data) override;
    void reset() override;

private:
    DHT *dht;
    float lastTemperature = NAN;
    const char *defaultUnit = "°C";
    int defaultPrecision = 1;

    bool readRaw(float &buffer) override;
};
