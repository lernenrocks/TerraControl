#include "SensorBase.h"

SensorBase::SensorBase(uint8_t id, const char *type)
    : _id(id), _type(type)
{
    ensureDefaults();
}

bool SensorBase::read(float &value, bool raw)
{
    if (!isValid())
        return false;
    readRaw(value);
    if (!raw)
    {
        // Lokale Defaults bewusst hier, nicht in den Gettern: Die Getter geben bei
        // einem NVS-Fehler absichtlich nur `false` zurück, ohne einen plausibel
        // aussehenden Ersatzwert zu setzen - ein Aufrufer, der den Rückgabewert
        // ignoriert, soll keinen unauffällig falschen Wert bekommen (z.B. in der GUI).
        // Hier im sicherheitskritischen Pfad wird der Rückgabewert geprüft; die
        // lokalen Defaults sind nur ein zusätzliches Sicherheitsnetz für den Fall
        // eines Bugs in genau dieser Prüfung.
        float scale = 1.0f;
        float offset = 0.0f;
        if (!getScale(scale) || !getOffset(offset))
            return false;
        value = value * scale + offset;
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

void SensorBase::getName(char *buffer, size_t len)
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    NvsStorage::readString(SENSOR_NAME_KEY, buffer, len);
}

bool SensorBase::setName(const char *name, size_t len)
{
    if (len < NAME_LEN)
    {
        char ns[NvsStorage::KEY_NAME_MAX] = {};
        nvsNamespace(ns, sizeof(ns));
        NvsStorage::Session session(ns, false);
        return NvsStorage::writeString(SENSOR_NAME_KEY, name);
    }
    return false;
}

bool SensorBase::getScale(float &scale) const
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readFloat(VALUE_KEY_SCALE, scale);
}

bool SensorBase::setScale(float scale)
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeFloat(VALUE_KEY_SCALE, scale);
}

bool SensorBase::getOffset(float &offset) const
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readFloat(VALUE_KEY_OFFSET, offset);
}

bool SensorBase::setOffset(float offset)
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeFloat(VALUE_KEY_OFFSET, offset);
}

bool SensorBase::getPrecision(int &precision) const
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readInt(VALUE_KEY_PRECISION, precision);
}

bool SensorBase::setPrecision(int precision)
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeInt(VALUE_KEY_PRECISION, precision);
}

bool SensorBase::getUnit(char *buffer, size_t len) const
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readString(VALUE_KEY_UNIT, buffer, len);
}

bool SensorBase::setUnit(const char *unit)
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeString(VALUE_KEY_UNIT, unit);
}

void SensorBase::nvsNamespace(char *buffer, size_t len) const
{
    snprintf(buffer, len, "sensor_%d", _id);
}

void SensorBase::ensureDefaults()
{
    char ns[NvsStorage::KEY_NAME_MAX] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);

    float dummyScale;
    if (!NvsStorage::readFloat(VALUE_KEY_SCALE, dummyScale))
    {
        NvsStorage::writeFloat(VALUE_KEY_SCALE, 1.0f);
        NvsStorage::writeFloat(VALUE_KEY_OFFSET, 0.0f);
    }

    char dummyName[NAME_LEN];
    if (!NvsStorage::readString(SENSOR_NAME_KEY, dummyName, sizeof(dummyName)))
    {
        char defaultName[NAME_LEN] = {};
        snprintf(defaultName, sizeof(defaultName), "Sensor %d", _id);
        NvsStorage::writeString(SENSOR_NAME_KEY, defaultName);
    }
}
