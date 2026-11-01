#include "ApDeviceSensor.h"
#include <new>
#include "NvsStorage.h"
#include "Logger.h"

bool ApDeviceSensor::read(float &value, bool raw)
{
    if (_valid)
        value = raw ? _value : _value * _scale + _offset;
    return _valid;
}

bool ApDeviceSensor::reset()
{
    bool ok = true;
    char scaleKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_SCALE, scaleKey, sizeof(scaleKey));
    char offsetKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_OFFSET,offsetKey,sizeof(offsetKey));
    char precisionKey[NvsStorage::NVS_KEY_LEN]={};
    nvsKey(CONFIG_KEY_PRECISION,precisionKey,sizeof(precisionKey));
    char unitKey[NvsStorage::NVS_KEY_LEN]={};
    nvsKey(CONFIG_KEY_UNIT,unitKey,sizeof(unitKey));
    NvsStorage::Session session(nvsNamespace(), false);
    if(NvsStorage::removeKey(scaleKey))
        _scale=DEFAULT_SCALE;
    else ok=false;
    if(NvsStorage::removeKey(offsetKey))
        _offset=DEFAULT_OFFSET;
    else ok=false;
    if(NvsStorage::removeKey(precisionKey))
        _precision=getDefaultPrecision(getType());
    else ok=false;
    if(NvsStorage::removeKey(unitKey))
        strlcpy(_unit, getDefaultUnit(getType()),sizeof(_unit));
    else ok=false;
    return ok;
}

ApDeviceSensor *ApDeviceSensor::recover(const char *nvsNamespace, const uint8_t idx, EntityType type)
{
    ApDeviceSensor *sensor = new (std::nothrow) ApDeviceSensor(nvsNamespace, idx, type);
    if (!sensor)
        return nullptr;
    if (!sensor->load())
    {
        delete sensor;
        return nullptr;
    }
    return sensor;
}

ApDeviceSensor *ApDeviceSensor::onboard(const char *nvsNamespace, const uint8_t idx, EntityType type, JsonObjectConst data)
{
    ApDeviceSensor *sensor = new (std::nothrow) ApDeviceSensor(nvsNamespace, idx, type);
    if (!sensor)
        return nullptr;
    if (!sensor->write(data))
    {
        delete sensor;
        return nullptr;
    }
    return sensor;
}
bool ApDeviceSensor::applyResponse(JsonObjectConst data)
{
    char path[SENSOR_SOURCE_LEN] = {};
    strlcpy(path, _source, sizeof(path));
    JsonVariantConst node = data;
    char *tail = nullptr;
    char *token = strtok_r(path, "/", &tail);
    while (token != nullptr)
    {
        node = node[token];
        token = strtok_r(nullptr, "/", &tail);
    }
    if (node["value"].is<float>() && node["valid"].is<bool>()) // SensorNode case
    {
        float read = node["value"];
        _value = read;
        _valid = node["valid"] && !isnan(read);
    }
    else if (node.is<float>())
    {
        _value = node;
        _valid = true;
    }
    else
    {
        _valid = false;
    }
    return _valid;
}

float ApDeviceSensor::getScale() const { return _scale; }
bool ApDeviceSensor::setScale(float scale)
{
    if (scale == 0.0f || !isfinite(scale))
    {
        Logger::log(Logger::ErrorLevel::WARN, "scale must not be 0 or infinite");
        return false;
    }
    char scaleKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_SCALE, scaleKey, sizeof(scaleKey));
    NvsStorage::Session session(nvsNamespace(), false);
    if (NvsStorage::writeFloat(scaleKey, scale))
    {
        _scale = scale;
        return true;
    }
    return false;
}
float ApDeviceSensor::getOffset() const { return _offset; }
bool ApDeviceSensor::setOffset(float offset)
{
    if (!isfinite(offset))
    {
        Logger::log(Logger::ErrorLevel::WARN, "offset must be finite");
        return false;
    }
    char offsetKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_OFFSET, offsetKey, sizeof(offsetKey));
    NvsStorage::Session session(nvsNamespace(), false);
    if (NvsStorage::writeFloat(offsetKey, offset))
    {
        _offset = offset;
        return true;
    }
    return false;
}
int ApDeviceSensor::getPrecision() const { return _precision; }
bool ApDeviceSensor::setPrecision(int precision)
{
    if (precision < 0)
    {
        Logger::log(Logger::ErrorLevel::WARN, "unit missing or too long");
        return false;
    }
    char precisionKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_PRECISION, precisionKey, sizeof(precisionKey));
    NvsStorage::Session session(nvsNamespace(), false);
    if (NvsStorage::writeInt(precisionKey, precision))
    {
        _precision = precision;
        return true;
    }
    return false;
}
const char *ApDeviceSensor::getUnit() const { return _unit; }
bool ApDeviceSensor::setUnit(const char *unit)
{
    if (!unit || strlen(unit) >= SENSOR_UNIT_LEN)
    {
        Logger::log(Logger::ErrorLevel::WARN, "Unit too many characters");
        return false;
    }
    char unitKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_UNIT, unitKey, sizeof(unitKey));
    NvsStorage::Session session(nvsNamespace(), false);
    if (NvsStorage::writeString(unitKey, unit))
    {
        strlcpy(_unit, unit, sizeof(_unit));
        return true;
    }
    return false;
}

// private
ApDeviceSensor::ApDeviceSensor(const char *nvsNamespace, const uint8_t idx, EntityType type)
    : EntityBase(nvsNamespace, idx, type)
{
    _precision = getDefaultPrecision(type);
    strlcpy(_unit, getDefaultUnit(type), sizeof(_unit));
}

bool ApDeviceSensor::load()
{
    if (!EntityBase::load())
        return false;
    char sourceKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_ENTITY_SOURCE, sourceKey, sizeof(sourceKey));
    NvsStorage::Session session(EntityBase::nvsNamespace(), true);
    char source[SENSOR_SOURCE_LEN] = {};
    if (!NvsStorage::readString(sourceKey, source, sizeof(source)))
        return false;
    strlcpy(_source, source, sizeof(_source));

    char scaleKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_SCALE, scaleKey, sizeof(scaleKey));
    NvsStorage::readFloat(scaleKey, _scale);

    char offsetKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_OFFSET, offsetKey, sizeof(offsetKey));
    NvsStorage::readFloat(offsetKey, _offset);

    char precisionKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_PRECISION, precisionKey, sizeof(precisionKey));
    NvsStorage::readInt(precisionKey, _precision);

    char unitKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_UNIT, unitKey, sizeof(unitKey));
    NvsStorage::readString(unitKey, _unit, sizeof(_unit));

    return true;
}

bool ApDeviceSensor::write(JsonObjectConst data)
{
    if (!data["source"].is<const char *>())
        return false;
    const char *source = data["source"];
    if (strlen(source) == 0 || !(strlen(source) < SENSOR_SOURCE_LEN))
        return false;
    if (!EntityBase::write(data))
        return false;
    char sourceKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_ENTITY_SOURCE, sourceKey, sizeof(sourceKey));
    NvsStorage::Session session(EntityBase::nvsNamespace(), false);
    if (!NvsStorage::writeString(sourceKey, source))
        return false;
    strlcpy(_source, source, sizeof(_source));
    return true;
}