#pragma once

#include "RelayBase.h"

constexpr const size_t RELAY_REQUEST_LEN = 256;

class ShellyGen2Relay : public RelayBase
{
public:
    ShellyGen2Relay(const char *nvsNamespace, uint8_t idx);
    ShellyGen2Relay(const char *nvsNamespace, uint8_t idx, JsonObjectConst data);
    ~ShellyGen2Relay() = default;

    bool buildSwitchRequest(bool on, char *buffer, size_t len) override;
    bool applyResponse(JsonObjectConst data) override;
};
