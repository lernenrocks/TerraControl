#pragma once

#include <Arduino.h>
#include <atomic>
#include <mutex>
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
constexpr const char CONFIG_KEY_ENTITY_TYPES[] = "entTypes";
constexpr const int RSSI_UNKNOWN = 0; // RSSI is always negative, 0 means no status response yet

class ApDevice
{
public:
    explicit ApDevice(const uint8_t id);
    ApDevice(const uint8_t id, JsonObjectConst data);

    ~ApDevice(); // removes the device and its entities from the NVS and frees them

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
    int getRssi() const;
    EntityBase *getEntity(const uint8_t idx) const;
    uint8_t getEntityCount() const;

private:
    std::vector<EntityBase *> entities;
    void nvsNamespace(char *buffer, size_t len) const;
    const uint8_t _id;
    std::atomic<bool> _online{false};
    std::atomic<unsigned long> _lastSeen{0};
    std::atomic<int> _rssi{RSSI_UNKNOWN};

    EntityBase *createEntity(EntityType type, uint8_t idx, JsonObjectConst data = JsonObjectConst());
};
