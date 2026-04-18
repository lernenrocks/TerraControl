#pragma once

#include <DHT.h>

enum class DHT22Value{
    TEMPERATURE, /** <@brief temoerature is requested */
    HUMIDITY /** <@brief humidity is requested */
};

namespace DHT22Sensor{


    /**
     * @brief initializes all 4 DHT22 Sensors
     */
    void init();

    /**
     * @brief reads temperature or humidity from the sensor at a given index
     * @param idx position of of the sensor index (0 - 3)
     * @param valueType requested value TEMPERATURE od HUMIDITY 
     * @param value value output parameter
     * @return true, if both values valid, false on read error
     */
    bool readValue(int idx, DHT22Value valueType, float &value);
}