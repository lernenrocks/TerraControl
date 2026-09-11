#include "WiFiRelayManager.h"
#include "NvsStorage.h"
#include "ShellyGen2Relay.h"
#include "Logger.h"

constexpr const char WIFIRELAYMANAGER_NVS_NAMESPACE[] = "wrManager";
constexpr const char CONFIG_KEY_RELAY_REGISTRY[] = "registry";
constexpr const size_t REGISTRY_MAX = 40; // 10 wifi devices a 4 relays

namespace
{
    struct RelaySlot
    {
        RelayBase *relay = nullptr;
        RelayType type = RelayType::NONE;

        RelaySlot() = default;
        RelaySlot(RelayBase *relay, RelayType type) : relay(relay), type(type) {}
    };

    RelaySlot relays[REGISTRY_MAX];

    RelayBase *find(const uint8_t *mac, uint8_t relayIdx)
    {
        for (const RelaySlot &slot : relays)
        {
            if (slot.relay && memcmp(mac, slot.relay->getMac(), MAC_LEN) == 0 && relayIdx == slot.relay->getRelayIdx())
            {
                return slot.relay;
            }
        }
        return nullptr;
    }

    RelayBase *find(const uint8_t id)
    {
        if (id >= REGISTRY_MAX)
        {
            return nullptr;
        }
        return relays[id].relay;
    }

    bool getNextID(uint8_t *id)
    {
        for (uint8_t i = 0; i < REGISTRY_MAX; i++)
        {
            if (relays[i].type == RelayType::NONE)
            {
                *id = i;
                return true;
            }
        }
        return false;
    }

    // builds the NVS blob fresh from the current relays[] state - never a
    // separately-maintained copy, so it can't drift from what's actually live
    bool persistRegistry()
    {
        RelayType blob[REGISTRY_MAX];
        for (uint8_t i = 0; i < REGISTRY_MAX; i++)
        {
            blob[i] = relays[i].type;
        }
        NvsStorage::Session session(WIFIRELAYMANAGER_NVS_NAMESPACE, false);
        return NvsStorage::writeBytes(CONFIG_KEY_RELAY_REGISTRY, blob, sizeof(blob));
    }

}
namespace WiFiRelayManager
{
    bool init()
    {
        bool success = true;
        bool registryChanged = false;
        RelayType registryBlob[REGISTRY_MAX] = {RelayType::NONE};

        {
            NvsStorage::Session session(WIFIRELAYMANAGER_NVS_NAMESPACE, true);
            if (NvsStorage::hasKey(CONFIG_KEY_RELAY_REGISTRY))
            {
                if (!NvsStorage::readBytes(CONFIG_KEY_RELAY_REGISTRY, registryBlob, sizeof(registryBlob)))
                    Logger::log(Logger::ErrorLevel::ERROR, "registry read failed despite existing key");
            }
            else
            {
                registryChanged = true;
            }
        }

        for (uint8_t i = 0; i < REGISTRY_MAX; i++)
        {
            switch (registryBlob[i])
            {
            case RelayType::SHELLY_GEN2:
            {
                ShellyGen2Relay *relay = new (std::nothrow) ShellyGen2Relay();
                if (!relay)
                {
                    success = false;
                    Logger::log(Logger::ErrorLevel::ERROR, "relay allocation failed");
                    break;
                }
                if (!relay->recover(i))
                {
                    success = false;
                    char msg[80] = {};
                    snprintf(msg, sizeof(msg), "relay id %d: recover() failed, registry/NVS mismatch - cleared", i);
                    Logger::log(Logger::ErrorLevel::ERROR, msg);
                    delete relay;
                    registryChanged = true;
                }
                else
                {
                    relays[i] = RelaySlot{relay, RelayType::SHELLY_GEN2};
                }
                break;
            }
            default:
                break;
            }
        }

        if (registryChanged)
        {
            persistRegistry();
        }

        return success;
    }

    bool add(const uint8_t *mac, uint8_t relayIdx, const char *password, const char *selfDescription, RelayType relayType)
    {
        uint8_t id;
        if (!getNextID(&id))
        {
            Logger::log(Logger::ErrorLevel::ERROR, "no free space for WifiRelay");
            return false;
        }
        ShellyGen2Relay *relay = new (std::nothrow) ShellyGen2Relay();
        if (!relay)
        {
            Logger::log(Logger::ErrorLevel::ERROR, "relay allocation failed");
            return false;
        }
        if (!relay->onboard(id, relayIdx, mac, password, selfDescription))
        {
            Logger::log(Logger::ErrorLevel::ERROR, "onboarding failed");
            delete relay;
            return false;
        }
        relays[id] = RelaySlot{relay, relayType};
        if (!persistRegistry())
        {
            Logger::log(Logger::ErrorLevel::ERROR, "registry write failed - relay works but won't survive reboot");
            return false;
        }
        return true;
    }

    bool remove(const uint8_t id)
    {
        RelayBase *relay = find(id);
        if (!relay)
        {
            return false;
        }
        if (!relay->remove())
        {
            Logger::log(Logger::ErrorLevel::WARN, "could not clear relay's own NVS namespace");
        }
        delete relay;
        relays[id] = RelaySlot{};
        if (!persistRegistry())
        {
            Logger::log(Logger::ErrorLevel::ERROR, "could not deregister relay");
            return false;
        }
        return true;
    }

    void markLost(const uint8_t id)
    {
        RelayBase *relay = find(id);
        if (relay)
        {
            relay->markLost();
        }
    }
    void markLost(const uint8_t *mac, uint8_t relayIdx)
    {
        RelayBase *relay = find(mac, relayIdx);
        if (relay)
        {
            relay->markLost();
        }
    }
    bool getSwitchRequest(bool on, char *buffer, size_t len, const uint8_t id)
    {
        RelayBase *relay = find(id);
        if (!relay)
        {
            return false;
        }
        return relay->buildSwitchRequest(on, buffer, len);
    }
    bool getStatusRequest(char *buffer, size_t len, const uint8_t id)
    {
        RelayBase *relay = find(id);
        if (!relay)
        {
            return false;
        }
        return relay->buildStatusRequest(buffer, len);
    }
    bool applyResponse(uint8_t id, JsonObjectConst data)
    {
        RelayBase *relay = find(id);
        if (!relay)
        {
            return false;
        }
        return relay->applyResponse(data);
    }
    bool getFailsafeSeconds(const uint8_t id, int &value)
    {
        RelayBase *relay = find(id);
        if (!relay)
        {
            return false;
        }
        value = relay->getFailsafeSeconds();
        return true;
    }
}
