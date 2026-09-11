#include "RelayBase.h"
#include "ShellyGen2Relay.h"
#include <Arduino.h>
#include <ArduinoJson.h>

enum class RelayType : uint8_t
{
    NONE = 0,
    SHELLY_GEN2 = 1
};

namespace WiFiRelayManager
{

    bool init();
    bool add(const uint8_t *mac, uint8_t relayIdx, const char *password, const char *type, RelayType relayType);
    bool remove(const uint8_t id);
    void markLost(const uint8_t id);
    void markLost(const uint8_t *mac, uint8_t relayIdx);
    bool getSwitchRequest(bool on, char *buffer, size_t len, const uint8_t id);
    bool getStatusRequest(char *buffer, size_t len, const uint8_t id);
    bool applyResponse(uint8_t id, JsonObjectConst data);
    bool getFailsafeSeconds(const uint8_t id, int &value);
}