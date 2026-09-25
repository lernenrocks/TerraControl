#include "ApDevice.h"
#include "SecureUtils.h"
#include "NvsStorage.h"
#include <Arduino.h>
#include "Logger.h"
#include "EntityType.h"
#include "ShellyGen2Relay.h"

ApDevice::ApDevice(const uint8_t id) : _id(id)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    uint8_t entityList[UINT8_MAX] = {0};
    uint8_t entityCount = 0;
    {
        NvsStorage::Session session(ns, true);

        NvsStorage::readBytes(CONFIG_KEY_ENTITY_TYPES, entityList, sizeof(entityList));
        int count = 0;
        if (!NvsStorage::readInt(CONFIG_KEY_ENTITY_COUNT, count) || count <= 0 || count > UINT8_MAX)
        {
            count = 0;
            char msg[64] = {};
            snprintf(msg, sizeof(msg), "WiFi Device %d invalid entity count", _id);
            Logger::log(Logger::ErrorLevel::ERROR, msg);
        }
        else
        {
            entityCount = static_cast<uint8_t>(count);
        }
    }

    entities.reserve(entityCount);
    for (uint8_t i = 0; i < entityCount; i++)
    {
        EntityBase *entity = createEntity(static_cast<EntityType>(entityList[i]), i);
        if (entity)
            entities.push_back(entity);
    }
}
// Onboarding: builds a new device from the JSON the worker assembled:
// {
//   "type": "Shelly Gen2/3",         text of deviceTypeToString
//   "mac": "8cbfeaa03350",           12 hex characters, no separators
//   "username": "admin",
//   "password": "...",
//   "name": "Heating",               optional
//   "entities": [                    one object per entity, the position is the index
//     { "type": "Shelly Gen3 Plug S", ... config of this entity ... }
//   ]
// }
// Nothing is written unless everything is valid. A device that failed has no MAC in the NVS,
// the caller finds out with getMac(). The password is not copied, the owner of the JSON wipes it.
ApDevice::ApDevice(const uint8_t id, JsonObjectConst data) : _id(id)
{
    const char *macText = data["mac"] | "";
    const char *username = data["username"] | "";
    const char *password = data["password"] | "";
    const char *name = data["name"] | "";
    const DeviceType deviceType = deviceTypeFromString(data["type"] | "");
    JsonArrayConst entityData = data["entities"].as<JsonArrayConst>();
    const size_t entityCount = entityData.size();

    uint8_t mac[MAC_LEN] = {};
    uint8_t entityTypes[UINT8_MAX] = {0};

    bool valid = strlen(macText) == MAC_STRING_LEN - 1 && NetworkUtils::macCharToBytes(mac, macText);
    valid = valid && strlen(username) > 0 && strlen(username) < DEVICE_USERNAME_LEN;
    valid = valid && strlen(password) > 0 && strlen(password) < DEVICE_PASSWORD_LEN;
    valid = valid && strlen(name) < DEVICE_NAME_LEN;
    valid = valid && deviceType != DeviceType::NONE;
    valid = valid && entityCount > 0 && entityCount <= UINT8_MAX;
    if (valid)
    {
        uint8_t index = 0;
        for (JsonVariantConst item : entityData)
        {
            const EntityType type = entityTypeFromString(item["type"] | "");
            if (type == EntityType::NONE)
            {
                valid = false;
                break;
            }
            entityTypes[index++] = static_cast<uint8_t>(type);
        }
    }
    if (!valid)
    {
        char msg[64] = {};
        snprintf(msg, sizeof(msg), "WiFi Device %d invalid onboarding data", _id);
        Logger::log(Logger::ErrorLevel::ERROR, msg);
        return;
    }

    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    bool written = false;
    {
        NvsStorage::Session session(ns, false);
        written = NvsStorage::writeBytes(CONFIG_KEY_DEVICE_MAC, mac, sizeof(mac)) &&
                  NvsStorage::writeString(CONFIG_KEY_DEVICE_USERNAME, username) &&
                  NvsStorage::writeString(CONFIG_KEY_DEVICE_PASSWORD, password) &&
                  NvsStorage::writeInt(CONFIG_KEY_DEVICE_TYPE, static_cast<int>(deviceType)) &&
                  NvsStorage::writeInt(CONFIG_KEY_ENTITY_COUNT, static_cast<int>(entityCount)) &&
                  NvsStorage::writeBytes(CONFIG_KEY_ENTITY_TYPES, entityTypes, entityCount);
        if (written && strlen(name) > 0)
            written = NvsStorage::writeString(CONFIG_KEY_DEVICE_NAME, name);
        if (!written)
            NvsStorage::clearNamespace(); // no half written device
    }
    if (!written)
    {
        char msg[64] = {};
        snprintf(msg, sizeof(msg), "WiFi Device %d could not be written", _id);
        Logger::log(Logger::ErrorLevel::ERROR, msg);
        return;
    }

    // The session is closed, the entities may open their own
    entities.reserve(entityCount);
    uint8_t index = 0;
    for (JsonVariantConst item : entityData)
    {
        EntityBase *entity = createEntity(static_cast<EntityType>(entityTypes[index]),
                                           index, item.as<JsonObjectConst>());
        if (entity)
            entities.push_back(entity);
        index++;
    }
}

