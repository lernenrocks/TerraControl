#include "DHT22Humidity.h"

namespace
{
    constexpr const unsigned long MINIMAL_READING_INTERVALL = 2000ul;
}

DHT22Humidity::DHT22Humidity(DHT *dht, uint8_t id) : SensorBase(id, "DHT22 Humidity"), dht(dht)
{
    int dummy;
    if (!getPrecision(dummy))
    {
        reset();
    }
}

bool DHT22Humidity::getCalibrationJson(char *buffer, size_t len)
{
    StaticJsonDocument<16> doc;
    doc.to<JsonArray>();
    if (measureJson(doc) >= len)
        return false;
    serializeJson(doc, buffer, len);
    return true;
}
bool DHT22Humidity::getCalibrationValuesJson(char *buffer, size_t len)
{
    StaticJsonDocument<16> doc;
    doc.to<JsonObject>();
    if (measureJson(doc) >= len)
        return false;
    serializeJson(doc, buffer, len);
    return true;
}
bool DHT22Humidity::calibrateSensorHardware(JsonObjectConst data) { return true; }
void DHT22Humidity::reset()
{
    setScale(DEFAULT_SCALE);
    setOffset(DEFAULT_OFFSET);
    setPrecision(defaultPrecision);
    setUnit(defaultUnit, strlen(defaultUnit));
}
const char *DHT22Humidity::getDefaultUnit() const { return defaultUnit; }
int DHT22Humidity::getDefaultPrecision() const { return defaultPrecision; }
bool DHT22Humidity::readRaw(float &buffer)
{
    unsigned long now = millis();
    if (now - lastReadTime >= MINIMAL_READING_INTERVALL)
    {
        lastValue = dht->readHumidity();
        lastReadTime = now;
    }
    buffer = lastValue;
    return !isnan(lastValue);
}
