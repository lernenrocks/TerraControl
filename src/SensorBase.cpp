#include "SensorBase.h"

SensorBase::SensorBase(uint8_t id, const char *type, int defaultPrecision, const char *defaultUnit)
    : _id(id), _type(type)
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

uint8_t SensorBase::getId() const
{
    return _id;
}

const char *SensorBase::getName() const
{
    return _name;
}

bool SensorBase::setName(const char *name)
{
    if (strlen(name) >= SENSOR_NAME_LEN)
        return false;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeString(CONFIG_KEY_SENSOR_NAME, name))
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
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeFloat(CONFIG_KEY_SCALE, scale))
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
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeFloat(CONFIG_KEY_OFFSET, offset))
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
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeInt(CONFIG_KEY_PRECISION, precision))
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
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeString(CONFIG_KEY_UNIT, unit))
        return false;
    strlcpy(_unit, unit, sizeof(_unit));
    return true;
}


void SensorBase::nvsNamespace(char *buffer, size_t len) const
{
    snprintf(buffer, len, "sensor_%d", _id);
}

void SensorBase::ensureDefaults(int defaultPrecision, const char *defaultUnit)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);

    if (!NvsStorage::readFloat(CONFIG_KEY_SCALE, _scale))
    {
        _scale = DEFAULT_SCALE;
        NvsStorage::writeFloat(CONFIG_KEY_SCALE, _scale);
    }
    if (!NvsStorage::readFloat(CONFIG_KEY_OFFSET, _offset))
    {
        _offset = DEFAULT_OFFSET;
        NvsStorage::writeFloat(CONFIG_KEY_OFFSET, _offset);
    }

    if (!NvsStorage::readString(CONFIG_KEY_SENSOR_NAME, _name, sizeof(_name)))
    {
        snprintf(_name, sizeof(_name), "Sensor %d", _id);
        NvsStorage::writeString(CONFIG_KEY_SENSOR_NAME, _name);
    }

    if (!NvsStorage::readInt(CONFIG_KEY_PRECISION, _precision))
    {
        _precision = defaultPrecision;
        NvsStorage::writeInt(CONFIG_KEY_PRECISION, _precision);
    }
    if (!NvsStorage::readString(CONFIG_KEY_UNIT, _unit, sizeof(_unit)))
    {
        strlcpy(_unit, defaultUnit, sizeof(_unit));
        NvsStorage::writeString(CONFIG_KEY_UNIT, _unit);
    }
}
