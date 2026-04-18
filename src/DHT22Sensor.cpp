#include "DHT22Sensor.h"


namespace
{
constexpr int DHTSENSORS_LEN = 4;
constexpr int DHTTYPE = 22;

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

    bool readValue(int idx, DHT22Value valueType, float &value){
               if (idx <0 || idx >= DHTSENSORS_LEN)
        {
            return false;
        }
        if(valueType==DHT22Value::TEMPERATURE){
            value=dhtSensors[idx].readTemperature();
        }
        else if(valueType==DHT22Value::HUMIDITY){
            value = dhtSensors[idx].readHumidity();
        }
        else {
            return false;
        }
        return !isnan(value);
    }
}

