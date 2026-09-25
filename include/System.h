#pragma once
#include <Arduino.h>

constexpr const uint8_t SYSTEM_DEVICE_NAME_LEN=32;
constexpr const uint8_t SYSTEM_DEVICE_H1_LEN=64;
constexpr const uint8_t SYSTEM_DEVICE_PASSWORD_LEN=64;
constexpr const uint8_t SYSTEM_STA_SSID_LEN=33; // max SSID 32 chars (IEEE 802.11) + terminator
constexpr const uint8_t SYSTEM_STA_PASSWORD_LEN=64;
constexpr const uint8_t SYSTEM_WIFI_PASSWORD_MIN_LEN=8; // WPA2 minimum, shared by AP- and STA-Passwort




namespace System
{
    bool init(const char *version);
    bool getDeviceName(char * buffer, uint8_t len);
    bool setDeviceName(const char *name);
    bool getDevicePassword(char *buffer, uint8_t len);
    bool setDevicePassword(const char *passwort);
    bool getDevicePasswordH1(char *buffer, uint8_t len);
    bool setDevicePasswordH1(const char *h1);
    bool getStaSsid(char *buffer, uint8_t len);
    bool setStaSsid(const char *ssid);
    bool getStaPassword(char *buffer, uint8_t len);
    bool setStaPassword(const char *password);
    bool isStaMode();
    bool setStaMode(const bool);
    const char *getVersion();
}