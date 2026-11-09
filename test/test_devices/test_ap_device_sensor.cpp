// E: ApDeviceSensor - walking the source path through a device response; a value that is
// missing or has the wrong form makes the entity invalid, never 0 or the last value.
// F: ApDevice::applyResponse() - distributes one response to all entities.
#include "TestSupport.h"

namespace
{
    class Sensor : public DeviceTest
    {
    protected:
        // one device (id 1) with a single measurement entity reading <source>
        ApDeviceSensor *single(const char *deviceType, const char *source)
        {
            JsonDoc doc;
            JsonObject data = addCredentials(doc.to<JsonObject>(), "8cbfeaa03351");
            data["type"] = deviceType;
            JsonObject entity = data.createNestedArray("entities").createNestedObject();
            entity["type"] = "Temperature";
            entity["name"] = "Test";
            entity["source"] = source;
            device = onboard(1, data);
            EXPECT_NE(device, nullptr);
            return device ? sensorAt(*device, 0) : nullptr;
        }
        bool apply(ApDeviceSensor *sensor, const char *json)
        {
            JsonDoc doc;
            return sensor->applyResponse(parse(doc, json));
        }
        DevicePtr device;
    };

    float valueOf(ApDeviceSensor *sensor)
    {
        float value = -999.0f;
        EXPECT_TRUE(sensor->read(value, true));
        return value;
    }
}

// E32 + F41: the recorded Shelly response fills every entity of the Plug S template
TEST_F(Sensor, ShellyResponseFillsAllEntities)
{
    DevicePtr plug = onboardPlugS();
    ASSERT_NE(plug, nullptr);
    FakeArduino::now = 12345;
    JsonDoc doc;
    EXPECT_TRUE(plug->applyResponse(parse(doc, PLUG_S_GEN3_STATUS)));

    EXPECT_TRUE(plug->isOnline());
    EXPECT_EQ(plug->getLastSeen(), 12345u);
    EXPECT_FALSE(relayAt(*plug, 0)->getActualOn());
    EXPECT_FLOAT_EQ(valueOf(sensorAt(*plug, 1)), 12.5f);    // switch:0/apower
    EXPECT_FLOAT_EQ(valueOf(sensorAt(*plug, 2)), 345.678f); // switch:0/aenergy/total
    EXPECT_FLOAT_EQ(valueOf(sensorAt(*plug, 3)), 43.4f);    // switch:0/temperature/tC
    EXPECT_FLOAT_EQ(valueOf(sensorAt(*plug, 4)), 16887.0f); // sys/uptime
    EXPECT_FLOAT_EQ(valueOf(sensorAt(*plug, 5)), -40.0f);   // wifi/rssi
}

TEST_F(Sensor, MissingFieldOnThePathIsInvalid) // E33
{
    ApDeviceSensor *sensor = single("Shelly Gen2/3", "switch:0/temperature/tC");
    EXPECT_FALSE(apply(sensor, R"({"switch:0": {"temperature": {"tF": 110.2}}})"));
    EXPECT_FALSE(apply(sensor, R"({"switch:0": {}})"));
    EXPECT_FALSE(apply(sensor, R"({})"));
}

TEST_F(Sensor, PathEndingAtAnObjectIsInvalid) // E34
{
    ApDeviceSensor *sensor = single("Shelly Gen2/3", "switch:0/aenergy");
    EXPECT_FALSE(apply(sensor, R"({"switch:0": {"aenergy": {"total": 345.6}}})"));
}

// E35: numbers as text are invalid - a manufacturer sending that gets its own applyResponse()
TEST_F(Sensor, WrongValueTypeIsInvalid)
{
    ApDeviceSensor *sensor = single("Shelly Gen2/3", "sys/uptime");
    EXPECT_FALSE(apply(sensor, R"({"sys": {"uptime": "12.5"}})")) << "text";
    EXPECT_FALSE(apply(sensor, R"({"sys": {"uptime": true}})")) << "bool";
    EXPECT_FALSE(apply(sensor, R"({"sys": {"uptime": null}})")) << "null";
    EXPECT_FALSE(apply(sensor, R"({"sys": {"uptime": [1]}})")) << "list";
}

TEST_F(Sensor, C3ValidValue) // E36
{
    ApDeviceSensor *sensor = single("C3 Sensor Node", "sensor:0");
    EXPECT_TRUE(apply(sensor, C3_SENSORS));
    EXPECT_FLOAT_EQ(valueOf(sensor), 58500.0f);
}

TEST_F(Sensor, C3InvalidValues) // E37
{
    ApDeviceSensor *sensor = single("C3 Sensor Node", "sensor:1");
    EXPECT_FALSE(apply(sensor, C3_SENSORS)) << "value null, valid false";

    ApDeviceSensor *other = single("C3 Sensor Node", "sensor:0");
    EXPECT_FALSE(apply(other, R"({"sensor:0": {"value": null, "valid": true}})"));
    EXPECT_FALSE(apply(other, R"({"sensor:0": {"value": 21.5, "valid": false}})"));
    EXPECT_FALSE(apply(other, R"({"sensor:0": {"value": 21.5}})")) << "valid missing";
    EXPECT_FALSE(apply(other, R"({"sensor:0": {"valid": true}})")) << "value missing";
}

// E38: a later broken response must not leave the old value looking valid
TEST_F(Sensor, InvalidAfterValidStaysInvalid)
{
    ApDeviceSensor *sensor = single("Shelly Gen2/3", "sys/uptime");
    ASSERT_TRUE(apply(sensor, R"({"sys": {"uptime": 100}})"));
    EXPECT_FALSE(apply(sensor, R"({"sys": {}})"));
    float value = -999.0f;
    EXPECT_FALSE(sensor->read(value, false));
    EXPECT_FLOAT_EQ(value, -999.0f) << "read() leaves the target untouched when invalid";
}

TEST_F(Sensor, ScaleAndOffset) // E39
{
    ApDeviceSensor *sensor = single("Shelly Gen2/3", "sys/uptime");
    ASSERT_TRUE(sensor->setScale(2.0f));
    ASSERT_TRUE(sensor->setOffset(1.0f));
    ASSERT_TRUE(apply(sensor, R"({"sys": {"uptime": 10}})"));
    float value = 0.0f;
    ASSERT_TRUE(sensor->read(value, true));
    EXPECT_FLOAT_EQ(value, 10.0f);
    ASSERT_TRUE(sensor->read(value, false));
    EXPECT_FLOAT_EQ(value, 21.0f);
}

TEST_F(Sensor, NoResponseYetIsInvalid) // E40
{
    ApDeviceSensor *sensor = single("Shelly Gen2/3", "sys/uptime");
    float value = 0.0f;
    EXPECT_FALSE(sensor->read(value, true));
}

// F42: an entity that could not be loaded must not stop the others
TEST_F(Sensor, EmptyPositionDoesNotStopTheOthers)
{
    ASSERT_NE(onboardPlugS(), nullptr);
    FakeNvs::setInt(NS0, "e2_type", 99);
    DevicePtr plug = recover(0);
    ASSERT_NE(plug, nullptr);
    ASSERT_EQ(plug->getEntity(2), nullptr);

    JsonDoc doc;
    EXPECT_TRUE(plug->applyResponse(parse(doc, PLUG_S_GEN3_STATUS)));
    EXPECT_FLOAT_EQ(valueOf(sensorAt(*plug, 1)), 12.5f);
    EXPECT_FLOAT_EQ(valueOf(sensorAt(*plug, 3)), 43.4f);
}
