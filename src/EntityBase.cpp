#include "EntityBase.h"
#include <Arduino.h>
#include "NvsStorage.h"

// public:
EntityType EntityBase::readType(const char *deviceNamespace, uint8_t idx)
{
    char key[NvsStorage::NVS_KEY_LEN] = {};
    buildKey(idx, CONFIG_KEY_ENTITY_TYPE, key, sizeof(key));
    NvsStorage::Session session(deviceNamespace, true);
    int value = 0;
    if (!NvsStorage::readInt(key, value) || value < 0 || value > UINT8_MAX)
        return EntityType::NONE;
    return static_cast<EntityType>(value);
}

uint8_t EntityBase::getIndex() const
{
    return _idx;
}
EntityType EntityBase::getType() const
{
    return _type;
}
bool EntityBase::getName(char *buffer, size_t len) const
{
    if (!buffer || len < ENTITY_NAME_LEN)
        return false;
    strlcpy(buffer, _name, len);
    return true;
}
bool EntityBase::setName(const char *name)
{
    if (!name || strlen(name) >= ENTITY_NAME_LEN)
        return false;
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_ENTITY_NAME, key, sizeof(key));
    NvsStorage::Session session(nvsNamespace(), false);
    if (!NvsStorage::writeString(key, name))
        return false;
    strlcpy(_name, name, sizeof(_name)); // RAM only after the NVS write succeeded
    return true;
}

// protected:
EntityBase::EntityBase(const char *deviceNamespace, uint8_t idx, EntityType type) : _idx(idx), _type(type)
{
    strlcpy(_nvsNamespace, deviceNamespace, sizeof(_nvsNamespace));
}
bool EntityBase::load()
{
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_ENTITY_NAME, key, sizeof(key));
    {
        NvsStorage::Session session(_nvsNamespace, true);
        NvsStorage::readString(key, _name, sizeof(_name));
    }
    return true;
}

bool EntityBase::write(JsonObjectConst data)
{
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_ENTITY_TYPE, key, sizeof(key));
    {
        NvsStorage::Session session(_nvsNamespace, false);
        NvsStorage::writeInt(key, static_cast<int>(_type));
    }
    return setName(data["name"] | "");
}

const char *EntityBase::nvsNamespace() const
{
    return _nvsNamespace;
}

void EntityBase::nvsKey(const char *key, char *buffer, size_t len) const
{
    buildKey(_idx, key, buffer, len);
}

// private:
void EntityBase::buildKey(uint8_t idx, const char *key, char *buffer, size_t len)
{
    snprintf(buffer, len, "e%u_%s", idx, key);
}
