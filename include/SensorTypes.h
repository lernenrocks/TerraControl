/**
 * @brief type of Sensor
 */
enum class SensorType{
    DHT22_TEMPERATURE, /** <@brief Temperature from DHT22 Sensor */
    DHT22_HUMIDITY, /** <@brief Humidity from DHT22 Sensor */
    SOIL_MOISTURE, /** <@brief capacitive Soil Moisture Sensor */
    REMOTE_SENSOR, /** <@brief generic ESP32 WiFi Sensor */
    UNKNOWN /** <@brief default value, not implemented Sensor */
};