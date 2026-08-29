#include "DHT22Temperature.h"

namespace{
 constexpr const unsigned long MINIMAL_READING_INTERVALL = 2000ul;
}

DHT22Temperature::DHT22Temperature(DHT *dht, uint8_t id) : SensorBase(id, "DHT22 Temperature"), dht(dht)
{
    int dummy;
    if (!getPrecision(dummy))
    {
        reset();
    }
}

bool DHT22Temperature::getCalibrationJson(char *buffer, size_t len)
{
    size_t written = snprintf(buffer, len, "[]");
    return written < len;
}
bool DHT22Temperature::getCalibrationValuesJson(char *buffer, size_t len)
{
    size_t written = snprintf(buffer, len, "{}");
    return written < len;
}
bool DHT22Temperature::calibrate(JsonObjectConst data){return true;}
void DHT22Temperature::reset(){
    setScale(DEFAULT_SCALE);
    setOffset(DEFAULT_OFFSET);
    setPrecision(defaultPrecision);
    setUnit(defaultUnit);
}
bool DHT22Temperature::readRaw(float &buffer)
{
    static unsigned long last = 0;
    unsigned long now = millis();
    if (now - last >= MINIMAL_READING_INTERVALL)
    {
        lastTemperature = dht->readTemperature();
        last = now;
    }
    buffer = lastTemperature;
    return !isnan(lastTemperature);
}
