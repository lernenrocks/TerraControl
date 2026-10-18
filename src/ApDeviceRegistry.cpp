#include "ApDeviceRegistry.h"
#include "ApDevice.h"
#include "Logger.h"
#include "NetworkUtils.h"

namespace
{
    ApDevice *devices[DEVICE_MAX] = {};
    SemaphoreHandle_t registryMutex = nullptr;

    bool isMacUsed(const char *macText, uint8_t *idx)
    {
        uint8_t macToCheck[MAC_LEN] = {};
        if (!NetworkUtils::macCharToBytes(macToCheck, macText))
            return false; // invalid MAC text, the ApDevice constructor rejects it anyway
        uint8_t deviceMac[MAC_LEN] = {};
        for (uint8_t i = 0; i < DEVICE_MAX; i++)
        {
            if (devices[i] == nullptr)
            {
                continue;
            }
            if (!devices[i]->getMac(deviceMac, MAC_LEN))
                continue; // buffer may still hold the previous device's MAC
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
    Lock::Lock()
    {
        xSemaphoreTake(registryMutex, portMAX_DELAY);
    }
    Lock::~Lock()
    {
        xSemaphoreGive(registryMutex);
    }

    void init()
    {
        if (registryMutex == nullptr)
        {
            registryMutex = xSemaphoreCreateMutex();
        }
        for (uint8_t i = 0; i < DEVICE_MAX; i++)
        {
            devices[i] = new (std::nothrow) ApDevice(i);
            if (!devices[i])
            {
                char msg[64] = {};
                snprintf(msg, sizeof(msg), "WiFi Device %d allocation failed", i);
                Logger::log(Logger::ErrorLevel::ERROR, msg);
                continue; // NVS stays untouched, the next boot tries again
            }
            uint8_t mac[MAC_LEN] = {};
            if (!devices[i]->getMac(mac, sizeof(mac)))
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
                devices[i] = new (std::nothrow) ApDevice(i, data);
                if (!devices[i])
                {
                    char msg[64] = {};
                    snprintf(msg, sizeof(msg), "WiFi Device %d allocation failed", i);
                    Logger::log(Logger::ErrorLevel::ERROR, msg);
                    return false;
                }
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
    ApDevice *find(const Lock &, uint8_t id){
        if(id <DEVICE_MAX){
        return devices[id];
        }
        else return nullptr;
    }
    ApDevice *find(const Lock &, const uint8_t *mac){
        uint8_t deviceMac[MAC_LEN] = {};
        for (uint8_t i =0;i<DEVICE_MAX;i++){
            if(!devices[i]) continue;
            if(!devices[i]->getMac(deviceMac,sizeof(deviceMac))) continue; // buffer may still hold the previous device's MAC
            if(memcmp(deviceMac,mac,sizeof(deviceMac))==0) 
                return devices[i];
        }
        return nullptr; 
    }
    bool getMac(uint8_t id, uint8_t *buffer, size_t len)
    {
        // no find() here: the caller already holds the Lock, and find() cannot be called without one
        ApDevice *device = id < DEVICE_MAX ? devices[id] : nullptr;
        if (!device)
            return false;
        return device->getMac(buffer, len);
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