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
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject dryStep = arr.createNestedObject();
    dryStep["instruction"] = "Keep sensor dry, then confirm";
    dryStep["key"] = CALIBRATE_KEY_DRY;
    JsonObject wetStep = arr.createNestedObject();
    wetStep["instruction"] = "Submerge sensor in water, then confirm";
    wetStep["key"] = CALIBRATE_KEY_WET;
    if (measureJson(doc) >= len)
        return false;
    serializeJson(doc, buffer, len);
    return true;
}
bool SoilMoisture::getCalibrationValuesJson(char *buffer, size_t len)
{
    float calDry = 0.0f, calWet = 0.0f;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    if (!NvsStorage::readFloat(CALIBRATE_KEY_DRY, calDry) || !NvsStorage::readFloat(CALIBRATE_KEY_WET, calWet))
        return false;

    StaticJsonDocument<64> doc;
    doc[CALIBRATE_KEY_DRY] = calDry;
    doc[CALIBRATE_KEY_WET] = calWet;
    if (measureJson(doc) >= len)
        return false;
    serializeJson(doc, buffer, len);
    return true;
}
bool SoilMoisture::calibrateSensorHardware(JsonObjectConst data)
{
    if (data[CALIBRATE_KEY_DRY].isNull() || data[CALIBRATE_KEY_WET].isNull())
    {
        return false;
    }
    float calDry = data[CALIBRATE_KEY_DRY];
    float calWet = data[CALIBRATE_KEY_WET];
    char ns[NvsStorage::NVS_KEY_LEN] = {};
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
    setUnit(defaultUnit, strlen(defaultUnit));
}
const char *SoilMoisture::getDefaultUnit() const { return defaultUnit; }
int SoilMoisture::getDefaultPrecision() const { return defaultPrecision; }
bool SoilMoisture::readRaw(float &buffer)
{
    unsigned long now = millis();
    if (now - lastReadTime >= MINIMAL_READING_INTERVALL)
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
            char ns[NvsStorage::NVS_KEY_LEN] = {};
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
        lastReadTime = now;
    }
    buffer = lastValue;
    return lastValid;
}
