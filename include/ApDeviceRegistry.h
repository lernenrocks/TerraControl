#pragma once
#include "ApDevice.h"
#include <Arduino.h>
#include <ArduinoJson.h>

// Access rule (NetworkWorker, Controller and ApiWorker share the devices):
// - Every call below except init() requires a held Lock - the functions do not lock themselves,
//   same as NvsStorage's read/write functions require an open Session.
// - find() takes the Lock as a parameter: it is the only way to get an ApDevice* (and through
//   getEntity() an EntityBase*), so the compiler refuses any access without a Lock.
//   The parameter is never used inside, it only proves that the caller holds one.
// - An ApDevice* or EntityBase* from the registry is only valid while that Lock is held.
//   Never keep one across a wait (TCP request, vTaskDelay): copy what you need, release the Lock,
//   and find() the device again afterwards - it may have been removed or replaced meanwhile.
// - Lock order: Lock first, NvsStorage::Session second, never the other way round (deadlock).
namespace ApDeviceRegistry
{
    class Lock
    {
    public:
        Lock();
        ~Lock();
        Lock(const Lock &) = delete;
        Lock &operator=(const Lock &) = delete;
    };

    void init(); // boot only, before any task uses the registry - creates the mutex, needs no Lock
    bool create(JsonObjectConst data);
    bool remove(uint8_t id);
    ApDevice *find(const Lock &, uint8_t id);
    ApDevice *find(const Lock &, const uint8_t *mac);
    bool getMac(uint8_t id, uint8_t *buffer, size_t len);
    uint8_t getFreeSlots();
}
