#include "SoilMoisture.h"
#include "Logger.h"

namespace
{
    constexpr const unsigned long MINIMAL_READING_INTERVALL = 500ul;
    constexpr const long MINIMAL_DIFFERENCE_WET_DRY = 500l;
}
SoilMoisture::SoilMoisture(uint8_t pin, uint8_t sensorId)
    : SensorBase(sensorId, "Soil Moisture", defaultPrecision, defaultUnit), pin(pin)
{
    // load the calibration once - uncalibrated if either value is missing
    char dryKey[NvsStorage::NVS_KEY_LEN] = {};
    char wetKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CALIBRATE_KEY_DRY, dryKey, sizeof(dryKey));
    nvsKey(CALIBRATE_KEY_WET, wetKey, sizeof(wetKey));
    NvsStorage::Session session(SENSOR_NVS_NAMESPACE, true);
    float calDry = 0.0f, calWet = 0.0f;
    if (NvsStorage::readFloat(dryKey, calDry) && NvsStorage::readFloat(wetKey, calWet))
    {

        if (fabs(calDry - calWet) >= MINIMAL_DIFFERENCE_WET_DRY)
        {
            _calDry = calDry;
            _calWet = calWet;
            _calibrated = true;
        }
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
    if (!_calibrated)
        return false;

    StaticJsonDocument<64> doc;
    doc[CALIBRATE_KEY_DRY] = _calDry;
    doc[CALIBRATE_KEY_WET] = _calWet;
    if (measureJson(doc) >= len)
        return false;
    serializeJson(doc, buffer, len);
    return true;
}
bool SoilMoisture::calibrateSensorHardware(JsonObjectConst data)
{
    if (!data[CALIBRATE_KEY_DRY].is<float>() || !data[CALIBRATE_KEY_WET].is<float>())
    {
        char msg[128] = {};
        snprintf(msg, sizeof(msg), "Calibration Error Sensor %s", getName());
        Logger::log(Logger::ErrorLevel::ERROR, msg);
        return false;
    }
    float calDry = data[CALIBRATE_KEY_DRY];
    float calWet = data[CALIBRATE_KEY_WET];
    if (fabs(calWet - calDry) < MINIMAL_DIFFERENCE_WET_DRY)
    {
        char msg[128] = {};
        snprintf(msg, sizeof(msg), "Sensor %s Difference between calibration values too low", getName());
        Logger::log(Logger::ErrorLevel::ERROR, msg);
        return false;
    }
    char dryKey[NvsStorage::NVS_KEY_LEN] = {};
    char wetKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CALIBRATE_KEY_DRY, dryKey, sizeof(dryKey));
    nvsKey(CALIBRATE_KEY_WET, wetKey, sizeof(wetKey));
    NvsStorage::Session session(SENSOR_NVS_NAMESPACE, false);
    if (!NvsStorage::writeFloat(dryKey, calDry) || !NvsStorage::writeFloat(wetKey, calWet))
        return false; // RAM keeps the previous calibration
    _calDry = calDry; // RAM only after the NVS write succeeded
    _calWet = calWet;
    _calibrated = true;
    return true;
}
void SoilMoisture::reset()
{
    setScale(DEFAULT_SCALE);
    setOffset(DEFAULT_OFFSET);
    setPrecision(defaultPrecision);
    setUnit(defaultUnit);
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
            if (!_calibrated)
            {
                lastValue = raw;
                lastValid = false;
            }
            else
            {
                lastValue = (raw - _calDry) / (_calWet - _calDry) * 100.0f;
                lastValid = true;
            }
        }
        lastReadTime = now;
    }
    buffer = lastValue;
    return lastValid;
}
