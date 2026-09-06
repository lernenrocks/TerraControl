#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"

constexpr const int DEFAULT_FAILSAFE_TIMEOUT_SEC = 300;
constexpr const size_t RELAY_NAME_LEN = 32;
constexpr const char CONFIG_KEY_FAILSAFE_TIMEOUT[] = "failsafeSec";
constexpr const char CONFIG_KEY_RELAY_NAME[] = "name";


class RelayBase
{
public:
    RelayBase(uint8_t id, const char *type ,uint8_t relayIdx);
    virtual ~RelayBase() = default;
    bool remove();

    virtual bool buildSwitchRequest(bool on, char *buffer, size_t len) = 0;
    virtual bool buildStatusRequest(char *buffer, size_t len) = 0;

    virtual bool applyResponse(JsonObjectConst data)=0;
    
    bool getName(char *buffer, size_t len)const;
    bool setName(const char *name, size_t len);
    bool getCurrentState(bool &isOn) const;
    bool getFailsafeSeconds(int &buffer)const;
    bool setFailsafeSeconds(int seconds);
    uint8_t getRelayIdx() const;
    uint8_t getId() const;
    bool isFound() const;
protected:
    void nvsNamespace(char *buffer, size_t len) const;

private:
    void ensureDefaults();
    uint8_t _id;
    const char *_type;
    uint8_t _relayIdx;
    bool currentState = false;
    bool found = false;
};