// Destroying a device removes it from the system: entities first, the own namespace last.
// Only the registry creates and deletes devices (new/delete), never a device on the stack.
ApDevice::~ApDevice()
{
    for (EntityBase *entity : entities)
    {
        if (entity)
        {
            entity->remove(); // clears the NVS namespace of the entity, failures are logged by NvsStorage
            delete entity;
        }
    }
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    NvsStorage::clearNamespace();
}

// The device answered a status request: it is reachable, remember its signal strength.
bool ApDevice::applyResponse(JsonObjectConst data)
{
    uint8_t deviceMac[MAC_LEN] = {0};
    uint8_t responseMac[MAC_LEN] = {0};
    getMac(deviceMac, sizeof(deviceMac));
    if (!data.containsKey("sys") || !data["sys"].containsKey("mac"))
    {
        return false;
    }
    NetworkUtils::macCharToBytes(responseMac, data["sys"]["mac"]);
    if (memcmp(deviceMac, responseMac, sizeof(deviceMac)) != 0)
    {
        setOnline(false);
        char msg[64]={0};
        snprintf(msg,sizeof(msg),"ApDevice id:%d, type:%s lost",_id,deviceTypeToString(getDeviceType()));
        Logger::log(Logger::ErrorLevel::WARN,msg);
        return false;
    }
    _rssi = data["wifi"]["rssi"] | RSSI_UNKNOWN;
    setOnline(true);
    for(EntityBase *entity : entities){
        if(entity) { entity->applyResponse(data); }
    }
    return true;
}

int ApDevice::getRssi() const { return _rssi; }

uint8_t ApDevice::getId() const { return _id; }

// The credentials are read from the NVS on demand and copied into the buffer of the caller.
// The caller wipes the buffer after use (SecureUtils::ensureZero).
bool ApDevice::getMac(uint8_t *buffer, size_t len) const
{
    if (!buffer || len < MAC_LEN)
        return false;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readBytes(CONFIG_KEY_DEVICE_MAC, buffer, MAC_LEN);
}

bool ApDevice::getUsername(char *buffer, size_t len) const
{
    if (!buffer || len < DEVICE_USERNAME_LEN)
        return false;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readString(CONFIG_KEY_DEVICE_USERNAME, buffer, len);
}

bool ApDevice::getPassword(char *buffer, size_t len) const
{
    if (!buffer || len < DEVICE_PASSWORD_LEN)
        return false;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readString(CONFIG_KEY_DEVICE_PASSWORD, buffer, len);
}

DeviceType ApDevice::getDeviceType() const
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    int value = 0;
    if (!NvsStorage::readInt(CONFIG_KEY_DEVICE_TYPE, value) || value < 0 || value > UINT8_MAX)
        return DeviceType::NONE;
    return static_cast<DeviceType>(value);
}

const char *ApDevice::getDeviceTypeString() const
{
    return deviceTypeToString(getDeviceType());
}

bool ApDevice::getName(char *buffer, size_t len) const
{
    if (!buffer || len < DEVICE_NAME_LEN)
        return false;
    {
        char ns[NvsStorage::NVS_KEY_LEN] = {};
        nvsNamespace(ns, sizeof(ns));
        NvsStorage::Session session(ns, true);
        if (NvsStorage::readString(CONFIG_KEY_DEVICE_NAME, buffer, len))
            return true;
    }
    // readString clears the buffer for a missing key, the default is not written to the NVS
    snprintf(buffer, len, "WiFi Device %d", _id);
    return true;
}

bool ApDevice::setName(const char *name)
{
    if (!name || strlen(name) >= DEVICE_NAME_LEN)
        return false;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeString(CONFIG_KEY_DEVICE_NAME, name);
}

bool ApDevice::isOnline() const { return _online; }

void ApDevice::setOnline(bool online)
{
    _online = online;
    if (online)
        _lastSeen = millis();
}

unsigned long ApDevice::getLastSeen() const { return _lastSeen; }

// The entities are created in the constructor and never change afterwards, reading needs no lock.
EntityBase *ApDevice::getEntity(const uint8_t idx) const
{
    if (idx >= entities.size())
        return nullptr;
    return entities[idx];
}

uint8_t ApDevice::getEntityCount() const
{
    return static_cast<uint8_t>(entities.size());
}

// private
void ApDevice::nvsNamespace(char *buffer, size_t len) const
{
    snprintf(buffer, len, "apDev_%d", _id);
}

EntityBase *ApDevice::createEntity(EntityType type, uint8_t idx, JsonObjectConst data)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    snprintf(ns, sizeof(ns), "dev%d_ent%d", _id, idx);

    EntityBase *entity = nullptr;
    switch (type)
    {
    case EntityType::SHELLY_GEN2_RELAY: // fall through
    case EntityType::SHELLY_GEN3_RELAY:
    case EntityType::SHELLY_GEN3_PLUG_S:
        if (data.isNull())
            entity = new (std::nothrow) ShellyGen2Relay(ns, idx);
        else
            entity = new (std::nothrow) ShellyGen2Relay(ns, idx, data);
        break;
    default:
        break;
    }
    if (!entity)
    {
        char msg[64] = {};
        snprintf(msg, sizeof(msg), "WiFi Device %d entity %d creation failed", _id, idx);
        Logger::log(Logger::ErrorLevel::ERROR, msg);
    }
    return entity;
}

