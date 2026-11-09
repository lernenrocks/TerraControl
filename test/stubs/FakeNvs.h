#pragma once
// In-memory NVS for the native test build, used by the Preferences and nvs_flash stand-ins.
// The real NvsStorage.cpp runs on top of it, so the tests cover NvsStorage too.
//
// Behaviour copied from the Arduino Preferences library + ESP-IDF NVS (not idealized):
// - a namespace exists once it was opened read-write; opening a missing one read-only fails
// - keys and namespaces longer than 15 characters are refused
// - every key has a type; reading it with another type returns the default value
// - clear() removes all keys, the namespace itself stays
//
// Test helpers below let a test prepare NVS content directly (e.g. a value an older firmware
// wrote) and make writes fail on command (failWrites).
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <string.h>

namespace FakeNvs
{
    enum class Type
    {
        U8,
        I32,
        STR,
        BLOB
    };

    struct Entry
    {
        Type type;
        int32_t number = 0;
        std::string text;
        std::vector<uint8_t> blob;
    };

    using Namespace = std::map<std::string, Entry>;

    inline std::map<std::string, Namespace> store;
    inline bool failWrites = false;  // every put/remove/clear fails, nothing changes
    inline size_t rawFreeEntries = 1000 + 126; // as nvs_get_stats() reports it (incl. reserved page)

    constexpr size_t MAX_NAME_LEN = 15;

    inline void reset()
    {
        store.clear();
        failWrites = false;
        rawFreeEntries = 1000 + 126;
    }

    // what NvsStorage::getFreeEntries() returns (it subtracts the 126 reserved entries)
    inline void setUsableFreeEntries(size_t entries) { rawFreeEntries = entries + 126; }

    inline bool hasNamespace(const std::string &ns) { return store.count(ns) > 0; }

    inline bool hasKey(const std::string &ns, const std::string &key)
    {
        auto it = store.find(ns);
        return it != store.end() && it->second.count(key) > 0;
    }

    inline size_t keyCount(const std::string &ns)
    {
        auto it = store.find(ns);
        return it == store.end() ? 0 : it->second.size();
    }

    inline void setInt(const std::string &ns, const std::string &key, int32_t value)
    {
        Entry entry{Type::I32};
        entry.number = value;
        store[ns][key] = entry;
    }

    inline void setString(const std::string &ns, const std::string &key, const std::string &value)
    {
        Entry entry{Type::STR};
        entry.text = value;
        store[ns][key] = entry;
    }

    inline void eraseKey(const std::string &ns, const std::string &key)
    {
        auto it = store.find(ns);
        if (it != store.end())
            it->second.erase(key);
    }

    inline int32_t getInt(const std::string &ns, const std::string &key)
    {
        return store[ns][key].number;
    }

    inline std::string getString(const std::string &ns, const std::string &key)
    {
        return store[ns][key].text;
    }
}
