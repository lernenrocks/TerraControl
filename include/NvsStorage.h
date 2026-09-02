#pragma once
#include <Arduino.h>

namespace
{
    bool begin(const char *pref_namespace, bool readOnly);
    void end();

}

namespace NvsStorage
{
    constexpr size_t NVS_KEY_LEN = 16; // max size of key or namespace name

    void init();
    void erase();

    bool writeBool(const char *key, const bool value);
    bool writeLong(const char *key, const long value);
    bool writeString(const char *key, const char *value);
    bool writeFloat(const char *key, const float value);
    bool writeInt(const char *key, const int value);

    bool readBool(const char *key, bool &value);
    bool readLong(const char *key, long &value);
    bool readString(const char *key, char *value, size_t len);
    bool readFloat(const char *key, float &value);
    bool readInt(const char *key, int &value);

    class Session
    {
    public:
        Session(const char *pref_namespace, bool readOnly);
        ~Session();
        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;
    };

}