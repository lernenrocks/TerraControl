// A: ApDevice::onboard() - which onboarding data is accepted, and that a failed onboarding
// leaves nothing behind in the NVS.
#include "TestSupport.h"

namespace
{
    class Onboard : public DeviceTest
    {
    protected:
        // Plug S onboarding with one field changed; returns whether a device was created
        bool onboardWith(const char *field, const std::string &value)
        {
            JsonDoc doc;
            JsonObject data = plugS(doc);
            data[field] = value;
            return onboard(0, data) != nullptr;
        }
        // Plug S onboarding where entity <idx> is changed by edit(); returns whether a device was created
        template <typename Edit>
        bool onboardWithEntity(uint8_t idx, Edit edit)
        {
            JsonDoc doc;
            JsonObject data = plugS(doc);
            edit(data["entities"][idx].as<JsonObject>());
            return onboard(0, data) != nullptr;
        }
    };
}

// A1
TEST_F(Onboard, PlugSTemplateWithCredentials)
{
    DevicePtr device = onboardPlugS();
    ASSERT_NE(device, nullptr);

    EXPECT_EQ(device->getDeviceType(), DeviceType::SHELLY_GEN2);
    ASSERT_EQ(device->getEntityCount(), 6);
    const EntityType expected[] = {EntityType::SHELLY_GEN3_PLUG_S, EntityType::POWER, EntityType::ENERGY,
                                   EntityType::TEMPERATURE, EntityType::UPTIME, EntityType::RSSI};
    for (uint8_t i = 0; i < 6; i++)
        EXPECT_EQ(device->getEntityType(i), expected[i]) << "position " << int(i);

    ASSERT_NE(relayAt(*device, 0), nullptr);
    EXPECT_EQ(relayAt(*device, 0)->getOutputIdx(), 0);

    uint8_t mac[MAC_LEN] = {};
    ASSERT_TRUE(device->getMac(mac, sizeof(mac)));
    const uint8_t expectedMac[MAC_LEN] = {0x8c, 0xbf, 0xea, 0xa0, 0x33, 0x50};
    EXPECT_EQ(memcmp(mac, expectedMac, MAC_LEN), 0);

    char text[DEVICE_PASSWORD_LEN] = {};
    ASSERT_TRUE(device->getUsername(text, sizeof(text)));
    EXPECT_STREQ(text, "admin");
    ASSERT_TRUE(device->getPassword(text, sizeof(text)));
    EXPECT_STREQ(text, TEST_PASSWORD);
    EXPECT_TRUE(ApDevice::hasData(0));
}

// A2: the template alone has no MAC/credentials and must not become a device
TEST_F(Onboard, TemplateAloneIsRejected)
{
    JsonDoc doc;
    EXPECT_EQ(onboard(0, parse(doc, ApDeviceTemplates::Shelly::PLUG_S_GEN3)), nullptr);
    EXPECT_FALSE(ApDevice::hasData(0));
}

// A3
TEST_F(Onboard, MalformedMacIsRejected)
{
    EXPECT_FALSE(onboardWith("mac", "8c:bf:ea:a0:33:50")) << "with separators";
    EXPECT_FALSE(onboardWith("mac", "8cbfeaa0335")) << "11 characters";
    EXPECT_FALSE(onboardWith("mac", "8cbfeaa033501")) << "13 characters";
    EXPECT_FALSE(onboardWith("mac", "8cbfZaa03350")) << "non-hex in the middle";
    EXPECT_FALSE(onboardWith("mac", "")) << "empty";
}

// A3: sscanf("%2hhx") stops at the first non-hex character and still counts the byte as read
TEST_F(Onboard, MacWithNonHexAtTheEndIsRejected)
{
    EXPECT_FALSE(onboardWith("mac", "8cbfeaa0335g"));
}

// A4
TEST_F(Onboard, UppercaseMacIsAccepted)
{
    DevicePtr device = onboardPlugS(0, "8CBFEAA03350");
    ASSERT_NE(device, nullptr);
    uint8_t mac[MAC_LEN] = {};
    device->getMac(mac, sizeof(mac));
    EXPECT_EQ(mac[0], 0x8c);
    EXPECT_EQ(mac[5], 0x50);
}

