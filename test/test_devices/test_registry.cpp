// ApDeviceRegistry - slots, onboarding through create(), defective slots, duplicate MACs.
// The registry keeps its devices in a file-scope array that survives from test to test:
// SetUp() starts from an empty NVS, TearDown() removes every device again.
#include "TestSupport.h"
#include "ApDeviceRegistry.h"

namespace
{
    class Registry : public DeviceTest
    {
    protected:
        void SetUp() override
        {
            DeviceTest::SetUp();
            ApDeviceRegistry::init(); // empty NVS -> every slot nullptr
        }
        void TearDown() override
        {
            {
                ApDeviceRegistry::Lock lock;
                for (uint8_t i = 0; i < DEVICE_MAX; i++)
                    ApDeviceRegistry::remove(i);
            }
            DeviceTest::TearDown();
        }
        static bool create(const char *mac, const char *name = "")
        {
            JsonDoc doc;
            JsonObject data = plugS(doc, mac);
            data["name"] = name;
            return ApDeviceRegistry::create(data);
        }
        static std::string macFor(int i)
        {
            char mac[MAC_STRING_LEN] = {};
            snprintf(mac, sizeof(mac), "8cbfeaa033%02x", i);
            return mac;
        }
        static std::string nameOf(ApDevice *device)
        {
            char name[DEVICE_NAME_LEN] = {};
            device->getName(name, sizeof(name));
            return name;
        }
    };
}

TEST_F(Registry, CreateUsesTheFirstFreeSlot)
{
    ApDeviceRegistry::Lock lock;
    EXPECT_EQ(ApDeviceRegistry::getFreeSlots(), DEVICE_MAX);
    ASSERT_TRUE(create(TEST_MAC));
    EXPECT_NE(ApDeviceRegistry::find(lock, uint8_t(0)), nullptr);
    EXPECT_EQ(ApDeviceRegistry::getFreeSlots(), DEVICE_MAX - 1);
}

TEST_F(Registry, CreateWithoutMacIsRefused)
{
    ApDeviceRegistry::Lock lock;
    JsonDoc doc;
    JsonObject data = plugS(doc);
    data.remove("mac");
    EXPECT_FALSE(ApDeviceRegistry::create(data));
    EXPECT_EQ(ApDeviceRegistry::getFreeSlots(), DEVICE_MAX);
}

// a failed onboarding must not occupy the slot
TEST_F(Registry, FailedOnboardingKeepsTheSlotFree)
{
    ApDeviceRegistry::Lock lock;
    JsonDoc doc;
    JsonObject data = plugS(doc);
    data["type"] = "Toaster";
    EXPECT_FALSE(ApDeviceRegistry::create(data));
    EXPECT_EQ(ApDeviceRegistry::getFreeSlots(), DEVICE_MAX);
    EXPECT_FALSE(ApDevice::hasData(0));
}

TEST_F(Registry, FullRegistryRefusesMore)
{
    ApDeviceRegistry::Lock lock;
    for (int i = 0; i < static_cast<int>(DEVICE_MAX); i++)
        ASSERT_TRUE(create(macFor(i).c_str())) << "device " << i;
    EXPECT_EQ(ApDeviceRegistry::getFreeSlots(), 0);
    EXPECT_FALSE(create(macFor(DEVICE_MAX).c_str()));
    EXPECT_TRUE(logged("no space left"));
}

TEST_F(Registry, NearlyFullNvsRefusesOnboarding)
{
    ApDeviceRegistry::Lock lock;
    FakeNvs::setUsableFreeEntries(39);
    EXPECT_FALSE(create(TEST_MAC));
    EXPECT_FALSE(ApDevice::hasData(0)) << "nothing written";
    FakeNvs::setUsableFreeEntries(40);
    EXPECT_TRUE(create(TEST_MAC));
}

