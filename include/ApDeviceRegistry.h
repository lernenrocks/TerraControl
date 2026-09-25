#pragma once
#include "ApDevice.h"
#include <Arduino.h>
#include <ArduinoJson.h>


namespace ApDeviceRegistry
{
    void init();
    bool create(JsonObjectConst data);
    bool remove(uint8_t id);
    ApDevice *find(uint8_t id);
    ApDevice *find(const uint8_t *mac);
    uint8_t getFreeSlots();
}
