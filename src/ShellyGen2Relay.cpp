#include "ShellyGen2Relay.h"

ShellyGen2Relay::ShellyGen2Relay(const char *nvsNamespace, uint8_t idx)
    : RelayBase(nvsNamespace, idx) {}

ShellyGen2Relay::ShellyGen2Relay(const char *nvsNamespace, uint8_t idx, JsonObjectConst data)
    : RelayBase(nvsNamespace, idx, data["name"] | "") {}

bool ShellyGen2Relay::buildSwitchRequest(bool on, char *buffer, size_t len)
{
    if (len < RELAY_REQUEST_LEN)
        return false;
    snprintf(buffer, len, "rpc/Switch.Set?id=%d&on=%s&toggle_after=%d",
        getIndex(), on ? "true" : "false", getFailsafeSeconds());
    return true;
}

bool ShellyGen2Relay::applyResponse(JsonObjectConst data)
{
    char key[16] = {};
    snprintf(key, sizeof(key), "switch:%d", getIndex());
    JsonVariantConst mine = data[key];
    if (mine.isNull())
        return false; // Index passt nicht zur Antwort, keinen Zustand raten
    actualOn = mine["output"] | false;
    return true;
}
