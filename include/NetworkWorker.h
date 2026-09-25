#pragma once
namespace NetworkWorker
{
    enum class RelayRequestType
    {
        NONE,
        SWITCH_ON,
        SWITCH_OFF,
        GET_STAT,
        BIND,
        RESET
    };

    bool pushRelayRequest(RelayRequestType type, uint8_t relayId);

    bool createRelayMailbox(uint8_t relayId);
    void destroyRelayMailbox(uint8_t relayId);

    /**
     * @brief starts NetworkWorker, called in initWifi().
     */
    void start();
}
