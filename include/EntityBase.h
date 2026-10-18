#pragma once

#include "NvsStorage.h"
#include "EntityType.h"
#include <ArduinoJson.h>

constexpr const uint8_t ENTITY_NAME_LEN = 32;
constexpr size_t ENTITY_KEY_NAME_MAX = 10;   // "e254_" + 10 = 15 Zeichen
constexpr const char CONFIG_KEY_ENTITY_NAME[] = "name";
static_assert(sizeof(CONFIG_KEY_ENTITY_NAME) - 1 <= ENTITY_KEY_NAME_MAX, "entity key name too long");
constexpr const char CONFIG_KEY_ENTITY_TYPE[] = "type";
static_assert(sizeof(CONFIG_KEY_ENTITY_TYPE) - 1 <= ENTITY_KEY_NAME_MAX, "entity key name too long");


// NVS layout: an entity has no namespace of its own - it lives in its device's namespace,
// every key prefixed with its index ("e<idx>_<key>", e.g. "e0_name"). The number of
// namespaces therefore does not grow with the number of entities (NVS limit: 254 per partition).
// NVS keys are limited to 15 characters: with the longest prefix "e254_" a key name may have 10,
// so every entity index a uint8_t can hold works. No entity key name may exceed 10 characters.
class EntityBase
{
public:
    virtual ~EntityBase() = default;
    // Lets the device pick the concrete class at boot, before the entity exists.
    static EntityType readType(const char *deviceNamespace, uint8_t idx);
    virtual bool applyResponse(JsonObjectConst data) = 0;

    uint8_t getIndex() const;
    // Loaded once at construction, RAM-only afterwards - no NVS access on every poll.
    EntityType getType() const;
    bool getName(char *buffer, size_t len) const;
    bool setName(const char *name);

protected:
    EntityBase(const char *deviceNamespace, uint8_t idx, EntityType type);
    virtual bool load();
    virtual bool write(JsonObjectConst data);
    const char *nvsNamespace() const;
    // Builds this entity's prefixed key ("e<idx>_<key>")
    void nvsKey(const char *key, char *buffer, size_t len) const;

private:
    static void buildKey(uint8_t idx, const char *key, char *buffer, size_t len);
    uint8_t _idx = 0;
    EntityType _type=EntityType::NONE;
    char _nvsNamespace[NvsStorage::NVS_KEY_LEN] = {};
    // NVS value mirrored in RAM, protected by ApDeviceRegistry::Lock (not atomic)
    char _name[ENTITY_NAME_LEN] = {};
};