// slot 0 has data but cannot be loaded: reported at boot, not free, skipped by create()
TEST_F(Registry, DefectiveSlotIsNotFree)
{
    FakeNvs::setInt(NS0, "devType", static_cast<int>(DeviceType::SHELLY_GEN2)); // no MAC
    ApDeviceRegistry::init();
    EXPECT_TRUE(logged("slot defect"));

    ApDeviceRegistry::Lock lock;
    EXPECT_EQ(ApDeviceRegistry::find(lock, uint8_t(0)), nullptr);
    EXPECT_EQ(ApDeviceRegistry::getFreeSlots(), DEVICE_MAX - 1);
    ASSERT_TRUE(create(TEST_MAC));
    EXPECT_NE(ApDeviceRegistry::find(lock, uint8_t(1)), nullptr);
    EXPECT_TRUE(FakeNvs::hasKey(NS0, "devType")) << "defective slot stays untouched";
}

// boot: devices in the NVS are loaded into their own slots
TEST_F(Registry, InitLoadsStoredDevices)
{
    ASSERT_NE(onboardPlugS(0, macFor(0).c_str()), nullptr);
    ASSERT_NE(onboardPlugS(3, macFor(3).c_str()), nullptr);
    ApDeviceRegistry::init();

    ApDeviceRegistry::Lock lock;
    EXPECT_NE(ApDeviceRegistry::find(lock, uint8_t(0)), nullptr);
    EXPECT_EQ(ApDeviceRegistry::find(lock, uint8_t(1)), nullptr);
    EXPECT_NE(ApDeviceRegistry::find(lock, uint8_t(3)), nullptr);
    EXPECT_EQ(ApDeviceRegistry::getFreeSlots(), DEVICE_MAX - 2);
}

TEST_F(Registry, RemoveErasesTheNvs)
{
    ApDeviceRegistry::Lock lock;
    ASSERT_TRUE(create(TEST_MAC));
    EXPECT_TRUE(ApDeviceRegistry::remove(0));
    EXPECT_EQ(ApDeviceRegistry::find(lock, uint8_t(0)), nullptr);
    EXPECT_FALSE(ApDevice::hasData(0));
    EXPECT_EQ(FakeNvs::keyCount(NS0), 0u);
    EXPECT_FALSE(ApDeviceRegistry::remove(DEVICE_MAX)) << "id out of range";
}

// the same Shelly onboarded again replaces the old device (wizard will ask first, later)
TEST_F(Registry, DuplicateMacReplacesTheOldDevice)
{
    ApDeviceRegistry::Lock lock;
    ASSERT_TRUE(create(TEST_MAC, "old"));
    ASSERT_TRUE(create(macFor(1).c_str()));
    ASSERT_TRUE(create("8CBFEAA03350", "new")) << "same MAC in capitals";

    EXPECT_EQ(ApDeviceRegistry::getFreeSlots(), DEVICE_MAX - 2);
    const uint8_t mac[MAC_LEN] = {0x8c, 0xbf, 0xea, 0xa0, 0x33, 0x50};
    ApDevice *device = ApDeviceRegistry::find(lock, mac);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(nameOf(device), "new");
    EXPECT_EQ(device->getId(), 0) << "lowest free slot after removing the old one";
}

TEST_F(Registry, FindByMac)
{
    ApDeviceRegistry::Lock lock;
    ASSERT_TRUE(create(macFor(0).c_str()));
    ASSERT_TRUE(create(macFor(1).c_str()));
    const uint8_t second[MAC_LEN] = {0x8c, 0xbf, 0xea, 0xa0, 0x33, 0x01};
    ApDevice *device = ApDeviceRegistry::find(lock, second);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getId(), 1);
    const uint8_t unknown[MAC_LEN] = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(ApDeviceRegistry::find(lock, unknown), nullptr);

    uint8_t buffer[MAC_LEN] = {};
    EXPECT_TRUE(ApDeviceRegistry::getMac(1, buffer, sizeof(buffer)));
    EXPECT_EQ(memcmp(buffer, second, MAC_LEN), 0);
    EXPECT_FALSE(ApDeviceRegistry::getMac(5, buffer, sizeof(buffer))) << "empty slot";
}
