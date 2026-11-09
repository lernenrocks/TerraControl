// D: ShellyGen2Relay - building the switch request and reading the relay state from the
// device status. applyResponse() must never guess a state.
#include "TestSupport.h"
#include "ShellyGen2Relay.h"

namespace
{
    class ShellyRelay : public DeviceTest
    {
    protected:
        void SetUp() override
        {
            DeviceTest::SetUp();
            device = onboardPlugS();
            ASSERT_NE(device, nullptr);
            relay = relayAt(*device, 0);
            ASSERT_NE(relay, nullptr);
        }
        // Shelly status with <edit> applied to "switch:0"
        template <typename Edit>
        bool applyStatus(Edit edit)
        {
            JsonDoc doc;
            JsonObject status = parse(doc, PLUG_S_GEN3_STATUS);
            edit(status);
            return relay->applyResponse(status);
        }
        DevicePtr device;
        ApDeviceRelayBase *relay = nullptr;
    };
}

// D27: builds the URI only - sending is the NetworkWorker's job
TEST_F(ShellyRelay, SwitchOnCarriesTheFailsafe)
{
    char uri[RELAY_REQUEST_LEN] = {};
    ASSERT_TRUE(relay->buildSwitchRequest(true, uri, sizeof(uri)));
    EXPECT_STREQ(uri, "rpc/Switch.Set?id=0&on=true&toggle_after=300");

    ASSERT_TRUE(relay->setFailsafeSeconds(90));
    relay->buildSwitchRequest(true, uri, sizeof(uri));
    EXPECT_STREQ(uri, "rpc/Switch.Set?id=0&on=true&toggle_after=90");
}

// D27: toggle_after on OFF would switch the relay back ON after the timer (finding 1, 2026-09-26)
TEST_F(ShellyRelay, SwitchOffHasNoTimer)
{
    char uri[RELAY_REQUEST_LEN] = {};
    ASSERT_TRUE(relay->buildSwitchRequest(false, uri, sizeof(uri)));
    EXPECT_STREQ(uri, "rpc/Switch.Set?id=0&on=false");
}

TEST_F(ShellyRelay, RequestUsesOutputIdxNotPosition)
{
    JsonDoc doc;
    JsonObject data = plugS(doc);
    data["entities"][0]["output"] = 2;
    DevicePtr other = onboard(1, data);
    ASSERT_NE(other, nullptr);
    char uri[RELAY_REQUEST_LEN] = {};
    relayAt(*other, 0)->buildSwitchRequest(false, uri, sizeof(uri));
    EXPECT_STREQ(uri, "rpc/Switch.Set?id=2&on=false");
}

TEST_F(ShellyRelay, TooSmallBufferIsRefused) // D28
{
    char uri[RELAY_REQUEST_LEN - 1] = {};
    EXPECT_FALSE(relay->buildSwitchRequest(true, uri, sizeof(uri)));
}

TEST_F(ShellyRelay, StateFromRealResponse) // D29
{
    EXPECT_TRUE(applyStatus([](JsonObject s)
                            { s["switch:0"]["output"] = true; }));
    EXPECT_TRUE(relay->getActualOn());
    EXPECT_TRUE(applyStatus([](JsonObject) {})) << "recorded response: output false";
    EXPECT_FALSE(relay->getActualOn());
}

// D30: start from "on", then a broken response must not change it to "off"
TEST_F(ShellyRelay, BrokenResponseKeepsTheLastState)
{
    ASSERT_TRUE(applyStatus([](JsonObject s)
                            { s["switch:0"]["output"] = true; }));

    EXPECT_FALSE(applyStatus([](JsonObject s)
                             { s.remove("switch:0"); }))
        << "switch:0 missing";
    EXPECT_FALSE(applyStatus([](JsonObject s)
                             { s["switch:0"].as<JsonObject>().remove("output"); }))
        << "output missing";
    EXPECT_FALSE(applyStatus([](JsonObject s)
                             { s["switch:0"]["output"] = "on"; }))
        << "output is text";
    EXPECT_FALSE(applyStatus([](JsonObject s)
                             { s["switch:0"]["output"] = 0; }))
        << "output is a number";
    EXPECT_TRUE(relay->getActualOn());
}

// D31: a relay on output 1 must not read the state of output 0
TEST_F(ShellyRelay, OtherOutputIsNotUsed)
{
    JsonDoc doc;
    JsonObject data = plugS(doc);
    data["entities"][0]["output"] = 1;
    DevicePtr other = onboard(1, data);
    ASSERT_NE(other, nullptr);
    JsonDoc statusDoc;
    EXPECT_FALSE(relayAt(*other, 0)->applyResponse(parse(statusDoc, PLUG_S_GEN3_STATUS)));
}
