#pragma once
#include <Arduino.h>


namespace NvsStorage
{
    constexpr size_t NVS_KEY_LEN = 16; // max size of key or namespace name

    void init();
    void erase();

    bool clearNamespace();
    bool hasKey(const char *key);

    bool removeKey(const char *key);

    bool writeBool(const char *key, const bool value);
    bool writeLong(const char *key, const long value);
    bool writeString(const char *key, const char *value);
    bool writeFloat(const char *key, const float value);
    bool writeInt(const char *key, const int value);
    bool writeBytes(const char *key, const void *value, size_t len);

    bool readBool(const char *key, bool &value);
    bool readLong(const char *key, long &value);
    bool readString(const char *key, char *value, size_t len);
    bool readFloat(const char *key, float &value);
    bool readInt(const char *key, int &value);
    bool readBytes(const char *key, void *value, size_t len);

    size_t getFreeEntries();

    class Session
    {
    public:
        Session(const char *pref_namespace, bool readOnly);
        ~Session();
        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;
    };

}