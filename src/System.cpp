#include "System.h"
#include <Arduino.h>
#include "NvsStorage.h"
#include "Logger.h"
#include "InitalCredentials.h"

constexpr const uint8_t VERSION_LEN = 16;
constexpr const char SYSTEM_NVS_NAMESPACE[] = "system";
constexpr const char CONFIG_KEY_SYSTEM_DEVICE_NAME[] = "name";
constexpr const char CONFIG_KEY_SYSTEM_DEVICE_PASSWORD[] = "password";
constexpr const char CONFIG_KEY_SYSTEM_H1[] = "h1";
constexpr const char CONFIG_KEY_SYSTEM_STA_MODE[] = "sta";
constexpr const char CONFIG_KEY_SYSTEM_STA_SSID[] = "staSsid";
constexpr const char CONFIG_KEY_SYSTEM_STA_PASSWORD[] = "staPw";

namespace
{
    char _version[VERSION_LEN] = {};
    bool isSTA = false;
}

namespace System
{
    bool init(const char *version)
    {
        if (strlen(version) >= VERSION_LEN)
            return false;
        else
        {
            strlcpy(_version, version, sizeof(_version));
        }
        bool hasName;
        bool hasPassword;
        {
            NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, true);
            hasName = NvsStorage::hasKey(CONFIG_KEY_SYSTEM_DEVICE_NAME);
            hasPassword = NvsStorage::hasKey(CONFIG_KEY_SYSTEM_DEVICE_PASSWORD);
            if(NvsStorage::hasKey(CONFIG_KEY_SYSTEM_STA_MODE)){
                NvsStorage::readBool(CONFIG_KEY_SYSTEM_STA_MODE,isSTA);
            }
        }
        if (!hasName)
        {
            if (!setDeviceName(DEFAULT_DEVICE_NAME))
                return false;
        }
        if (!hasPassword)
        {
            if (!setDevicePassword(DEFAULT_DEVICE_PASSWORD))
                return false;
        }
        return true;
    }
    bool getDeviceName(char *buffer, uint8_t len)
    {
        if (len < SYSTEM_DEVICE_NAME_LEN)
        {
            return false;
        }
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, true);
        return NvsStorage::readString(CONFIG_KEY_SYSTEM_DEVICE_NAME, buffer, len);
    }
    bool setDeviceName(const char *name)
    {
        if (strlen(name) >= SYSTEM_DEVICE_NAME_LEN)
            return false;
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, false);
        if (!NvsStorage::writeString(CONFIG_KEY_SYSTEM_DEVICE_NAME, name))
        {
            return false;
        }
        return true;
    }
    bool getDevicePassword(char *buffer, uint8_t len)
    {
        if (len < SYSTEM_DEVICE_PASSWORD_LEN)
        {
            return false;
        }
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, true);
        return NvsStorage::readString(CONFIG_KEY_SYSTEM_DEVICE_PASSWORD, buffer, len);
    }
    bool setDevicePassword(const char *password)
    {
        if (strlen(password) >= SYSTEM_DEVICE_PASSWORD_LEN || strlen(password) < SYSTEM_WIFI_PASSWORD_MIN_LEN)
            return false;
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, false);
        if (!NvsStorage::writeString(CONFIG_KEY_SYSTEM_DEVICE_PASSWORD, password))
        {
            return false;
        }
        return true;
    }

    bool getDevicePasswordH1(char *buffer, uint8_t len)
    {
        if (len < SYSTEM_DEVICE_H1_LEN + 1) //+1 for terminal symbol. Buffer needs 64 chars + terminal
            return false;
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, true);
        return NvsStorage::readString(CONFIG_KEY_SYSTEM_H1, buffer, len);
    }
    bool setDevicePasswordH1(const char *h1)
    {
        if (strlen(h1) != SYSTEM_DEVICE_H1_LEN)
            return false;
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, false);
        if (!NvsStorage::writeString(CONFIG_KEY_SYSTEM_H1, h1))
        {
            return false;
        }
        return true;
    }
    bool getStaSsid(char *buffer, uint8_t len)
    {
        if (len < SYSTEM_STA_SSID_LEN)
        {
            return false;
        }
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, true);
        return NvsStorage::readString(CONFIG_KEY_SYSTEM_STA_SSID, buffer, len);
    }
    bool setStaSsid(const char *ssid)
    {
        if (strlen(ssid) >= SYSTEM_STA_SSID_LEN)
            return false;
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, false);
        if (!NvsStorage::writeString(CONFIG_KEY_SYSTEM_STA_SSID, ssid))
        {
            return false;
        }
        return true;
    }
    bool getStaPassword(char *buffer, uint8_t len)
    {
        if (len < SYSTEM_STA_PASSWORD_LEN)
        {
            return false;
        }
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, true);
        return NvsStorage::readString(CONFIG_KEY_SYSTEM_STA_PASSWORD, buffer, len);
    }
    bool setStaPassword(const char *password)
    {
        if (strlen(password) >= SYSTEM_STA_PASSWORD_LEN || strlen(password) < SYSTEM_WIFI_PASSWORD_MIN_LEN)
            return false;
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, false);
        if (!NvsStorage::writeString(CONFIG_KEY_SYSTEM_STA_PASSWORD, password))
        {
            return false;
        }
        return true;
    }
    bool isStaMode()
    {
        return isSTA;
    }

    bool setStaMode(const bool StaMode)
    {
        NvsStorage::Session session(SYSTEM_NVS_NAMESPACE, false);
        if (NvsStorage::writeBool(CONFIG_KEY_SYSTEM_STA_MODE, StaMode))
        {
            isSTA = StaMode;
            return true;
        }
        return false;
    }
    const char *getVersion()
    {
        return _version;
    }
}