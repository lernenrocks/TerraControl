#include "SensorBase.h"

SensorBase::SensorBase(uint8_t sensorId, const char *type, int defaultPrecision, const char *defaultUnit)
    : _sensorId(sensorId), _type(type)
{
    ensureDefaults(defaultPrecision, defaultUnit);
}

bool SensorBase::read(float &value, bool raw)
{
    if (!readRaw(value))
        return false;
    if (!raw)
    {
        value = value * getScale() + getOffset();
    }
    return true;
}

const char *SensorBase::getType() const
{
    return _type;
}

uint8_t SensorBase::getSensorId() const
{
    return _sensorId;
}

const char *SensorBase::getName() const
{
    return _name;
}

bool SensorBase::setName(const char *name)
{
    if (strlen(name) >= SENSOR_NAME_LEN)
        return false;
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_SENSOR_NAME, key, sizeof(key));
    NvsStorage::Session session(SENSOR_NVS_NAMESPACE, false);
    if (!NvsStorage::writeString(key, name))
        return false;
    strlcpy(_name, name, sizeof(_name));
    return true;
}

float SensorBase::getScale() const
{
    return _scale;
}

bool SensorBase::setScale(float scale)
{
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_SCALE, key, sizeof(key));
    NvsStorage::Session session(SENSOR_NVS_NAMESPACE, false);
    if (!NvsStorage::writeFloat(key, scale))
        return false;
    _scale = scale;
    return true;
}

float SensorBase::getOffset() const
{
    return _offset;
}

bool SensorBase::setOffset(float offset)
{
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_OFFSET, key, sizeof(key));
    NvsStorage::Session session(SENSOR_NVS_NAMESPACE, false);
    if (!NvsStorage::writeFloat(key, offset))
        return false;
    _offset = offset;
    return true;
}

int SensorBase::getPrecision() const
{
    return _precision;
}

bool SensorBase::setPrecision(int precision)
{
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_PRECISION, key, sizeof(key));
    NvsStorage::Session session(SENSOR_NVS_NAMESPACE, false);
    if (!NvsStorage::writeInt(key, precision))
        return false;
    _precision = precision;
    return true;
}

const char *SensorBase::getUnit() const
{
    return _unit;
}

bool SensorBase::setUnit(const char *unit)
{
    if (strlen(unit) >= SENSOR_UNIT_LEN)
        return false;
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_UNIT, key, sizeof(key));
    NvsStorage::Session session(SENSOR_NVS_NAMESPACE, false);
    if (!NvsStorage::writeString(key, unit))
        return false;
    strlcpy(_unit, unit, sizeof(_unit));
    return true;
}


void SensorBase::nvsKey(const char *key, char *buffer, size_t len) const
{
    snprintf(buffer, len, "s%u_%s", _sensorId, key);
}

void SensorBase::ensureDefaults(int defaultPrecision, const char *defaultUnit)
{
    // RAM defaults first - they stay in place if a value is missing in the NVS
    _scale = DEFAULT_SCALE;
    _offset = DEFAULT_OFFSET;
    _precision = defaultPrecision;
    strlcpy(_unit, defaultUnit, sizeof(_unit));
    snprintf(_name, sizeof(_name), "Sensor %d", _sensorId);

    char scaleKey[NvsStorage::NVS_KEY_LEN] = {};
    char offsetKey[NvsStorage::NVS_KEY_LEN] = {};
    char nameKey[NvsStorage::NVS_KEY_LEN] = {};
    char precisionKey[NvsStorage::NVS_KEY_LEN] = {};
    char unitKey[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_SCALE, scaleKey, sizeof(scaleKey));
    nvsKey(CONFIG_KEY_OFFSET, offsetKey, sizeof(offsetKey));
    nvsKey(CONFIG_KEY_SENSOR_NAME, nameKey, sizeof(nameKey));
    nvsKey(CONFIG_KEY_PRECISION, precisionKey, sizeof(precisionKey));
    nvsKey(CONFIG_KEY_UNIT, unitKey, sizeof(unitKey));

    NvsStorage::Session session(SENSOR_NVS_NAMESPACE, false);

    if (!NvsStorage::readFloat(scaleKey, _scale))
    {
        _scale = DEFAULT_SCALE;
        NvsStorage::writeFloat(scaleKey, _scale);
    }
    if (!NvsStorage::readFloat(offsetKey, _offset))
    {
        _offset = DEFAULT_OFFSET;
        NvsStorage::writeFloat(offsetKey, _offset);
    }

    if (!NvsStorage::readString(nameKey, _name, sizeof(_name)))
    {
        snprintf(_name, sizeof(_name), "Sensor %d", _sensorId);
        NvsStorage::writeString(nameKey, _name);
    }

    if (!NvsStorage::readInt(precisionKey, _precision))
    {
        _precision = defaultPrecision;
        NvsStorage::writeInt(precisionKey, _precision);
    }
    if (!NvsStorage::readString(unitKey, _unit, sizeof(_unit)))
    {
        strlcpy(_unit, defaultUnit, sizeof(_unit));
        NvsStorage::writeString(unitKey, _unit);
    }
}
