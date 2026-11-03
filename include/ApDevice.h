#pragma once

#include <Arduino.h>
#include <atomic>
#include "NvsStorage.h"
#include "NetworkUtils.h"
#include <vector>
#include "EntityBase.h"
#include "DeviceType.h"
#include "EntityType.h"
#include <ArduinoJson.h>

constexpr const size_t DEVICE_MAX = 10; // AP-Verbindungslimit, geteilt zwischen Relais- und Remote-Sensor-Geräten
constexpr const size_t DEVICE_NAME_LEN = 32;
constexpr const size_t DEVICE_USERNAME_LEN = 32;
constexpr const size_t DEVICE_PASSWORD_LEN = 64;
constexpr const char CONFIG_KEY_DEVICE_NAME[] = "name";
constexpr const char CONFIG_KEY_DEVICE_MAC[] = "mac";
constexpr const char CONFIG_KEY_DEVICE_USERNAME[] = "username";
constexpr const char CONFIG_KEY_DEVICE_PASSWORD[] = "password";
constexpr const char CONFIG_KEY_DEVICE_TYPE[] = "devType";
constexpr const char CONFIG_KEY_ENTITY_COUNT[] = "entCount";

class ApDevice
{
public:
    
    ~ApDevice(); // frees the object and its entities, NVS stays untouched
    // a copy would share the entity pointers - both destructors would delete them
    ApDevice(const ApDevice &) = delete;
    ApDevice &operator=(const ApDevice &) = delete;
    void erase();
    static ApDevice *recover(uint8_t id);
    static ApDevice *onboard(uint8_t id, JsonObjectConst data);

    uint8_t getId() const;
    bool getMac(uint8_t *buffer, size_t len) const;
    bool getUsername(char *buffer, size_t len) const;
    bool getPassword(char *buffer, size_t len) const;
    DeviceType getDeviceType() const;
    const char *getDeviceTypeString() const; // text literal from deviceTypeToString, safe to hold
    bool getName(char *buffer, size_t len) const;
    bool setName(const char *name);
    bool applyResponse(JsonObjectConst data);
    bool isOnline() const;
    void setOnline(bool online);
    unsigned long getLastSeen() const;
    // The vector position always equals the NVS entity index (e<idx>_): an entity that could
    // not be loaded keeps its position as nullptr instead of letting the following ones shift.
    // getEntity() may therefore return nullptr for idx < getEntityCount() - the caller must
    // treat that like a missing entity (rule invalid), never fall back to another index.
    EntityBase *getEntity(const uint8_t idx) const;
    uint8_t getEntityCount() const; // number of positions, including entities that failed to load
    // Forwards to the entity's own EntityBase::getType() - the type lives only there now
    // (RAM-only, set once at construction), no separate copy kept on ApDevice.
    EntityType getEntityType(const uint8_t idx) const;
    static bool hasData(uint8_t id);

private:
    explicit ApDevice(const uint8_t id);
    std::vector<EntityBase *> entities;
    static void nvsNamespace(uint8_t id, char *buffer, size_t len);
    const uint8_t _id;
    // NVS values mirrored in RAM - loaded once, NVS only touched again by setters.
    // Not atomic: access is protected by ApDeviceRegistry::Lock. The password is a secret
    // and deliberately NOT mirrored - it is read from the NVS on every getPassword().    
    uint8_t _mac[MAC_LEN] = {};
    char _username[DEVICE_USERNAME_LEN] = {};
    char _name[DEVICE_NAME_LEN] = {};
    DeviceType _deviceType = DeviceType::NONE;
    std::atomic<bool> _online{false};
    std::atomic<unsigned long> _lastSeen{0};

    bool load();
    bool write(JsonObjectConst data);

    EntityBase *recoverEntity(EntityType type, uint8_t idx);
    EntityBase *onboardEntity(JsonObjectConst data, uint8_t idx);
};
