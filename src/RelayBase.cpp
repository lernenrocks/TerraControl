#include "RelayBase.h"
#include "Logger.h"
#include "NvsStorage.h"
#include <Arduino.h>
// public
RelayBase::RelayBase(const char *nvsNamespace,uint8_t idx):EntityBase(nvsNamespace,idx)
{
    loadFailsafe();
}
RelayBase::RelayBase(const char *nvsNamespace,uint8_t idx,const char *name):EntityBase(nvsNamespace,idx,name)
{
    loadFailsafe();
}


bool RelayBase::getActualOn() const { return actualOn; }
    bool RelayBase::getDesiredOn() const {return _desiredOn;}
    void RelayBase::setDesiredOn(bool on){_desiredOn=on;}

    RelayMode RelayBase::getMode() const{return _mode;}
    void RelayBase::setMode(RelayMode mode){_mode=mode;}
int RelayBase::getFailsafeSeconds() const { return _failsafeSeconds; }
bool RelayBase::setFailsafeSeconds(int seconds)
{
    NvsStorage::Session session(EntityBase::nvsNamespace(), false);
    if (!NvsStorage::writeInt(CONFIG_KEY_FAILSAFE_TIMEOUT, seconds))
        return false;
    _failsafeSeconds = seconds;
    return true;
}
//private
void RelayBase::loadFailsafe(){
    NvsStorage::Session session(EntityBase::nvsNamespace(), true);
    if(NvsStorage::hasKey(CONFIG_KEY_FAILSAFE_TIMEOUT)){
        int seconds = 0;
        NvsStorage::readInt(CONFIG_KEY_FAILSAFE_TIMEOUT,seconds);
        _failsafeSeconds=seconds;
    }
    else _failsafeSeconds=DEFAULT_FAILSAFE_TIMEOUT_SEC;
}

