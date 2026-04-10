#pragma once
#include <stddef.h>
class WiFiClient {
public:
    virtual int available() { return 0; }
    virtual int read()      { return -1; }
    virtual int connected() { return 0; }
    virtual void stop()     {}
};