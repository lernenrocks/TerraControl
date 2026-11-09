// B: ApDevice::recover() - loading at boot, including NVS content that is damaged or was
// written by another firmware version. Each test onboards a Plug S, deletes the object
// (the NVS stays, like after a reboot), changes the NVS directly and loads again.
#include "TestSupport.h"

namespace
{
    class Recover : public DeviceTest
    {
    protected:
        void SetUp() override
        {
            DeviceTest::SetUp();
            JsonDoc doc;
            JsonObject data = plugS(doc);
            data["name"] = "Heating";
            ASSERT_NE(onboard(0, data), nullptr); // object deleted right away, NVS stays
        }
    };
}

// B11
TEST_F(Recover, EverythingComesBack)
{
    DevicePtr device = recover(0);
    ASSERT_NE(device, nullptr);

    EXPECT_EQ(device->getDeviceType(), DeviceType::SHELLY_GEN2);
    char text[DEVICE_PASSWORD_LEN] = {};
    device->getName(text, sizeof(text));
    EXPECT_STREQ(text, "Heating");
    device->getUsername(text, sizeof(text));
    EXPECT_STREQ(text, "admin");
    uint8_t mac[MAC_LEN] = {};
    device->getMac(mac, sizeof(mac));
    EXPECT_EQ(mac[0], 0x8c);

    ASSERT_EQ(device->getEntityCount(), 6);
    const char *names[] = {"Relay 1", "Power 1", "Energy 1", "Temperature 1", "Uptime", "RSSI"};
    for (uint8_t i = 0; i < 6; i++)
    {
        ASSERT_NE(device->getEntity(i), nullptr) << "position " << int(i);
        device->getEntity(i)->getName(text, sizeof(text));
        EXPECT_STREQ(text, names[i]);
    }
    EXPECT_EQ(device->getEntityType(5), EntityType::RSSI);
    EXPECT_EQ(relayAt(*device, 0)->getOutputIdx(), 0);
    EXPECT_EQ(relayAt(*device, 0)->getFailsafeSeconds(), DEFAULT_FAILSAFE_TIMEOUT_SEC);
    EXPECT_EQ(relayAt(*device, 0)->getMode(), RelayMode::AUTO);
    EXPECT_FALSE(device->isOnline()) << "online is RAM-only";
}

// B12: no MAC -> not loadable, but the slot still has data -> defective, not free
TEST_F(Recover, MissingMacMakesTheSlotDefective)
{
    FakeNvs::eraseKey(NS0, "mac");
    EXPECT_EQ(recover(0), nullptr);
    EXPECT_TRUE(ApDevice::hasData(0));
}

// B13: a type this firmware does not know (e.g. after a rollback) - nothing may shift
TEST_F(Recover, UnknownEntityTypeKeepsItsPosition)
{
    FakeNvs::setInt(NS0, "e2_type", 99);
    DevicePtr device = recover(0);
    ASSERT_NE(device, nullptr);
    ASSERT_EQ(device->getEntityCount(), 6);
    EXPECT_EQ(device->getEntity(2), nullptr);
    EXPECT_EQ(device->getEntityType(2), EntityType::NONE);
    EXPECT_EQ(device->getEntityType(3), EntityType::TEMPERATURE);
    EXPECT_EQ(device->getEntityType(4), EntityType::UPTIME);
    EXPECT_EQ(device->getEntityType(5), EntityType::RSSI);
    EXPECT_TRUE(FakeNvs::hasKey(NS0, "e2_source")) << "NVS of the unknown entity stays untouched";
}

// B14: mandatory fields of an entity
TEST_F(Recover, EntityWithoutMandatoryFieldIsEmpty)
{
    FakeNvs::eraseKey(NS0, "e0_output");
    FakeNvs::eraseKey(NS0, "e1_source");
    DevicePtr device = recover(0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getEntity(0), nullptr) << "relay without output";
    EXPECT_EQ(device->getEntity(1), nullptr) << "sensor without source";
    EXPECT_NE(device->getEntity(2), nullptr);
}

// B15: onboarding never stores a device without entities - a missing count is an NVS fault
TEST_F(Recover, MissingEntityCountMakesTheSlotDefective)
{
    FakeNvs::eraseKey(NS0, "entCount");
    EXPECT_EQ(recover(0), nullptr);
    EXPECT_TRUE(ApDevice::hasData(0));
    EXPECT_TRUE(logged("invalid entity count"));
}

TEST_F(Recover, ZeroEntityCountMakesTheSlotDefective)
{
    FakeNvs::setInt(NS0, "entCount", 0);
    EXPECT_EQ(recover(0), nullptr);
    EXPECT_TRUE(ApDevice::hasData(0));
}

// B16: the device still loads; the NetworkWorker has to skip it (review list, item 7)
TEST_F(Recover, MissingDeviceTypeLoadsAsNone)
{
    FakeNvs::eraseKey(NS0, "devType");
    DevicePtr device = recover(0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), DeviceType::NONE);
    EXPECT_TRUE(logged("no type"));
}

// B17 (changed with A10): an empty stored name comes back empty, not as the default
TEST_F(Recover, EmptyNameStaysEmpty)
{
    FakeNvs::setString(NS0, "name", "");
    DevicePtr device = recover(0);
    ASSERT_NE(device, nullptr);
    char name[DEVICE_NAME_LEN] = {};
    device->getName(name, sizeof(name));
    EXPECT_STREQ(name, "");
}

// B18: the password is read from the NVS on every call, never kept in RAM
TEST_F(Recover, PasswordNeedsAFullSizeBuffer)
{
    DevicePtr device = recover(0);
    ASSERT_NE(device, nullptr);
    char small[DEVICE_PASSWORD_LEN - 1] = {};
    EXPECT_FALSE(device->getPassword(small, sizeof(small)));
    char password[DEVICE_PASSWORD_LEN] = {};
    ASSERT_TRUE(device->getPassword(password, sizeof(password)));
    EXPECT_STREQ(password, TEST_PASSWORD);
}
