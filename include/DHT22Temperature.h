#pragma once

#include "SensorBase.h"
#include <DHT.h>

class DHT22Temperature : public SensorBase
{
public:
    DHT22Temperature(DHT *dht, uint8_t sensorId);
    ~DHT22Temperature()=default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    bool getCalibrationValuesJson(char *buffer, size_t len) override;
    bool calibrateSensorHardware(JsonObjectConst data) override;
    void reset() override;
    const char *getDefaultUnit() const override;
    int getDefaultPrecision() const override;

private:
    DHT *dht;
    unsigned long lastReadTime = 0;
    float lastValue = NAN;
    static constexpr const char *defaultUnit = "°C";
    static constexpr int defaultPrecision = 1;
    bool readRaw(float &buffer) override;
    bool valid=false;
};
