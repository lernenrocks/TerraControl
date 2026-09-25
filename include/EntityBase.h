#pragma once

#include "NvsStorage.h"
#include <ArduinoJson.h>

constexpr const uint8_t ENTITY_NAME_LEN = 32;
constexpr const char CONFIG_KEY_ENTITY_NAME[] = "name";

class EntityBase
{
public:
    EntityBase(const char *nvsNamespace, uint8_t idx);
    EntityBase(const char *nvsNamespace, uint8_t idx, const char *name);
    virtual ~EntityBase() = default;

    bool remove();

    virtual bool applyResponse(JsonObjectConst data) = 0;

    uint8_t getIndex() const;
    bool getName(char *buffer, size_t len) const;
    bool setName(const char *name);

protected:
    const char *nvsNamespace() const;

private:
    uint8_t _idx = 0;
    char _nvsNamespace[NvsStorage::NVS_KEY_LEN]={};
};