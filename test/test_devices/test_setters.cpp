// C: setters write the NVS first and change the RAM value only after that succeeded -
// with a failing NVS (FakeNvs::failWrites) RAM and NVS must still agree.
#include "TestSupport.h"

namespace
{
    class Setters : public DeviceTest
    {
    protected:
        void SetUp() override
        {
            DeviceTest::SetUp();
            device = onboardPlugS();
            ASSERT_NE(device, nullptr);
            relay = relayAt(*device, 0);
            power = sensorAt(*device, 1);
            ASSERT_NE(relay, nullptr);
            ASSERT_NE(power, nullptr);
        }
        // simulates a reboot: object gone, NVS stays, load again
        void reboot()
        {
            device.reset();
            device = recover(0);
            ASSERT_NE(device, nullptr);
            relay = relayAt(*device, 0);
            power = sensorAt(*device, 1);
        }
        DevicePtr device;
        ApDeviceRelayBase *relay = nullptr;
        ApDeviceSensor *power = nullptr;
    };
}

TEST_F(Setters, ValuesSurviveAReboot) // C23
{
    ASSERT_TRUE(device->setName("Terrarium 1"));
    ASSERT_TRUE(relay->setName("Heat lamp"));
    ASSERT_TRUE(relay->setMode(RelayMode::FORCED_ON));
    ASSERT_TRUE(relay->setFailsafeSeconds(120));
    ASSERT_TRUE(power->setScale(2.0f));
    ASSERT_TRUE(power->setOffset(1.5f));
    ASSERT_TRUE(power->setPrecision(3));
    ASSERT_TRUE(power->setUnit("kW"));

    reboot();

    char text[ENTITY_NAME_LEN] = {};
    device->getName(text, sizeof(text));
    EXPECT_STREQ(text, "Terrarium 1");
    relay->getName(text, sizeof(text));
    EXPECT_STREQ(text, "Heat lamp");
    EXPECT_EQ(relay->getMode(), RelayMode::FORCED_ON);
    EXPECT_EQ(relay->getFailsafeSeconds(), 120);
    EXPECT_FLOAT_EQ(power->getScale(), 2.0f);
    EXPECT_FLOAT_EQ(power->getOffset(), 1.5f);
    EXPECT_EQ(power->getPrecision(), 3);
    EXPECT_STREQ(power->getUnit(), "kW");
}

TEST_F(Setters, FailingNvsLeavesRamUnchanged)
{
    FakeNvs::failWrites = true;

    EXPECT_FALSE(device->setName("new"));
    EXPECT_FALSE(relay->setName("new"));
    EXPECT_FALSE(relay->setMode(RelayMode::FORCED_ON));
    EXPECT_FALSE(relay->setFailsafeSeconds(120));
    EXPECT_FALSE(power->setScale(2.0f));
    EXPECT_FALSE(power->setOffset(1.5f));
    EXPECT_FALSE(power->setPrecision(3));
    EXPECT_FALSE(power->setUnit("kW"));

    char text[ENTITY_NAME_LEN] = {};
    device->getName(text, sizeof(text));
    EXPECT_STREQ(text, "");
    relay->getName(text, sizeof(text));
    EXPECT_STREQ(text, "Relay 1");
    EXPECT_EQ(relay->getMode(), RelayMode::AUTO);
    EXPECT_EQ(relay->getFailsafeSeconds(), DEFAULT_FAILSAFE_TIMEOUT_SEC);
    EXPECT_FLOAT_EQ(power->getScale(), DEFAULT_SCALE);
    EXPECT_FLOAT_EQ(power->getOffset(), DEFAULT_OFFSET);
    EXPECT_EQ(power->getPrecision(), getDefaultPrecision(EntityType::POWER));
    EXPECT_STREQ(power->getUnit(), "W");
    EXPECT_TRUE(logged("nvs write failed"));
}

// A10 at runtime: clearing a name must reach the NVS and the RAM alike
TEST_F(Setters, EmptyStringsCanBeSet)
{
    ASSERT_TRUE(device->setName("x"));
    EXPECT_TRUE(device->setName(""));
    EXPECT_TRUE(relay->setName(""));
    EXPECT_TRUE(power->setUnit(""));
    EXPECT_FALSE(logged("nvs write failed"));

    reboot();
    char text[ENTITY_NAME_LEN] = {};
    device->getName(text, sizeof(text));
    EXPECT_STREQ(text, "");
    relay->getName(text, sizeof(text));
    EXPECT_STREQ(text, "");
    EXPECT_STREQ(power->getUnit(), "");
}

TEST_F(Setters, NameLengthLimits)
{
    EXPECT_TRUE(device->setName(repeat('n', DEVICE_NAME_LEN - 1).c_str()));
    EXPECT_FALSE(device->setName(repeat('n', DEVICE_NAME_LEN).c_str()));
    EXPECT_FALSE(device->setName(nullptr));
    EXPECT_TRUE(relay->setName(repeat('n', ENTITY_NAME_LEN - 1).c_str()));
    EXPECT_FALSE(relay->setName(repeat('n', ENTITY_NAME_LEN).c_str()));
    EXPECT_FALSE(relay->setName(nullptr));
}

