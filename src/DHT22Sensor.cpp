#include "DHT22Sensor.h"


namespace
{
#define DHTSENSORS_LEN 4

#define DHTTYPE 22

    DHT dhtSensors[DHTSENSORS_LEN] = {
        DHT(DHTPIN_0, DHTTYPE),
        DHT(DHTPIN_1, DHTTYPE),
        DHT(DHTPIN_2, DHTTYPE),
        DHT(DHTPIN_3, DHTTYPE),
    };
}

namespace DHT22Sensor
{
    void init()
    {
        for (int i = 0; i < DHTSENSORS_LEN; i++)
        {
            dhtSensors[i].begin();
        }
    }
    bool readValues(int idx, float &temperature, float &humidity)
    {
        if (idx >= DHTSENSORS_LEN)
        {
            return false;
        }
        temperature = dhtSensors[idx].readTemperature();
        humidity = dhtSensors[idx].readHumidity();
        return !isnan(temperature) && !isnan(humidity);
    }
}