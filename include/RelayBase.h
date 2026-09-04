#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"

constexpr const uint32_t DEFAULT_FAILSAFE_TIMEOUT_SEC = 300;
constexpr const size_t RELAY_NAME_LEN = 32;
constexpr const size_t RELAY_PASSWORD_LEN = 64;
constexpr const char CONFIG_KEY_FAILSAFE_TIMEOUT[] = "failsafeSec";
constexpr const char CONFIG_KEY_RELAY_NAME[] = "name";
constexpr const char CONFIG_KEY_RELAY_PASSWORD[] = "password";
constexpr const char CONFIG_KEY_RELAY_ID[] = "id";
constexpr const char CONFIG_KEY_RELAY_TYPE[] = "type";

class RelayBase
{
public:
    RelayBase(uint8_t id, const char *type);
    virtual ~RelayBase() = default;

    virtual bool buildSwitchRequest(bool on, char *buffer, size_t len) = 0;
    virtual bool buildStatusRequest(char *buffer, size_t len) = 0;

    virtual bool applyResponse(JsonObjectConst data)=0;
    bool getName(char *buffer, size_t len);
    bool setName(const char *name, size_t len);
    bool getCurrentState(bool &isOn) const;
    
    bool getFailsafeSeconds(uint32_t &buffer);
    bool setFailsafeSeconds(int seconds);


    uint8_t getId() const;
protected:
    void nvsNamespace(char *buffer, size_t len) const;
    bool getPassword(char *buffer, size_t len);
    bool setPassword(const char *password, size_t len);

private:
    uint8_t id;
    const char *type;
    bool currentState = false;
    bool valid = false;
};