#pragma once
enum class AnalogSensorType
{
    SOIL_MOISTURE_V1_2, /** <@brief Soil Moisture Sensor reqested */
    UNKNOWN              /** <@brief type not supported yet */
};
namespace AnalogSensor
{
    /**
     * @brief initializes all analog Sensors
     */
    void init();

    /**
     * @brief reads temperature or humidity from the sensor at a given index
     * @param idx position of of the sensor index
     * @param AnalogSensorType requested type e.g. SOIL_MOISTURE
     * @param value value output parameter
     * @return true, if value valid, false on read error
     */
    bool readValue(int idx, AnalogSensorType analogSensorType, float &value);
}