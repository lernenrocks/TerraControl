#include "EntityBase.h"
#include <Arduino.h>
#include "NvsStorage.h"


// public:
EntityBase::EntityBase(const char *nvsNamespace, uint8_t idx) : _idx(idx)
{
    strlcpy(_nvsNamespace, nvsNamespace, sizeof(_nvsNamespace));
}
EntityBase::EntityBase(const char *nvsNamespace, uint8_t idx, const char *name) : _idx(idx)
{
    strlcpy(_nvsNamespace, nvsNamespace, sizeof(_nvsNamespace));
    setName(name);
}

bool EntityBase::remove()
{
    NvsStorage::Session session(nvsNamespace(), false);
    return NvsStorage::clearNamespace();
}

uint8_t EntityBase::getIndex() const
{
    return _idx;
}
bool EntityBase::getName(char *buffer, size_t len) const
{
        if (!buffer || len < ENTITY_NAME_LEN)
        return false;
    NvsStorage::Session session(nvsNamespace(),true);
    return NvsStorage::readString(CONFIG_KEY_ENTITY_NAME,buffer,len);

}
bool EntityBase::setName(const char *name)
{
    if (!name || strlen(name) >= ENTITY_NAME_LEN)
        return false;

    NvsStorage::Session session(nvsNamespace(),false);
    return NvsStorage::writeString(CONFIG_KEY_ENTITY_NAME,name);
}

// protected:
const char *EntityBase::nvsNamespace() const
{
    return _nvsNamespace;
}