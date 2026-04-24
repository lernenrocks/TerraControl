#include "SensorManager.h"
#include "DHT22Sensor.h"
#include "AnalogSensor.h"

namespace SensorManager
{
    void initSensors()
    {
        DHT22Sensor::init();
        AnalogSensor::init();
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

            if ((!sensors[i].inUse) || (now - sensors[i].lastUpdate < sensors[i].updateInterval))
            {
                continue;
            }
            float value = 0.0f;
            bool valid = false;
            switch (sensors[i].type)
            {
            case SensorType::DHT22_TEMPERATURE:
                valid = DHT22Sensor::readValue(sensors[i].sensorIndex, DHT22Value::TEMPERATURE, value);
                break;
            case SensorType::DHT22_HUMIDITY:
                valid = DHT22Sensor::readValue(sensors[i].sensorIndex, DHT22Value::HUMIDITY, value);
                break;
            case SensorType::SOIL_MOISTURE:
                valid = AnalogSensor::readValue(sensors[i].sensorIndex, AnalogSensorType::SOIL_MOISTURE_V1_2, value);
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
                sensors[i].lastUpdate = now;
                if (!sensors[i].online)
                {
                    sensors[i].online = true;
                    sensors[i].dirty = true;
                }
            }
            if (fabsf(sensors[i].calMax - sensors[i].calMin) > 0.001f)
            {
                value = (value - sensors[i].calMin) / (sensors[i].calMax - sensors[i].calMin) * 100.0f;
            }
            value -= sensors[i].calOffset;
            value = constrain(value, 0.0f, 100.0f);
            if (sensors[i].value != value)
            {
                sensors[i].value = value;
                sensors[i].dirty = true;
            }
            DataHub::setSensorData(sensors[i], i);
        }
    }

    void calibrate(int idx, float calMin, float calMax, float calOffset)
    {
        DataHub::SensorData sensor;
        if (DataHub::getSensorData(sensor, idx))
        {
            sensor.calMin = calMin;
            sensor.calMax = calMax;
            sensor.calOffset = calOffset;
            DataHub::setSensorData(sensor, idx);
        }
    }
}