#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace SensorManager
{
    /**
     * @brief initializes Sensors
     */
    bool init();

    bool readSensor(float &value, bool raw, uint8_t sensorId);

    bool getSensorCalibrationJson(char *buffer, size_t len, uint8_t sensorId);
    bool getSensorCalibrationValuesJson(char *buffer, size_t len, uint8_t sensorId);
    bool calibrateSensorHardware(JsonObjectConst data, uint8_t sensorId);
    bool getSensorConfigJson(char *buffer, size_t len, uint8_t sensorId);
    bool calibrateSensorConfig(JsonObjectConst data, uint8_t sensorId);
    void resetSensor(uint8_t sensorId);
    void printSensorsToSerial();
    void printSensorConfigsToSerial();

}
