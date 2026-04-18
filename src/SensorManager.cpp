#include "SensorManager.h"
#include "DHT22Sensor.h"

namespace SensorManager
{
    void initSensors()
    {
        DHT22Sensor::init();
    }

    void update(unsigned long now)
    {
        DataHub::SensorData sensors[SENSOR_DATA_SIZE];
        if (!DataHub::getSensorDataArray(sensors))
        {
            Serial.println("[WARN] SensorManager::update() hat keine Daten aus dem DataHub erhalten");
            return;
        }

        for (int i = 0; i < SENSOR_DATA_SIZE; i++)
        {

            if ((!sensors[i].inUse)||(now - sensors[i].lastUpdate< sensors[i].updateInterval))
            {
                continue;
            }
            float value=0.0f;
            bool valid=false;
            switch (sensors[i].type)
            {
            case SensorType::DHT22_TEMPERATURE:
                valid = DHT22Sensor::readValue(sensors[i].sensorIndex, DHT22Value::TEMPERATURE, value);
                break;
            case SensorType::DHT22_HUMIDITY:
                valid = DHT22Sensor::readValue(sensors[i].sensorIndex, DHT22Value::HUMIDITY, value);
                break;
            default:
                continue;
            }
            if (!valid)
            {
                value = 0.0f;
                if (sensors[i].online)
                {
                    sensors[i].online = false;
                    sensors[i].dirty = true;
                }
            }
            else
            {
                if (!sensors[i].online)
                {
                    sensors[i].online = true;
                    sensors[i].dirty = true;
                }
            }
            if (sensors[i].value != value)
            {
                sensors[i].value = value;
                sensors[i].dirty = true;
            }
            if (sensors[i].dirty)
            {
                DataHub::setSensorData(sensors[i], i);
            }
        }
    }
}