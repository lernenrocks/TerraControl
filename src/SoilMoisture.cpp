#include "SoilMoisture.h"

namespace
{
    constexpr const unsigned long MINIMAL_READING_INTERVALL = 500ul;
}
SoilMoisture::SoilMoisture(uint8_t pin, uint8_t id) : SensorBase(id, "Soil Moisture"), pin(pin)
{
    int dummy;
    if (!getPrecision(dummy))
    {
        reset();
    }
}

bool SoilMoisture::getCalibrationJson(char *buffer, size_t len)
{
    size_t written = snprintf(buffer, len,
                              "[{\"instruction\":\"Keep sensor dry, then confirm\",\"key\":\"%s\"},"
                              "{\"instruction\":\"Submerge sensor in water, then confirm\",\"key\":\"%s\"}]",
                              CALIBRATE_KEY_DRY, CALIBRATE_KEY_WET);
    return written < len;
}
bool SoilMoisture::getCalibrationValuesJson(char *buffer, size_t len)
{
    float calDry = 0.0f, calWet = 0.0f;
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    if(NvsStorage::readFloat(CALIBRATE_KEY_DRY, calDry) && NvsStorage::readFloat(CALIBRATE_KEY_WET, calWet))
    {
    size_t written = snprintf(buffer, len, "{\"%s\":%f,\"%s\":%f}",CALIBRATE_KEY_DRY,calDry,CALIBRATE_KEY_WET,calWet);
    return true;
    }
    return false;
}
bool SoilMoisture::calibrate(JsonObjectConst data)
{
    if (data[CALIBRATE_KEY_DRY].isNull() || data[CALIBRATE_KEY_WET].isNull())
    {
        return false;
    }
    float calDry = data[CALIBRATE_KEY_DRY];
    float calWet = data[CALIBRATE_KEY_WET];
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    NvsStorage::writeFloat(CALIBRATE_KEY_DRY, calDry);
    NvsStorage::writeFloat(CALIBRATE_KEY_WET, calWet);
    return true;
}
void SoilMoisture::reset()
{
    setScale(DEFAULT_SCALE);
    setOffset(DEFAULT_OFFSET);
    setPrecision(defaultPrecision);
    setUnit(defaultUnit);
}
bool SoilMoisture::readRaw(float &buffer)
{
    static unsigned long last = 0;
    unsigned long now = millis();
    if (now - last >= MINIMAL_READING_INTERVALL)
    {
        float raw = analogRead(pin);
        if (raw <= 0 || raw >= 4095)
        {
            lastValue = raw;
            lastValid = false;
        } // short circuit or open pin
        else
        {
            float calDry = 0.0f, calWet = 0.0f;
            char ns[NvsStorage::KEY_NAME_MAX] = {};
            nvsNamespace(ns, sizeof(ns));
            NvsStorage::Session session(ns, true);
            bool calibrated = NvsStorage::readFloat(CALIBRATE_KEY_DRY, calDry) && NvsStorage::readFloat(CALIBRATE_KEY_WET, calWet);
            if (!calibrated)
            {
                lastValue = raw;
                lastValid = false;
            }
            else
            {
                lastValue = (raw - calDry) / (calWet - calDry) * 100.0f;
                lastValid = true;
            }
        }
        last = now;
    }
    buffer = lastValue;
    return lastValid;
}
