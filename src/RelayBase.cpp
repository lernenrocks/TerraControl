#include "RelayBase.h"
#include "Logger.h"
#include "NvsStorage.h"
#include <Arduino.h>
// public
RelayBase::RelayBase(uint8_t id, const char *type, uint8_t relayIdx) : _id(id), _type(type), _relayIdx(relayIdx)
{
    ensureDefaults();
}

bool RelayBase::remove()
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::clearNamespace();
}

bool RelayBase::getName(char *buffer, size_t len) const
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readString(CONFIG_KEY_RELAY_NAME, buffer, len);
}
bool RelayBase::setName(const char *name, size_t len)
{
    if (len < RELAY_NAME_LEN)
    {
        char ns[NvsStorage::NVS_KEY_LEN] = {};
        nvsNamespace(ns, sizeof(ns));
        NvsStorage::Session session(ns, false);
        return NvsStorage::writeString(CONFIG_KEY_RELAY_NAME, name);
    }
    return false;
}
bool RelayBase::getCurrentState(bool &isOn) const
{
    isOn = currentState;
    return isFound();
}
bool RelayBase::getFailsafeSeconds(int &buffer) const
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);
    return NvsStorage::readInt(CONFIG_KEY_FAILSAFE_TIMEOUT, buffer);
}
bool RelayBase::setFailsafeSeconds(int seconds)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::writeInt(CONFIG_KEY_FAILSAFE_TIMEOUT, seconds);
}
uint8_t RelayBase::getRelayIdx() const { return _relayIdx; }
uint8_t RelayBase::getId() const { return _id; }
bool RelayBase::isFound() const {return found; }

// protected
void RelayBase::nvsNamespace(char *buffer, size_t len) const
{
    snprintf(buffer, len, "wifiRelay_%d", _id);
}

// private
void RelayBase::ensureDefaults()
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);

    int dummyFailsafe;
    if (!NvsStorage::readInt(CONFIG_KEY_FAILSAFE_TIMEOUT, dummyFailsafe))
    {
        NvsStorage::writeInt(CONFIG_KEY_FAILSAFE_TIMEOUT, DEFAULT_FAILSAFE_TIMEOUT_SEC);
    }

    char dummyName[RELAY_NAME_LEN];
    if (!NvsStorage::readString(CONFIG_KEY_RELAY_NAME, dummyName, sizeof(dummyName)))
    {
        char defaultName[RELAY_NAME_LEN] = {};
        snprintf(defaultName, sizeof(defaultName), "WifiRelay %d", _id);
        NvsStorage::writeString(CONFIG_KEY_RELAY_NAME, defaultName);
    }
}