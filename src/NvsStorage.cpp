#include "NvsStorage.h"
#include <Preferences.h>
#include "nvs_flash.h"
#include "Logger.h"

namespace
{
    Preferences preferences;
    SemaphoreHandle_t nvsMutex = nullptr;
    bool begin(const char *pref_namespace, bool readOnly)
    {

        if (xSemaphoreTake(nvsMutex, portMAX_DELAY) != pdTRUE)
        {
            return false;
        }
        return preferences.begin(pref_namespace, readOnly);
    }
    void end()
    {
        preferences.end();
        xSemaphoreGive(nvsMutex);
    }

}

namespace NvsStorage
{
    Session::Session(const char *pref_namespace, bool readOnly)
    {
        if (!begin(pref_namespace, readOnly))
        {
            Logger::log(Logger::ErrorLevel::WARN, "could not open nvs!");
        }
    }
    Session::~Session()
    {
        end();
    }
    void init()
    {
        if (nvsMutex == nullptr)
        {
            nvsMutex = xSemaphoreCreateMutex();
        }
    }
    void erase()
    {
        nvs_flash_erase();
        nvs_flash_init();
    }
    bool writeBool(const char *key, const bool value)
    {
        return preferences.putBool(key, value) > 0; // returns bytes written
    }
    bool writeLong(const char *key, const long value)
    {
        return preferences.putLong(key, value) > 0;
    }
    bool writeString(const char *key, const char *value)
    {
        return preferences.putString(key, value) > 0;
    }
    bool writeFloat(const char *key, const float value)
    {
        return preferences.putFloat(key, value) > 0;
    }
    bool writeInt(const char *key, const int value)
    {
        return preferences.putInt(key, value) > 0;
    }

    bool readBool(const char *key, bool &value)
    {
        if (!preferences.isKey(key))
        {
            return false;
        }
        value = preferences.getBool(key);
        return true;
    }
    bool readLong(const char *key, long &value)
    {
        if (!preferences.isKey(key))
        {
            return false;
        }
        value = preferences.getLong(key);
        return true;
    }
    bool readString(const char *key, char *value, size_t len)
    {
        if (!preferences.isKey(key))
        {
            if (len > 0) // @note garantes valid empty string in every path
                value[0] = '\0';
            return false;
        }
        preferences.getString(key, value, len);
        return true;
    }
    bool readFloat(const char *key, float &value)
    {
        if (!preferences.isKey(key))
        {
            return false;
        }
        value = preferences.getFloat(key);
        return true;
    }
    bool readInt(const char *key, int &value)
    {
        if (!preferences.isKey(key))
        {
            return false;
        }
        value = preferences.getInt(key);
        return true;
    }

}