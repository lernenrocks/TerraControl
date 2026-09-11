#pragma once

#include "RelayBase.h"

class ShellyGen2Relay : public RelayBase
{
public:
    ShellyGen2Relay();
    ~ShellyGen2Relay() = default;

    bool buildSwitchRequest(bool on, char *buffer, size_t len) override;
    bool buildStatusRequest(char *buffer, size_t len) override;

    bool applyResponse(JsonObjectConst data) override;
};