// A5: the buffers hold LEN - 1 characters plus the terminator
TEST_F(Onboard, TextLengthLimits)
{
    EXPECT_FALSE(onboardWith("username", ""));
    EXPECT_TRUE(onboardWith("username", repeat('u', DEVICE_USERNAME_LEN - 1)));
    EXPECT_FALSE(onboardWith("username", repeat('u', DEVICE_USERNAME_LEN)));

    EXPECT_FALSE(onboardWith("password", ""));
    EXPECT_TRUE(onboardWith("password", repeat('p', DEVICE_PASSWORD_LEN - 1)));
    EXPECT_FALSE(onboardWith("password", repeat('p', DEVICE_PASSWORD_LEN)));

    EXPECT_TRUE(onboardWith("name", repeat('n', DEVICE_NAME_LEN - 1)));
    EXPECT_FALSE(onboardWith("name", repeat('n', DEVICE_NAME_LEN)));
}

// A6
TEST_F(Onboard, DeviceTypeAndEntityListAreChecked)
{
    EXPECT_FALSE(onboardWith("type", "Toaster")) << "unknown device type";

    JsonDoc doc;
    JsonObject data = plugS(doc);
    data.remove("entities");
    EXPECT_EQ(onboard(0, data), nullptr) << "entities missing";

    data.createNestedArray("entities");
    EXPECT_EQ(onboard(0, data), nullptr) << "entities empty";

    data.remove("entities");
    data.createNestedObject("entities")["type"] = "Power";
    EXPECT_EQ(onboard(0, data), nullptr) << "entities is an object, not a list";
}

// A7: the device data is already written when entity 1 fails - erase() must clean it up
TEST_F(Onboard, FailingEntityLeavesNothingBehind)
{
    EXPECT_FALSE(onboardWithEntity(1, [](JsonObject entity)
                                   { entity.remove("source"); }));
    EXPECT_FALSE(ApDevice::hasData(0));
    EXPECT_EQ(FakeNvs::keyCount(NS0), 0u);
}

// A8: "output" is mandatory for a relay and must fit a Shelly id
TEST_F(Onboard, RelayOutputIsChecked)
{
    EXPECT_FALSE(onboardWithEntity(0, [](JsonObject e)
                                   { e.remove("output"); }))
        << "missing";
    EXPECT_FALSE(onboardWithEntity(0, [](JsonObject e)
                                   { e["output"] = -1; }))
        << "-1";
    EXPECT_FALSE(onboardWithEntity(0, [](JsonObject e)
                                   { e["output"] = 256; }))
        << "256";
    EXPECT_FALSE(onboardWithEntity(0, [](JsonObject e)
                                   { e["output"] = true; }))
        << "bool";
    EXPECT_TRUE(onboardWithEntity(0, [](JsonObject e)
                                  { e["output"] = 255; }))
        << "255";
}

// A9: "source" is mandatory for a measurement entity
TEST_F(Onboard, SensorSourceIsChecked)
{
    EXPECT_FALSE(onboardWithEntity(1, [](JsonObject e)
                                   { e["source"] = ""; }))
        << "empty";
    EXPECT_FALSE(onboardWithEntity(1, [](JsonObject e)
                                   { e["source"] = repeat('s', SENSOR_SOURCE_LEN); }))
        << "too long";
    EXPECT_TRUE(onboardWithEntity(1, [](JsonObject e)
                                  { e["source"] = repeat('s', SENSOR_SOURCE_LEN - 1); }))
        << "longest allowed";
    EXPECT_FALSE(onboardWithEntity(1, [](JsonObject e)
                                   { e["source"] = 42; }))
        << "number instead of text";
}

// A10: "" is a valid name - stored and returned as it is, the default is only a suggestion
TEST_F(Onboard, MissingNamesAreStoredEmpty)
{
    JsonDoc doc;
    JsonObject data = plugS(doc);
    data.remove("name");
    data["entities"][0].as<JsonObject>().remove("name");
    DevicePtr device = onboard(0, data);
    ASSERT_NE(device, nullptr);

    char name[DEVICE_NAME_LEN] = {};
    ASSERT_TRUE(device->getName(name, sizeof(name)));
    EXPECT_STREQ(name, "");
    EXPECT_TRUE(FakeNvs::hasKey(NS0, "name"));
    ASSERT_TRUE(device->getDefaultName(name, sizeof(name)));
    EXPECT_STREQ(name, "WiFi Device 0");

    ASSERT_TRUE(device->getEntity(0)->getName(name, sizeof(name)));
    EXPECT_STREQ(name, "");
    EXPECT_TRUE(FakeNvs::hasKey(NS0, "e0_name"));
    EXPECT_FALSE(logged("nvs write failed"));
}
