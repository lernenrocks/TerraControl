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

    /**
     * @brief calibrates a Sensor. Handles Offset calivration and two point calibration
     * @param idx index of the sensor in sensorData array
     * @param calMin the minimun value for two point calibration (e.g min dry soil or air)
     * @param calMax the maximum value for two point calibration (e.g. max wet soil or water)
     * @param calOffset the offset for calibration (e.g. tara for an scale, or temperature correction)
     */
    void calibrate(int index, float calMin,float calMax, float calOffset);
}