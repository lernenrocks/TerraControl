#pragma once

namespace SensorManager
{
    /**
     * @brief initializes Sensors
     */
    bool init();

    bool readSensor(float &value, bool raw, uint8_t idx);

    bool getSensorCalibrationJson(char *buffer, size_t len, uint8_t idx);
    bool getSensorCalibrationValuesJson(char *buffer, size_t len, uint8_t idx);
    bool calibrateSensorHardware(JsonObjectConst data, uint8_t idx);
    bool getSensorConfigJson(char *buffer, size_t len, uint8_t idx);
    bool calibrateSensorConfig(JsonObjectConst data, uint8_t idx);
    void resetSensor(uint8_t idx);

}