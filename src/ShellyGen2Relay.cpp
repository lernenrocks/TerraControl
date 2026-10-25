#include "ShellyGen2Relay.h"
#include <new>

ShellyGen2Relay *ShellyGen2Relay::recover(const char *nvsNamespace, uint8_t idx, EntityType type)
{
    ShellyGen2Relay *relay = new (std::nothrow) ShellyGen2Relay(nvsNamespace, idx, type);
    if (!relay)
        return nullptr;
    if (!relay->load())
    {
        delete relay;
        return nullptr;
    }
    return relay;
}

ShellyGen2Relay *ShellyGen2Relay::onboard(const char *nvsNamespace, uint8_t idx, EntityType type, JsonObjectConst data)
{
        ShellyGen2Relay *relay = new (std::nothrow) ShellyGen2Relay(nvsNamespace, idx, type);
    if (!relay)
        return nullptr;
    if (!relay->write(data))
    {
        delete relay;
        return nullptr;
    }
    return relay;
}


bool ShellyGen2Relay::buildSwitchRequest(bool on, char *buffer, size_t len)
{
    if (len < RELAY_REQUEST_LEN)
        return false;
    if (on)
    {
        snprintf(buffer, len, "rpc/Switch.Set?id=%u&on=true&toggle_after=%d",
                 RelayBase::getOutputIdx(), getFailsafeSeconds());
    }
    else
    {
        snprintf(buffer, len, "rpc/Switch.Set?id=%u&on=false",
                 RelayBase::getOutputIdx());
    }

    return true;
}

bool ShellyGen2Relay::applyResponse(JsonObjectConst data)
{
    char key[16] = {};
    snprintf(key, sizeof(key), "switch:%u", getOutputIdx());
    JsonVariantConst mine = data[key];
    JsonVariantConst output = mine["output"];
    if (mine.isNull())
        return false; // Index passt nicht zur Antwort, keinen Zustand raten
    if(!output.is<bool>())
        return false;
    actualOn = output.as<bool>();
    return true;
}

// private
ShellyGen2Relay::ShellyGen2Relay(const char *nvsNamespace, uint8_t idx, EntityType type)
    : RelayBase(nvsNamespace, idx, type) {}
