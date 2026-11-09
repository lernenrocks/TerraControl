#pragma once
// Stand-in for FreeRTOS in the native test build. The tests run in a single thread, so a mutex
// is just a flag. Taking a mutex that is already taken (or was never created) would block
// forever / crash on the ESP32 - here it is counted in FakeRtos::violations, and every test
// fixture checks that the counter stayed 0. That way a nested NvsStorage::Session or a missing
// init() still shows up as a failing test instead of passing silently.
#include <stdint.h>

typedef int BaseType_t;
typedef uint32_t TickType_t;

#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) (ms)

struct FakeMutex
{
    bool taken = false;
};
typedef FakeMutex *SemaphoreHandle_t;

namespace FakeRtos
{
    inline int violations = 0;
}

inline SemaphoreHandle_t xSemaphoreCreateMutex()
{
    return new FakeMutex(); // lives until the test program ends, like the real one
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t)
{
    if (!mutex || mutex->taken)
    {
        FakeRtos::violations++;
        return mutex ? pdTRUE : pdFALSE;
    }
    mutex->taken = true;
    return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    if (mutex)
        mutex->taken = false;
    return pdTRUE;
}

inline void vTaskDelay(TickType_t) {}
inline void taskYIELD() {}