TEST_F(Setters, FailsafeMinimum) // C19
{
    EXPECT_FALSE(relay->setFailsafeSeconds(MIN_FAILSAFE_SEC - 1));
    EXPECT_FALSE(relay->setFailsafeSeconds(0));
    EXPECT_FALSE(relay->setFailsafeSeconds(-5));
    EXPECT_EQ(relay->getFailsafeSeconds(), DEFAULT_FAILSAFE_TIMEOUT_SEC);
    EXPECT_TRUE(relay->setFailsafeSeconds(MIN_FAILSAFE_SEC));
    EXPECT_EQ(relay->getFailsafeSeconds(), MIN_FAILSAFE_SEC);
}

TEST_F(Setters, ScaleAndOffsetMustBeUsable) // C20
{
    EXPECT_FALSE(power->setScale(0.0f));
    EXPECT_FALSE(power->setScale(INFINITY));
    EXPECT_FALSE(power->setScale(NAN));
    EXPECT_FALSE(power->setOffset(NAN));
    EXPECT_FALSE(power->setOffset(-INFINITY));
    EXPECT_FLOAT_EQ(power->getScale(), DEFAULT_SCALE);
    EXPECT_FLOAT_EQ(power->getOffset(), DEFAULT_OFFSET);
    EXPECT_TRUE(power->setScale(-1.0f)) << "negative scale is allowed (e.g. inverted sensor)";
}

TEST_F(Setters, PrecisionMustNotBeNegative) // C21
{
    EXPECT_FALSE(power->setPrecision(-1));
    EXPECT_TRUE(logged("precision must not be negative"));
    EXPECT_TRUE(power->setPrecision(0));
}

TEST_F(Setters, UnitLengthLimit) // C22
{
    EXPECT_TRUE(power->setUnit(repeat('u', SENSOR_UNIT_LEN - 1).c_str()));
    EXPECT_FALSE(power->setUnit(repeat('u', SENSOR_UNIT_LEN).c_str()));
    EXPECT_FALSE(power->setUnit(nullptr));
}

// C24: the setter never stores less than 30 s - if the NVS holds it anyway, use the default
TEST_F(Setters, StoredFailsafeBelowMinimumFallsBackToDefault)
{
    FakeNvs::setInt(NS0, "e0_failsafe", 10);
    reboot();
    EXPECT_EQ(relay->getFailsafeSeconds(), DEFAULT_FAILSAFE_TIMEOUT_SEC);
    EXPECT_TRUE(logged("[WARN]"));
}

TEST_F(Setters, StoredFailsafeAtMinimumIsUsed)
{
    FakeNvs::setInt(NS0, "e0_failsafe", MIN_FAILSAFE_SEC);
    reboot();
    EXPECT_EQ(relay->getFailsafeSeconds(), MIN_FAILSAFE_SEC);
}

// C25: a mode stored by a newer firmware (rollback) - shown as NOT_IN_LIST, never guessed
TEST_F(Setters, UnknownStoredModeIsNotInList)
{
    FakeNvs::setInt(NS0, "e0_mode", 3);
    reboot();
    EXPECT_EQ(relay->getMode(), RelayMode::NOT_IN_LIST);
    EXPECT_TRUE(logged("[WARN]"));
    EXPECT_EQ(FakeNvs::getInt(NS0, "e0_mode"), 3) << "the stored value stays untouched";

    EXPECT_TRUE(relay->setMode(RelayMode::AUTO)) << "the user can choose a known mode again";
    EXPECT_EQ(relay->getMode(), RelayMode::AUTO);
}

TEST_F(Setters, NotInListCannotBeSet)
{
    EXPECT_FALSE(relay->setMode(RelayMode::NOT_IN_LIST));
    EXPECT_FALSE(relay->setMode(static_cast<RelayMode>(9))) << "value outside the enum";
    EXPECT_EQ(relay->getMode(), RelayMode::AUTO);
    EXPECT_FALSE(FakeNvs::hasKey(NS0, "e0_mode"));
}

TEST_F(Setters, MissingModeIsAuto)
{
    EXPECT_FALSE(FakeNvs::hasKey(NS0, "e0_mode")) << "onboarding does not write the mode";
    reboot();
    EXPECT_EQ(relay->getMode(), RelayMode::AUTO);
}

// C26: reset() removes what the user set; keys that were never set are no error
TEST_F(Setters, SensorResetRestoresDefaults)
{
    ASSERT_TRUE(power->setScale(2.0f));
    EXPECT_TRUE(power->reset());
    EXPECT_FLOAT_EQ(power->getScale(), DEFAULT_SCALE);
    EXPECT_FLOAT_EQ(power->getOffset(), DEFAULT_OFFSET);
    EXPECT_EQ(power->getPrecision(), getDefaultPrecision(EntityType::POWER));
    EXPECT_STREQ(power->getUnit(), "W");
    EXPECT_FALSE(FakeNvs::hasKey(NS0, "e1_scale"));

    reboot();
    EXPECT_FLOAT_EQ(power->getScale(), DEFAULT_SCALE);
}
