#include "DHT22Temperature.h"

namespace
{
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
    StaticJsonDocument<16> doc;
    doc.to<JsonArray>();
    if (measureJson(doc) >= len)
        return false;
    serializeJson(doc, buffer, len);
    return true;
}
bool DHT22Temperature::getCalibrationValuesJson(char *buffer, size_t len)
{
    StaticJsonDocument<16> doc;
    doc.to<JsonObject>();
    if (measureJson(doc) >= len)
        return false;
    serializeJson(doc, buffer, len);
    return true;
}
bool DHT22Temperature::calibrateSensorHardware(JsonObjectConst data) { return true; }
void DHT22Temperature::reset()
{
    setScale(DEFAULT_SCALE);
    setOffset(DEFAULT_OFFSET);
    setPrecision(defaultPrecision);
    setUnit(defaultUnit, strlen(defaultUnit));
}
const char *DHT22Temperature::getDefaultUnit() const { return defaultUnit; }
int DHT22Temperature::getDefaultPrecision() const { return defaultPrecision; }
bool DHT22Temperature::readRaw(float &buffer)
{
    unsigned long now = millis();
    if (now - lastReadTime >= MINIMAL_READING_INTERVALL)
    {
        lastValue = dht->readTemperature();
        lastReadTime = now;
    }
    buffer = lastValue;
    return !isnan(lastValue);
}
