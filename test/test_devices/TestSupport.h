#pragma once
// Shared setup for the device tests: a fixture that resets the fake NVS/time/mutexes before
// every test and checks afterwards that no mutex was taken twice, plus helpers to build
// onboarding JSON.
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <ArduinoJson.h>
#include "FakeNvs.h"
#include "Arduino.h"
#include "NvsStorage.h"
#include "ApDevice.h"
#include "ApDeviceRelayBase.h"
#include "ApDeviceSensor.h"
#include "ApDeviceTemplates.h"
#include "Responses.h"

// unique_ptr deletes the device when the test ends - no manual delete, no leak on an early
// return from a failed ASSERT. The factories still return a raw pointer, the test wraps it.
using DevicePtr = std::unique_ptr<ApDevice>;
using JsonDoc = StaticJsonDocument<8192>; // test-only size, large enough for the Shelly status

constexpr char TEST_MAC[] = "8cbfeaa03350";
constexpr char TEST_PASSWORD[] = "secret-pw";
constexpr char NS0[] = "apDev_0"; // NVS namespace of device 0

class DeviceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeNvs::reset();
        FakeArduino::reset();
        FakeRtos::violations = 0;
        NvsStorage::init();
    }

    void TearDown() override
    {
        // on the ESP32 this would have been a deadlock (nested Session/Lock) or a crash (no init)
        EXPECT_EQ(FakeRtos::violations, 0) << "a mutex was taken twice or never created";
    }

    static bool logged(const char *text)
    {
        return FakeArduino::serialOutput.find(text) != std::string::npos;
    }
};

inline JsonObject parse(JsonDoc &doc, const char *json)
{
    const DeserializationError error = deserializeJson(doc, json);
    EXPECT_FALSE(error) << error.c_str();
    return doc.as<JsonObject>();
}

inline JsonObject addCredentials(JsonObject data, const char *mac = TEST_MAC)
{
    data["mac"] = mac;
    data["username"] = "admin";
    data["password"] = TEST_PASSWORD;
    return data;
}

// The Plug S template plus credentials - what the wizard will hand to onboard()
inline JsonObject plugS(JsonDoc &doc, const char *mac = TEST_MAC)
{
    return addCredentials(parse(doc, ApDeviceTemplates::Shelly::PLUG_S_GEN3), mac);
}

inline DevicePtr onboard(uint8_t id, JsonObjectConst data) { return DevicePtr(ApDevice::onboard(id, data)); }
inline DevicePtr recover(uint8_t id) { return DevicePtr(ApDevice::recover(id)); }

inline DevicePtr onboardPlugS(uint8_t id = 0, const char *mac = TEST_MAC)
{
    JsonDoc doc;
    return onboard(id, plugS(doc, mac));
}

// nullptr if the position is empty or holds another kind of entity
inline ApDeviceRelayBase *relayAt(const ApDevice &device, uint8_t idx)
{
    return dynamic_cast<ApDeviceRelayBase *>(device.getEntity(idx));
}
inline ApDeviceSensor *sensorAt(const ApDevice &device, uint8_t idx)
{
    return dynamic_cast<ApDeviceSensor *>(device.getEntity(idx));
}

inline std::string repeat(char c, size_t count) { return std::string(count, c); }
