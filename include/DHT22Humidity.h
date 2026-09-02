#pragma once

#include "SensorBase.h"
#include <DHT.h>

class DHT22Humidity : public SensorBase
{
public:
    DHT22Humidity(DHT *dht, uint8_t id);
    ~DHT22Humidity()=default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    bool getCalibrationValuesJson(char *buffer, size_t len) override;
    bool calibrateSensorHardware(JsonObjectConst data) override;
    void reset() override;
    const char *getDefaultUnit() const override;
    int getDefaultPrecision() const override;

private:
    DHT *dht;
    float lastValue = NAN;
    const char *defaultUnit = "%";
    int defaultPrecision = 1;

    bool readRaw(float &buffer) override;
};
