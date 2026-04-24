#include <Arduino.h>
#include "AnalogSensor.h"

namespace
{
    constexpr int ANALOG_SENSORS_LEN = 1;
    constexpr int pins[ANALOG_SENSORS_LEN] = {ANALOG_IN_0};

}
namespace AnalogSensor
{
    void init()
    {
        for (int i = 0; i < ANALOG_SENSORS_LEN; i++)
            pinMode(pins[i], INPUT);
    }

    /**
     * @brief reads a value from the analog sensor at the given index
     * @param idx sensor index
     * @param type hardware variant — provided by SensorManager from DataHub
     * @param value output parameter
     * @return true if value is valid, false on index out of range, unknown type, or rail value
     */
    bool readValue(int idx, AnalogSensorType type, float &value)
    {
        if (idx < 0 || idx >= ANALOG_SENSORS_LEN)
        {
            return false;
        }
        switch (type)
        {
        case AnalogSensorType::SOIL_MOISTURE_V1_2:
            value = analogRead(pins[idx]);
            return (value > 0 && value < 4095);
        case AnalogSensorType::UNKNOWN:
            return false;
        }
        return false;
    }
}