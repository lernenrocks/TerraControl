#pragma once

#include "DataHub.h"


namespace SensorManager{

    /** 
     * @brief initializes Hardware Sensors wired to this ESP
     */
    void initSensors();

    /**
     * @brief iterates over the sensorData, reads the registered Sensors and
     * writes the values into the DataHub.
     */
    void update(unsigned long now);
}