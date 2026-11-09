#include "ApDeviceRelayBase.h"
#include "NvsStorage.h"
#include "Logger.h"
#include <Arduino.h>

namespace
{
    // true only for the modes that may be stored - NOT_IN_LIST and any unknown value are not
    bool isStorableMode(RelayMode mode)
    {
        switch (mode)
        {
        case RelayMode::AUTO:
        case RelayMode::FORCED_ON:
        case RelayMode::FORCED_OFF:
            return true;
        case RelayMode::NOT_IN_LIST:
            return false;
        }
        return false; // a value outside the enum, e.g. cast from a number
    }
}
// public
ApDeviceRelayBase::ApDeviceRelayBase(const char *nvsNamespace, uint8_t idx, EntityType type) : EntityBase(nvsNamespace, idx, type) {}

bool ApDeviceRelayBase::load()
{
    if (!EntityBase::load())
        return false;
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_OUTPUT, key, sizeof(key));
    int value = 0;
    {
        NvsStorage::Session session(nvsNamespace(), true);
        if (!NvsStorage::readInt(key, value) || value < 0 || value > UINT8_MAX)
            return false;
    }
    _outputIdx = static_cast<uint8_t>(value);
    loadFailsafe();
    loadMode();
    return true;
}
bool ApDeviceRelayBase::write(JsonObjectConst data)
{
    if (!data["output"].is<uint8_t>())
        return false;
    const uint8_t outIdx = data["output"];

    if (!EntityBase::write(data))
        return false;
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_OUTPUT, key, sizeof(key));
    {
        NvsStorage::Session session(nvsNamespace(), false);
        NvsStorage::writeInt(key, outIdx);
        _outputIdx = outIdx;
    }
    return true;
}

bool ApDeviceRelayBase::getActualOn() const { return actualOn; }
bool ApDeviceRelayBase::getDesiredOn() const { return _desiredOn; }
void ApDeviceRelayBase::setDesiredOn(bool on) { _desiredOn = on; }

RelayMode ApDeviceRelayBase::getMode() const { return _mode; }
bool ApDeviceRelayBase::setMode(RelayMode mode)
{
    if (!isStorableMode(mode))
    {
        Logger::log(Logger::ErrorLevel::WARN, "relay mode not supported");
        return false;
    }
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_RELAY_MODE, key, sizeof(key));
    NvsStorage::Session session(EntityBase::nvsNamespace(), false);
    if(!NvsStorage::writeInt(key, static_cast<int>(mode)))
        return false;
    _mode = mode;
    return true;
}
uint8_t ApDeviceRelayBase::getOutputIdx() const { return _outputIdx; }
int ApDeviceRelayBase::getFailsafeSeconds() const { return _failsafeSeconds; }
bool ApDeviceRelayBase::setFailsafeSeconds(int seconds)
{
    if (seconds <MIN_FAILSAFE_SEC)
    { 
        return false;
    }
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_FAILSAFE_TIMEOUT, key, sizeof(key));
    NvsStorage::Session session(EntityBase::nvsNamespace(), false);
    if (!NvsStorage::writeInt(key, seconds))
        return false;
    _failsafeSeconds = seconds;
    return true;
}
// private
void ApDeviceRelayBase::loadFailsafe()
{
    _failsafeSeconds = DEFAULT_FAILSAFE_TIMEOUT_SEC;
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_FAILSAFE_TIMEOUT, key, sizeof(key));
    NvsStorage::Session session(EntityBase::nvsNamespace(), true);
    int seconds = 0;
    if (!NvsStorage::readInt(key, seconds))
        return; // not set by the user: default
    if (seconds < MIN_FAILSAFE_SEC)
    {
        // the setter never stores such a value - keep the default as a safeguard
        Logger::log(Logger::ErrorLevel::WARN, "stored failsafe below minimum, default used");
        return;
    }
    _failsafeSeconds = seconds;
}
void ApDeviceRelayBase::loadMode()
{
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_RELAY_MODE, key, sizeof(key));
    int savedMode = static_cast<int>(RelayMode::AUTO); // missing key: default AUTO
    {
        NvsStorage::Session session(EntityBase::nvsNamespace(), true);
        NvsStorage::readInt(key, savedMode);
    }
    const RelayMode mode = static_cast<RelayMode>(savedMode);
    if (isStorableMode(mode))
    {
        _mode = mode;
        return;
    }
    // stored by a firmware that knows more modes - never guess, let the user see it
    _mode = RelayMode::NOT_IN_LIST;
    Logger::log(Logger::ErrorLevel::WARN, "stored relay mode not supported by this firmware");
}
