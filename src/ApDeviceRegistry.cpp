#include "ApDeviceRegistry.h"
#include "ApDevice.h"
#include "Logger.h"
#include "NetworkUtils.h"

namespace
{
    ApDevice *devices[DEVICE_MAX] = {};

    bool isMacUsed(const char *macText, uint8_t *idx)
    {
        uint8_t macToCheck[MAC_LEN];
        NetworkUtils::macCharToBytes(macToCheck, macText);
        uint8_t deviceMac[MAC_LEN];
        for (uint8_t i = 0; i < DEVICE_MAX; i++)
        {
            if (devices[i] == nullptr)
            {
                continue;
            }
            devices[i]->getMac(deviceMac, MAC_LEN);
            if (memcmp(macToCheck, deviceMac, MAC_LEN) == 0)
            {
                *idx = i;
                return true;
            }
        }
        return false;
    }

}
namespace ApDeviceRegistry
{
    void init()
    {
        for (uint8_t i = 0; i < DEVICE_MAX; i++)
        {
            devices[i] = new ApDevice(i);
            uint8_t mac[MAC_LEN] = {};
            if (!devices[i]->getMac(mac, sizeof(mac)) || devices[i]->getEntityCount() == 0)
            {
                delete devices[i];
                devices[i] = nullptr;
                continue;
            }
        }
    }
    bool create(JsonObjectConst data)
    {
        if (!data.containsKey("mac"))
        {
            return false;
        }
        const char *deviceMac = data["mac"] | "";

        uint8_t idx;
        if (isMacUsed(deviceMac, &idx)) // Duplicate check
        {
            remove(idx);
            char msg[64] = {};
            snprintf(msg, sizeof(msg), "removed duplicate Mac %s from device list", deviceMac);
            Logger::log(Logger::ErrorLevel::WARN, msg);
        }
        if (getFreeSlots() == 0) // spacce left check
        {
            Logger::log(Logger::ErrorLevel::ERROR, "no space left for more Devices");
            return false;
        }

        for (uint8_t i = 0; i < DEVICE_MAX; i++)
        {
            if (devices[i] == nullptr)
            {
                devices[i] = new ApDevice(i, data);
                uint8_t mac[MAC_LEN] = {};
                if (!devices[i]->getMac(mac, sizeof(mac)) || devices[i]->getEntityCount() == 0)
                {
                   remove(i);
                    char msg[64] = {};
                    const char *deviceType = data["type"] | "";
                    snprintf(msg, sizeof(msg), "could not bind device mac %s,type %s.", deviceMac, deviceType);
                    Logger::log(Logger::ErrorLevel::ERROR, msg);
                    return false;
                }
                else return true;
            }
        }
        return false;
    }

    bool remove(uint8_t id){
        if(id>=DEVICE_MAX)
            return false;
        delete devices[id];
        devices[id]=nullptr;
        return true;
    }
    ApDevice *find(uint8_t id){
        if(id <DEVICE_MAX){
        return devices[id];
        }
        else return nullptr;
    }
    ApDevice *find(const uint8_t *mac){
        uint8_t deviceMac[MAC_LEN] = {};
        for (uint8_t i =0;i<DEVICE_MAX;i++){
            if(!devices[i]) continue;
            devices[i]->getMac(deviceMac,sizeof(deviceMac));
            if(memcmp(deviceMac,mac,sizeof(deviceMac))==0) 
                return devices[i];
        }
        return nullptr; 
    }
    uint8_t getFreeSlots()
    {
        uint8_t freeSlots=0;
        for (uint8_t i=0;i<DEVICE_MAX;i++){
            if(!devices[i]) { freeSlots++;}
        }
        return freeSlots;
    }
}