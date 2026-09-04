#include "SensorBase.h"

SensorBase::SensorBase(uint8_t id, const char *type)
    : _id(id), _type(type)
{
    ensureDefaults();
}

bool SensorBase::read(float &value, bool raw)
{
    if (!readRaw(value))
        return false;
    if (!raw)
    {
        // Lokale Defaults bewusst hier, nicht in den Gettern: Die Getter geben bei
        // einem NVS-Fehler absichtlich nur `false` zurück, ohne einen plausibel
        // aussehenden Ersatzwert zu setzen - ein Aufrufer, der den Rückgabewert
        // ignoriert, soll keinen unauffällig falschen Wert bekommen (z.B. in der GUI).
        // Hier im sicherheitskritischen Pfad wird der Rückgabewert geprüft; die
        // lokalen Defaults sind nur ein zusätzliches Sicherheitsnetz für den Fall
        // eines Bugs in genau dieser Prüfung.
        float scale = DEFAULT_SCALE;
        float offset = DEFAULT_OFFSET;
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

bool SensorBase::getName(char *buffer, size_t len)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readString(CONFIG_KEY_SENSOR_NAME, buffer, len);
}

bool SensorBase::setName(const char *name, size_t len)
{
    if (len < SENSOR_NAME_LEN)
    {
        char ns[NvsStorage::NVS_KEY_LEN] = {};
        nvsNamespace(ns, sizeof(ns));
        NvsStorage::Session session(ns, false);
        return NvsStorage::writeString(CONFIG_KEY_SENSOR_NAME, name);
    }
    return false;
}

bool SensorBase::getScale(float &scale) const
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readFloat(CONFIG_KEY_SCALE, scale);
}

bool SensorBase::setScale(float scale)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeFloat(CONFIG_KEY_SCALE, scale);
}

bool SensorBase::getOffset(float &offset) const
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readFloat(CONFIG_KEY_OFFSET, offset);
}

bool SensorBase::setOffset(float offset)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeFloat(CONFIG_KEY_OFFSET, offset);
}

bool SensorBase::getPrecision(int &precision) const
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readInt(CONFIG_KEY_PRECISION, precision);
}

bool SensorBase::setPrecision(int precision)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeInt(CONFIG_KEY_PRECISION, precision);
}

bool SensorBase::getUnit(char *buffer, size_t len) const
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readString(CONFIG_KEY_UNIT, buffer, len);
}

bool SensorBase::setUnit(const char *unit, size_t len)
{
    if (len < SENSOR_UNIT_LEN)
    {
        char ns[NvsStorage::NVS_KEY_LEN] = {};
        nvsNamespace(ns, sizeof(ns));
        NvsStorage::Session session(ns, false);
        return NvsStorage::writeString(CONFIG_KEY_UNIT, unit);
    }
    return false;
}

void SensorBase::nvsNamespace(char *buffer, size_t len) const
{
    snprintf(buffer, len, "sensor_%d", _id);
}

void SensorBase::ensureDefaults()
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);

    float dummyScale;
    if (!NvsStorage::readFloat(CONFIG_KEY_SCALE, dummyScale))
    {
        NvsStorage::writeFloat(CONFIG_KEY_SCALE, DEFAULT_SCALE);
        NvsStorage::writeFloat(CONFIG_KEY_OFFSET, DEFAULT_OFFSET);
    }

    char dummyName[SENSOR_NAME_LEN];
    if (!NvsStorage::readString(CONFIG_KEY_SENSOR_NAME, dummyName, sizeof(dummyName)))
    {
        char defaultName[SENSOR_NAME_LEN] = {};
        snprintf(defaultName, sizeof(defaultName), "Sensor %d", _id);
        NvsStorage::writeString(CONFIG_KEY_SENSOR_NAME, defaultName);
    }
}
