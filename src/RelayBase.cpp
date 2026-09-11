#include "RelayBase.h"
#include "Logger.h"
#include "NvsStorage.h"
#include <Arduino.h>
// public
RelayBase::RelayBase() {}

bool RelayBase::onboard(uint8_t id, uint8_t relayIdx, const uint8_t *mac, const char *password, const char *type)
{
    _id = id;
    _relayIdx = relayIdx;
    memcpy(_mac, mac, sizeof(_mac));
    writeDefaults();
    if (!writeIdentity())
        return false;
    if (!writePassword(password))
        return false;
    if (!writeType(type))
        return false;
    return true;
}

bool RelayBase::recover(uint8_t id)
{
    _id = id;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, true);

    if (!NvsStorage::readBytes(CONFIG_KEY_RELAY_MAC, _mac, sizeof(_mac)))
        return false;

    int relayIdxTmp;
    if (!NvsStorage::readInt(CONFIG_KEY_RELAY_IDX, relayIdxTmp))
        return false;
    _relayIdx = (uint8_t)relayIdxTmp;

    if (!NvsStorage::readInt(CONFIG_KEY_FAILSAFE_TIMEOUT, _failsafeSeconds))
        _failsafeSeconds = DEFAULT_FAILSAFE_TIMEOUT_SEC;

    if (!NvsStorage::readString(CONFIG_KEY_RELAY_NAME, _name, sizeof(_name)))
        snprintf(_name, sizeof(_name), "WifiRelay %d", _id);

    NvsStorage::readString(CONFIG_KEY_RELAY_TYPE, _type, sizeof(_type));
    NvsStorage::readString(CONFIG_KEY_RELAY_PASSWORD, _password, sizeof(_password));

    return true;
}

bool RelayBase::remove()
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    return NvsStorage::clearNamespace();
}

const char *RelayBase::getName() const
{
    return _name;
}
bool RelayBase::setName(const char *name)
{
    if (strlen(name) >= RELAY_NAME_LEN)
        return false;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeString(CONFIG_KEY_RELAY_NAME, name))
        return false;
    strlcpy(_name, name, sizeof(_name));
    return true;
}
const char *RelayBase::getType() const
{
    return _type;
}
const char *RelayBase::getPassword() const
{
    return _password;
}
bool RelayBase::getOutputOn(bool &isOn) const
{
    isOn = outputOn;
    return isFound();
}
int RelayBase::getFailsafeSeconds() const { return _failsafeSeconds; }
bool RelayBase::setFailsafeSeconds(int seconds)
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeInt(CONFIG_KEY_FAILSAFE_TIMEOUT, seconds))
        return false;
    _failsafeSeconds = seconds;
    return true;
}
uint8_t RelayBase::getRelayIdx() const { return _relayIdx; }
uint8_t RelayBase::getId() const { return _id; }
const uint8_t *RelayBase::getMac() const { return _mac; }
bool RelayBase::isFound() const { return found; }
void RelayBase::markLost() { found = false; }

// protected
void RelayBase::nvsNamespace(char *buffer, size_t len) const
{
    snprintf(buffer, len, "wifiRelay_%d", _id);
}

// private
bool RelayBase::writeType(const char *type)
{
    if (strlen(type) >= RELAY_TYPE_LEN)
        return false;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeString(CONFIG_KEY_RELAY_TYPE, type))
        return false;
    strlcpy(_type, type, sizeof(_type));
    return true;
}

bool RelayBase::writePassword(const char *password)
{
    if (strlen(password) >= RELAY_PASSWORD_LEN)
        return false;
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeString(CONFIG_KEY_RELAY_PASSWORD, password))
        return false;
    strlcpy(_password, password, sizeof(_password));
    return true;
}

void RelayBase::writeDefaults()
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);

    if (!NvsStorage::readInt(CONFIG_KEY_FAILSAFE_TIMEOUT, _failsafeSeconds))
    {
        _failsafeSeconds = DEFAULT_FAILSAFE_TIMEOUT_SEC;
        NvsStorage::writeInt(CONFIG_KEY_FAILSAFE_TIMEOUT, _failsafeSeconds);
    }

    if (!NvsStorage::readString(CONFIG_KEY_RELAY_NAME, _name, sizeof(_name)))
    {
        snprintf(_name, sizeof(_name), "WifiRelay %d", _id);
        NvsStorage::writeString(CONFIG_KEY_RELAY_NAME, _name);
    }
}

bool RelayBase::writeIdentity()
{
    char ns[NvsStorage::NVS_KEY_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    NvsStorage::Session session(ns, false);
    if (!NvsStorage::writeBytes(CONFIG_KEY_RELAY_MAC, _mac, sizeof(_mac)))
        return false;
    if (!NvsStorage::writeInt(CONFIG_KEY_RELAY_IDX, _relayIdx))
        return false;
    return true;
}
