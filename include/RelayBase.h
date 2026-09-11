#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "NvsStorage.h"
#include <atomic>

constexpr const int DEFAULT_FAILSAFE_TIMEOUT_SEC = 300;
constexpr const size_t RELAY_NAME_LEN = 32;
constexpr const size_t RELAY_TYPE_LEN = 32;
constexpr const size_t MAC_LEN = 6;
constexpr const size_t RELAY_PASSWORD_LEN = 64;
constexpr const char CONFIG_KEY_FAILSAFE_TIMEOUT[] = "failsafeSec";
constexpr const char CONFIG_KEY_RELAY_NAME[] = "name";
constexpr const char CONFIG_KEY_RELAY_TYPE[] = "type";
constexpr const char CONFIG_KEY_RELAY_PASSWORD[] = "password";
constexpr const char CONFIG_KEY_RELAY_MAC[] = "mac";
constexpr const char CONFIG_KEY_RELAY_IDX[] = "relayIdx";

constexpr const size_t RELAY_REQUEST_LEN = 256;

class RelayBase
{
public:
    RelayBase();
    virtual ~RelayBase() = default;
    bool remove();
    bool recover(uint8_t id);
    bool onboard(uint8_t id, uint8_t relayIdx, const uint8_t *mac, const char *password, const char *type);

    virtual bool buildSwitchRequest(bool on, char *buffer, size_t len) = 0;
    virtual bool buildStatusRequest(char *buffer, size_t len) = 0;

    virtual bool applyResponse(JsonObjectConst data) = 0;

    const char *getName() const;
    bool setName(const char *name);
    const char *getType() const;
    const char *getPassword() const;
    bool getOutputOn(bool &isOn) const;
    int getFailsafeSeconds() const;
    bool setFailsafeSeconds(int seconds);
    uint8_t getRelayIdx() const;
    uint8_t getId() const;
    const uint8_t *getMac() const;
    bool isFound() const;
    void markLost();

protected:
    void nvsNamespace(char *buffer, size_t len) const;
    std::atomic<bool> found{false};
    std::atomic<bool> outputOn{false};

private:
    void writeDefaults();
    bool writeIdentity();
    bool writeType(const char *type);
    bool writePassword(const char *password);
    uint8_t _id = 0;
    uint8_t _relayIdx = 0;
    uint8_t _mac[MAC_LEN] = {};
    char _name[RELAY_NAME_LEN] = {};
    char _type[RELAY_TYPE_LEN] = {};
    char _password[RELAY_PASSWORD_LEN] = {};
    int _failsafeSeconds = DEFAULT_FAILSAFE_TIMEOUT_SEC;
};
