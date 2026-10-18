#include "ApDevice.h"
#include "SecureUtils.h"
#include "NvsStorage.h"
#include <Arduino.h>
#include "Logger.h"
#include "EntityType.h"
#include "ShellyGen2Relay.h"

ApDevice::ApDevice(const uint8_t id) : _id(id) {}

ApDevice *ApDevice::recover(uint8_t id)
{
    ApDevice *device = new (std::nothrow) ApDevice(id);
    if (!device)
        return nullptr;
    if (!device->load())
    {
        delete device;
        return nullptr;
    }
    return device;
}
ApDevice *ApDevice::onboard(uint8_t id, JsonObjectConst data)
{
    ApDevice *device = new (std::nothrow) ApDevice(id);
    if (!device)
        return nullptr;
    if (!device->write(data))
    {
        delete device;
        return nullptr;
    }
    return device;
}

bool ApDevice::load()
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    uint8_t entityCount = 0;
    {
        NvsStorage::Session session(ns, true);

        if (!NvsStorage::readBytes(CONFIG_KEY_DEVICE_MAC, _mac, sizeof(_mac)))
            return false;
        if (!NvsStorage::readString(CONFIG_KEY_DEVICE_USERNAME, _username, sizeof(_username)))
        {
            char msg[64] = {};
            snprintf(msg, sizeof(msg), "no Username in ApDevice id: %d", _id);
            Logger::log(Logger::ErrorLevel::ERROR, msg);
        }
        NvsStorage::readString(CONFIG_KEY_DEVICE_NAME, _name, sizeof(_name)); // optional, empty if missing
        int type = 0;
        if (NvsStorage::readInt(CONFIG_KEY_DEVICE_TYPE, type) && type >= 0 && type <= UINT8_MAX)
        {
            _deviceType = static_cast<DeviceType>(type);
        }
        else
        {
            char msg[64] = {};
            snprintf(msg, sizeof(msg), "no type in ApDevice id: %d", _id);
            Logger::log(Logger::ErrorLevel::ERROR, msg);
            _deviceType = DeviceType::NONE;
        }
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

    // The device session is closed - each entity reads its prefixed keys from the device
    // namespace in its own session, one at a time (NvsStorage's semaphore does not nest).
    entities.reserve(entityCount);
    for (uint8_t i = 0; i < entityCount; i++)
    {
        EntityBase *entity = recoverEntity(EntityBase::readType(ns, i), i);
        if (!entity)
        {
            char msg[64] = {};
            snprintf(msg, sizeof(msg), "WiFi Decive %d entity %d could not be loaded", _id, i);
            Logger::log(Logger::ErrorLevel::ERROR, msg);
            continue;
        }
        entities.push_back(entity);
    }
    return true;
}
EntityBase *ApDevice::recoverEntity(EntityType type, uint8_t idx)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    switch (type)
    {
    case EntityType::SHELLY_GEN2_RELAY: // fall through
    case EntityType::SHELLY_GEN3_RELAY:
    case EntityType::SHELLY_GEN3_PLUG_S:
        return ShellyGen2Relay::recover(ns, idx, type);
    default:
        return nullptr;
    }
}
EntityBase *ApDevice::onboardEntity(JsonObjectConst data, uint8_t idx)
{
    EntityType type = entityTypeFromString(data["type"] | "");
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    switch (type)
    {
    case EntityType::SHELLY_GEN2_RELAY: // fall through
    case EntityType::SHELLY_GEN3_RELAY:
    case EntityType::SHELLY_GEN3_PLUG_S:
        return ShellyGen2Relay::onboard(ns, idx, type, data);
    default:
        return nullptr;
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
// Device attributes are checked before writing
bool ApDevice::write(JsonObjectConst data)
{
    const char *macText = data["mac"] | "";
    const char *username = data["username"] | "";
    const char *password = data["password"] | "";
    const char *name = data["name"] | "";
    const DeviceType deviceType = deviceTypeFromString(data["type"] | "");
    JsonArrayConst entityData = data["entities"].as<JsonArrayConst>();
    const size_t entityCount = entityData.size();

    uint8_t mac[MAC_LEN] = {};

    bool valid = strlen(macText) == MAC_STRING_LEN - 1 && NetworkUtils::macCharToBytes(mac, macText);
    valid = valid && strlen(username) > 0 && strlen(username) < DEVICE_USERNAME_LEN;
    valid = valid && strlen(password) > 0 && strlen(password) < DEVICE_PASSWORD_LEN;
    valid = valid && strlen(name) < DEVICE_NAME_LEN;
    valid = valid && deviceType != DeviceType::NONE;
    valid = valid && entityCount > 0 && entityCount <= UINT8_MAX;
    if (!valid)
    {
        char msg[64] = {};
        snprintf(msg, sizeof(msg), "WiFi Device %d invalid onboarding data", _id);
        Logger::log(Logger::ErrorLevel::ERROR, msg);
        return false;
    }

    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    {
        NvsStorage::Session session(ns, false);
        NvsStorage::writeBytes(CONFIG_KEY_DEVICE_MAC, mac, sizeof(mac));
        NvsStorage::writeString(CONFIG_KEY_DEVICE_USERNAME, username);
        NvsStorage::writeString(CONFIG_KEY_DEVICE_PASSWORD, password);
        NvsStorage::writeInt(CONFIG_KEY_DEVICE_TYPE, static_cast<int>(deviceType));
        NvsStorage::writeInt(CONFIG_KEY_ENTITY_COUNT, static_cast<int>(entityCount));
        // entity types are written by each entity under its own prefixed keys, not here
        if (strlen(name) > 0)
            NvsStorage::writeString(CONFIG_KEY_DEVICE_NAME, name);
    }
    //mirror into RAM
    memcpy(_mac, mac, sizeof(_mac));
    strlcpy(_username, username, sizeof(_username));
    strlcpy(_name, name, sizeof(_name));
    _deviceType = deviceType;

    // The session is closed, the entities may open their own
    entities.reserve(entityCount);
    uint8_t index = 0;
    for (JsonVariantConst item : entityData)
    {
        EntityBase *entity = onboardEntity(item.as<JsonObjectConst>(),index);
        if (!entity)
            return false;
        entities.push_back(entity);
        index++;
    }
    return true;
}

// Destroying a device removes it from the system. The entities' keys live in the device
// namespace, so clearing it once removes the device and all its entities from the NVS.
// Only the registry creates and deletes devices (new/delete), never a device on the stack.
ApDevice::~ApDevice()
{
    for (EntityBase *entity : entities)
        delete entity; // delete on nullptr is a no-op
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    NvsStorage::clearNamespace();
}

// The device answered a status request: it is reachable. MAC verification happens
// in the NetworkWorker before this is called - by the time applyResponse() runs, the
// caller has already confirmed this response really came from this device.
bool ApDevice::applyResponse(JsonObjectConst data)
{
    setOnline(true);
    for (EntityBase *entity : entities)
    {
        if (entity)
        {
            entity->applyResponse(data);
        }
    }
    return true;
}

uint8_t ApDevice::getId() const { return _id; }

bool ApDevice::getMac(uint8_t *buffer, size_t len) const
{
    if (!buffer || len < MAC_LEN)
        return false;
    memcpy(buffer, _mac, MAC_LEN);
    return true;
}

bool ApDevice::getUsername(char *buffer, size_t len) const
{
    if (!buffer || len < DEVICE_USERNAME_LEN || _username[0] == '\0')
        return false;
    strlcpy(buffer, _username, len);
    return true;
}

// The password is a secret: read from the NVS on demand, never kept in RAM by this class.
// The caller wipes the buffer after use (SecureUtils::ensureZero).
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
    return _deviceType;
}

const char *ApDevice::getDeviceTypeString() const
{
    return deviceTypeToString(getDeviceType());
}

bool ApDevice::getName(char *buffer, size_t len) const
{
    if (!buffer || len < DEVICE_NAME_LEN)
        return false;
    if (_name[0] != '\0')
        strlcpy(buffer, _name, len);
    else // no name set, the default is not written to the NVS
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
    if (!NvsStorage::writeString(CONFIG_KEY_DEVICE_NAME, name))
        return false;
    strlcpy(_name, name, sizeof(_name)); // RAM only after the NVS write succeeded
    return true;
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

EntityType ApDevice::getEntityType(const uint8_t idx) const
{
    EntityBase *entity = getEntity(idx);
    return entity ? entity->getType() : EntityType::NONE;
}

// private
void ApDevice::nvsNamespace(char *buffer, size_t len) const
{
    snprintf(buffer, len, "apDev_%d", _id);
}
