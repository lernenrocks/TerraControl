#pragma once

#include "ApDeviceRelayBase.h"
#include "EntityType.h"
#include "Arduino.h"
#include "ArduinoJson.h"

constexpr const size_t RELAY_REQUEST_LEN = 256;

class ShellyGen2Relay : public ApDeviceRelayBase
{
public:
    ~ShellyGen2Relay() = default;
    static ShellyGen2Relay *recover(const char *nvsNamespace, uint8_t idx, EntityType type);
    static ShellyGen2Relay *onboard(const char *nvsNamespace, uint8_t idx, EntityType type, JsonObjectConst data);
    bool buildSwitchRequest(bool on, char *buffer, size_t len) override;
    bool applyResponse(JsonObjectConst data) override;

private:
    ShellyGen2Relay(const char *nvsNamespace, uint8_t idx, EntityType type);
};
