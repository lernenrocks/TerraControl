#include "ShellyGen2Relay.h"

ShellyGen2Relay::ShellyGen2Relay()
    : RelayBase() {}

bool ShellyGen2Relay::buildSwitchRequest(bool on, char *buffer, size_t len)
{
    if(len<RELAY_REQUEST_LEN)
        return false;
    snprintf(buffer,len, "rpc/Switch.Set?id=%d&on=%s&toggle_after=%d",
        RelayBase::getRelayIdx(),on ? "true":"false",getFailsafeSeconds());
    return true;
}

bool ShellyGen2Relay::buildStatusRequest(char *buffer, size_t len)
{
    if(len <RELAY_REQUEST_LEN)
        return false;
    snprintf(buffer, len, "rpc/Switch.GetStatus?id=%d", RelayBase::getRelayIdx());
    return true; 
}

bool ShellyGen2Relay::applyResponse(JsonObjectConst data)
{
    found=true;
    outputOn = data["output"] | false;
    return true;
}
