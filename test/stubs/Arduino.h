#pragma once
// Stand-in for the Arduino core in the native test build: only what src/ uses.
// millis() returns a value the test sets (FakeArduino::now) - time does not run by itself.
// Serial collects everything printed, so a test can check that a [WARN]/[ERROR] was logged.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string>
#include "freertos/FreeRTOS.h"

namespace FakeArduino
{
    inline unsigned long now = 0;
    inline std::string serialOutput;

    inline void reset()
    {
        now = 0;
        serialOutput.clear();
    }
}

inline unsigned long millis() { return FakeArduino::now; }

class FakeSerial
{
public:
    int printf(const char *format, ...)
    {
        char line[256] = {};
        va_list args;
        va_start(args, format);
        const int written = vsnprintf(line, sizeof(line), format, args);
        va_end(args);
        FakeArduino::serialOutput += line;
        return written;
    }
};

inline FakeSerial Serial;
