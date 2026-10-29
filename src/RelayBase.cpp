#include "RelayBase.h"
#include "NvsStorage.h"
#include <Arduino.h>
// public
RelayBase::RelayBase(const char *nvsNamespace, uint8_t idx, EntityType type) : EntityBase(nvsNamespace, idx, type) {}

bool RelayBase::load()
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
bool RelayBase::write(JsonObjectConst data)
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

bool RelayBase::getActualOn() const { return actualOn; }
bool RelayBase::getDesiredOn() const { return _desiredOn; }
void RelayBase::setDesiredOn(bool on) { _desiredOn = on; }

RelayMode RelayBase::getMode() const { return _mode; }
bool RelayBase::setMode(RelayMode mode)
{
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_RELAY_MODE, key, sizeof(key));
    NvsStorage::Session session(EntityBase::nvsNamespace(), false);
    if(!NvsStorage::writeInt(key, static_cast<int>(mode)))
        return false;
    _mode = mode;
    return true;
}
uint8_t RelayBase::getOutputIdx() const { return _outputIdx; }
int RelayBase::getFailsafeSeconds() const { return _failsafeSeconds; }
bool RelayBase::setFailsafeSeconds(int seconds)
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
void RelayBase::loadFailsafe()
{
    _failsafeSeconds = DEFAULT_FAILSAFE_TIMEOUT_SEC;
    char key[NvsStorage::NVS_KEY_LEN] = {};
    nvsKey(CONFIG_KEY_FAILSAFE_TIMEOUT, key, sizeof(key));
    NvsStorage::Session session(EntityBase::nvsNamespace(), true);
    int seconds = 0;
    if (NvsStorage::readInt(key, seconds))
    {
        _failsafeSeconds = seconds;
    }
}
void RelayBase::loadMode()
{
    int savedMode=0;
    char key[NvsStorage::NVS_KEY_LEN]={};
    nvsKey(CONFIG_KEY_RELAY_MODE,key,sizeof(key));
    {
    NvsStorage::Session session(EntityBase::nvsNamespace(), true);
    NvsStorage::readInt(key,savedMode);
    }
    RelayMode desiredMode = static_cast<RelayMode>(savedMode);
    bool found =false;
    switch (desiredMode)
    {
        case RelayMode::AUTO:
        case RelayMode::FORCED_OFF:
        case RelayMode::FORCED_ON:
        found=true;
        break;
    }
    if(found){
        _mode=desiredMode;
    }

}
