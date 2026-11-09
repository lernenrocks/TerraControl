#pragma once
// Stand-in for the Arduino Preferences library on top of FakeNvs. Return values follow the
// real library (framework-arduinoespressif32, libraries/Preferences/src/Preferences.cpp),
// including its quirks - e.g. putString() returns strlen(value), so "" returns 0 just like
// a failure. Only the functions NvsStorage.cpp uses.
#include "FakeNvs.h"
#include <math.h>

class Preferences
{
public:
    bool begin(const char *name, bool readOnly = false, const char * = nullptr)
    {
        if (_started || !name || strlen(name) > FakeNvs::MAX_NAME_LEN)
            return false;
        if (readOnly && !FakeNvs::hasNamespace(name))
            return false; // nvs_open() read-only on a missing namespace fails
        if (!readOnly)
            FakeNvs::store[name]; // read-write creates the namespace
        _ns = name;
        _readOnly = readOnly;
        _started = true;
        return true;
    }

    void end() { _started = false; }

    bool clear()
    {
        if (!canWrite())
            return false;
        FakeNvs::store[_ns].clear();
        return true;
    }

    bool remove(const char *key)
    {
        if (!canWrite() || !key || !ns().count(key))
            return false;
        ns().erase(key);
        return true;
    }

    bool isKey(const char *key) { return _started && key && ns().count(key) > 0; }

    size_t putBool(const char *key, bool value) { return putNumber(key, FakeNvs::Type::U8, value ? 1 : 0) ? 1 : 0; }
    size_t putInt(const char *key, int32_t value) { return putNumber(key, FakeNvs::Type::I32, value) ? 4 : 0; }
    size_t putLong(const char *key, int32_t value) { return putNumber(key, FakeNvs::Type::I32, value) ? 4 : 0; }
    size_t putFloat(const char *key, float value) { return putBytes(key, &value, sizeof(value)); }

    size_t putString(const char *key, const char *value)
    {
        if (!value || !canWrite(key))
            return 0;
        FakeNvs::Entry entry{FakeNvs::Type::STR};
        entry.text = value;
        ns()[key] = entry;
        return strlen(value); // the real library does exactly this
    }

    size_t putBytes(const char *key, const void *value, size_t len)
    {
        if (!value || !len || !canWrite(key))
            return 0;
        FakeNvs::Entry entry{FakeNvs::Type::BLOB};
        const uint8_t *bytes = static_cast<const uint8_t *>(value);
        entry.blob.assign(bytes, bytes + len);
        ns()[key] = entry;
        return len;
    }

    bool getBool(const char *key, bool defaultValue = false)
    {
        const FakeNvs::Entry *entry = find(key, FakeNvs::Type::U8);
        return entry ? entry->number != 0 : defaultValue;
    }

    int32_t getInt(const char *key, int32_t defaultValue = 0)
    {
        const FakeNvs::Entry *entry = find(key, FakeNvs::Type::I32);
        return entry ? entry->number : defaultValue;
    }

    int32_t getLong(const char *key, int32_t defaultValue = 0) { return getInt(key, defaultValue); }

    float getFloat(const char *key, float defaultValue = NAN)
    {
        float value = defaultValue;
        getBytes(key, &value, sizeof(value));
        return value;
    }

    // returns the length incl. terminator, 0 if missing, wrong type or too long for maxLen
    size_t getString(const char *key, char *value, size_t maxLen)
    {
        const FakeNvs::Entry *entry = find(key, FakeNvs::Type::STR);
        if (!entry || !value || !maxLen)
            return 0;
        const size_t len = entry->text.size() + 1;
        if (len > maxLen)
            return 0; // nothing written, like the real library
        memcpy(value, entry->text.c_str(), len);
        return len;
    }

    size_t getBytesLength(const char *key)
    {
        const FakeNvs::Entry *entry = find(key, FakeNvs::Type::BLOB);
        return entry ? entry->blob.size() : 0;
    }

    size_t getBytes(const char *key, void *buf, size_t maxLen)
    {
        const FakeNvs::Entry *entry = find(key, FakeNvs::Type::BLOB);
        if (!entry || !buf || entry->blob.size() > maxLen)
            return 0;
        memcpy(buf, entry->blob.data(), entry->blob.size());
        return entry->blob.size();
    }

private:
    bool _started = false;
    bool _readOnly = false;
    std::string _ns;

    FakeNvs::Namespace &ns() { return FakeNvs::store[_ns]; }

    bool canWrite(const char *key = "") const
    {
        return _started && !_readOnly && !FakeNvs::failWrites && key && strlen(key) <= FakeNvs::MAX_NAME_LEN;
    }

    bool putNumber(const char *key, FakeNvs::Type type, int32_t value)
    {
        if (!canWrite(key))
            return false;
        FakeNvs::Entry entry{type};
        entry.number = value;
        ns()[key] = entry;
        return true;
    }

    const FakeNvs::Entry *find(const char *key, FakeNvs::Type type)
    {
        if (!_started || !key)
            return nullptr;
        auto it = ns().find(key);
        if (it == ns().end() || it->second.type != type)
            return nullptr;
        return &it->second;
    }
};
