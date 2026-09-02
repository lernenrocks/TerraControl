#include "SensorManager.h"
#include "DHT22Temperature.h"
#include "DHT22Humidity.h"
#include "SoilMoisture.h"
#include "Logger.h"
#include <vector>

constexpr const uint8_t MAX_DHT_22 = 4;

namespace
{
    std::vector<SensorBase *> sensors;
    DHT dht[MAX_DHT_22] = {DHT(DHTPIN_0, DHT22), DHT(DHTPIN_1, DHT22), DHT(DHTPIN_2, DHT22), DHT(DHTPIN_3, DHT22)};
    uint8_t sensorCount = 0;
    SensorBase *find(const uint8_t idx)
    {
        for (SensorBase *s : sensors)
        {
            if (s->getId() == idx)
            {
                return s;
            }
        }
        return nullptr;
    }
}
namespace SensorManager
{
    bool init()
    {
        bool success=true;
        // initialise DHT
        for (size_t i = 0; i < MAX_DHT_22; i++)
        {
            dht[i].begin();
            DHT22Temperature *dhtT = new (std::nothrow) DHT22Temperature(&dht[i], sensorCount);
            if (!dhtT)
            {
                char msg[64];
                snprintf(msg, sizeof(msg), "DHT allocation failed, pin index %d, free heap %u", (int)i, ESP.getFreeHeap());
                Logger::log(Logger::ErrorLevel::ERROR, msg);
                success = false;
            }
            else
            {
                sensors.push_back(dhtT);
                sensorCount++;
            }
            DHT22Humidity *dhtH = new (std::nothrow) DHT22Humidity(&dht[i], sensorCount);
            if (!dhtH)
            {
                char msg[64];
                snprintf(msg, sizeof(msg), "DHT allocation failed, pin index %d, free heap %u", (int)i, ESP.getFreeHeap());
                Logger::log(Logger::ErrorLevel::ERROR, msg);
                success = false;
            }
            else
            {
                sensors.push_back(dhtH);
                sensorCount++;
            }
        }
        SoilMoisture *soilM = new (std::nothrow) SoilMoisture(ANALOG_IN_0, sensorCount);
        if (!soilM)
        {
            char msg[64];
            snprintf(msg, sizeof(msg), "Soil Moisture Sensor allocation failed, pin %d, free heap %u", ANALOG_IN_0, ESP.getFreeHeap());
            Logger::log(Logger::ErrorLevel::ERROR, msg);
            success = false;
        }
        else
        {
            sensors.push_back(soilM);
            sensorCount++;
        }
        return success;
    }
    bool readSensor(float &value, bool raw, uint8_t idx)
    {
        SensorBase *s = find(idx);
        if (!s)
            return false;
        return s->read(value, raw);
    }

    bool getSensorCalibrationJson(char *buffer, size_t len, uint8_t idx)
    {
        SensorBase *s = find(idx);
        if (!s)
            return false;
        return s->getCalibrationJson(buffer, len);
    }

    bool getSensorCalibrationValuesJson(char *buffer, size_t len, uint8_t idx)
    {
        SensorBase *s = find(idx);
        if (!s)
            return false;
        return s->getCalibrationValuesJson(buffer, len);
    }
    bool calibrateSensorHardware(JsonObjectConst data, uint8_t idx)
    {
        SensorBase *s = find(idx);
        if (!s)
            return false;
        return s->calibrateSensorHardware(data);
    }
    bool getSensorConfigJson(char *buffer, size_t len, uint8_t idx)
    {
        SensorBase *s = find(idx);
        if (!s)
            return false;
        const char *type = s->getType();
        char name[SENSOR_NAME_LEN] = {};
        if (!s->getName(name, sizeof(name)))
            snprintf(name, sizeof(name), "sensor %d", idx);
        char unit[SENSOR_UNIT_LEN] = {};
        if (!s->getUnit(unit, sizeof(unit)))
            strlcpy(unit, s->getDefaultUnit(), sizeof(unit));
        int precision;
        if (!s->getPrecision(precision))
            precision = s->getDefaultPrecision();
        float scale;
        if (!s->getScale(scale))
            scale = DEFAULT_SCALE;
        float offset;
        if (!s->getOffset(offset))
            offset = DEFAULT_OFFSET;

        size_t written = snprintf(buffer, len,
                                  "{\"%s\":%d,\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":%f,\"%s\":%f,\"%s\":%d,\"%s\":\"%s\"}",
                                  CONFIG_KEY_ID, idx,
                                  CONFIG_KEY_TYPE, type,
                                  CONFIG_KEY_NAME, name,
                                  CONFIG_KEY_SCALE, scale,
                                  CONFIG_KEY_OFFSET, offset,
                                  CONFIG_KEY_PRECISION, precision,
                                  CONFIG_KEY_UNIT, unit);
        if (written >= len)
        {
            Logger::log(Logger::ErrorLevel::ERROR, "Sensor config json buffer to small");
        }
        return written < len;
    }
    bool calibrateSensorConfig(JsonObjectConst data, uint8_t idx)
    {
        SensorBase *s = find(idx);
        if (!s)
            return false;
        bool success = true;
        if (!data[CONFIG_KEY_NAME].isNull())
        {
            const char *newName = data[CONFIG_KEY_NAME];
            if (!s->setName(newName, strlen(newName)))
            {
                char msg[64] = {};
                snprintf(msg, sizeof(msg), "sensor %d could not set name", idx);
                Logger::log(Logger::ErrorLevel::WARN, msg);
                success = false;
            }
        }
        if (!data[CONFIG_KEY_UNIT].isNull())
        {
            const char *newUnit = data[CONFIG_KEY_UNIT];
            if (!s->setUnit(newUnit, strlen(newUnit)))
            {
                char msg[64] = {};
                snprintf(msg, sizeof(msg), "sensor %d could not set unit", idx);
                Logger::log(Logger::ErrorLevel::WARN, msg);
                success = false;
            }
        }
        if (!data[CONFIG_KEY_SCALE].isNull())
        {
            float newScale = data[CONFIG_KEY_SCALE];
            if (!s->setScale(newScale))
            {
                char msg[64] = {};
                snprintf(msg, sizeof(msg), "sensor %d could not set scale", idx);
                Logger::log(Logger::ErrorLevel::WARN, msg);
                success = false;
            }
        }
        if (!data[CONFIG_KEY_OFFSET].isNull())
        {
            float newOffset = data[CONFIG_KEY_OFFSET];
            if (!s->setOffset(newOffset))
            {
                char msg[64] = {};
                snprintf(msg, sizeof(msg), "sensor %d could not set offset", idx);
                Logger::log(Logger::ErrorLevel::WARN, msg);
                success = false;
            }
        }
        if (!data[CONFIG_KEY_PRECISION].isNull())
        {
            int newPrecision = data[CONFIG_KEY_PRECISION];
            if (!s->setPrecision(newPrecision))
            {
                char msg[64] = {};
                snprintf(msg, sizeof(msg), "sensor %d could not set precision", idx);
                Logger::log(Logger::ErrorLevel::WARN, msg);
                success = false;
            }
        }
        return success;
    }
    void resetSensor(uint8_t idx)
    {
        SensorBase *s = find(idx);
        if (!s)
            return;
        s->reset();
    }
}
