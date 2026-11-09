#pragma once
#include <stddef.h>
#include <stdint.h>

// Stand-in for the Arduino WiFiClient: a test derives from it and feeds prepared data.
class WiFiClient
{
public:
    virtual ~WiFiClient() = default;
    virtual int connect(const char *, uint16_t, int32_t) { return 0; }
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int connected() { return 0; }
    virtual void stop() {}
    virtual size_t printf(const char *, ...) { return 0; }
};
