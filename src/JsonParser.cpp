#include "JsonParser.h"
#include "NetworkUtils.h"
#include "DataHub.h"
namespace
{
    using namespace JsonParser;
    bool parseWiFiRelayEntry(const JsonObject &switchObjectJson, DataHub::WifiRelay &entry);
    
    
    ParseResult parseWifiRelayStatus(JsonObject switchStatusJson)
    {
        if (!(switchStatusJson.containsKey("sys") && (switchStatusJson.containsKey("wifi"))))
        {
            Serial.println("sys und wifi nicht im Json enthalten");
            return ParseResult::INVALID_JSON;
        }
        JsonObject sys = switchStatusJson["sys"].as<JsonObject>();
        JsonObject wifi = switchStatusJson["wifi"].as<JsonObject>();
        if (!sys || !wifi)
        {
            Serial.println("sys oder wifi im Json NULL");
            return ParseResult::INVALID_JSON;
        }
        if (!sys.containsKey("mac"))
        {
            Serial.println("Mac nicht im Json enthalten");
            return ParseResult::FIELD_MISSING;
        }
        char mac[MAC_LEN] = {};
        strncpy(mac, sys["mac"], MAC_LEN - 1);
        NetworkUtils::normalizeMac(mac);
        for (int i = 0; i < RELAY_SIZE; i++)
        {
            char switchIdent[12] = {};
            snprintf(switchIdent, sizeof(switchIdent), "switch:%d", i);
            if (!switchStatusJson.containsKey(switchIdent))
            {
                continue;
            }
            JsonObject switchObject = switchStatusJson[switchIdent].as<JsonObject>();
            if (!switchObject)
            {
                continue;
            }
            int idx;
            DataHub::WifiRelay entry;
            DataHub::getWifiRelayEntry(entry, idx, mac, i);
            if (entry.inUse)
            {
                entry.rssi = wifi["rssi"] | 0;
                bool changed = parseWiFiRelayEntry(switchObject, entry);
                if (!entry.online)
                {
                    entry.online = true;
                    changed = true;
                }
                entry.lastUpdate = millis();
                if (changed) entry.isDirty = true;
                DataHub::setWifiRelayEntry(entry, idx);
            }
        }
        return ParseResult::OK;
    }

    /**
     * @brief Parst einen einzelnen Relay-Block (switch:x) und füllt DataHub.
     *
     * @param entry Relay Eintrag, der gefüllt wird.
     * @param switchObject Json Entität, dass einen Eintrag repräsentiert.
     * @return ParseResult mit Status, modified=true wenn gesetzt.
     */
    bool parseWiFiRelayEntry(const JsonObject &switchObjectJson, DataHub::WifiRelay &entry)
    {
        bool modified = false;

        bool newOutput = switchObjectJson["output"] | false;
        if (entry.output != newOutput)
        {
            entry.output = newOutput;
            modified = true;
        }

        float newVoltage = switchObjectJson["voltage"] | 0.0f;
        if (fabs(entry.voltage - newVoltage)>0.01f)
        {
            entry.voltage = newVoltage;
            modified = true;
        }

        float newCurrent = switchObjectJson["current"] | 0.0f;
        if (fabs(entry.current - newCurrent)>0.01f)
        {
            entry.current = newCurrent;
            modified = true;
        }

        float newPower = switchObjectJson["apower"] | 0.0f;
        if (fabs(entry.power - newPower)>0.1f)
        {
            entry.power = newPower;
            modified = true;
        }

        float newAenergy = switchObjectJson["aenergy"]["total"] | 0.0f;
        if (fabs(entry.aenergy - newAenergy)>0.1f)
        {
            entry.aenergy = newAenergy;
            modified = true;
        }

        return modified;
    }
}

namespace JsonParser
{
    /** @note Nicht thread Safe. nur aus Wifi Worker Task aufrufen*/
    static StaticJsonDocument<2048> doc;

    /**
     * @brief Parst JSON aus einem Stream und übersetzt in DataHub-Objekte.
     *
     * @param stream Eingabe-Stream (z.B. WiFiClient oder File).
     * @param target Zieltyp, der gefüllt werden soll (z.B. WIFI_RELAY_ENTRY).
     * @return ParseResult Status + Index + Message / Flag.
     */
    ParseResult parseJson(Stream &stream, TargetType target)
    {

        doc.clear();
        DeserializationError err = deserializeJson(doc, stream);
        if (err)
        {
            Serial.println("Stream enthielt kein valides Json");
            return ParseResult::STREAM_ERROR;
        }
        JsonObject root = doc.as<JsonObject>();
        if (!root)
        {
            Serial.println("root im Json NULL");
            return ParseResult::INVALID_JSON;
        }
        if (target == TargetType::WIFI_RELAY_STATUS)
        {
            return parseWifiRelayStatus(root);
        }
        Serial.println("Kein Parsing Ziel");
        return ParseResult::NO_MATCHING_TARGET;
    }
}
