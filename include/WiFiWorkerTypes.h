#pragma once
namespace WiFiWorker
{
    /**
     * @brief command types for a wifi Relay
     */
    enum class RelayCommand
    {
        NONE,       /** <@brief default command, do nothing */
        SWITCH_ON,  /** <@brief switch relay on */
        SWITCH_OFF, /** <@brief switch relay off */
        GET_STATUS  /** <@brief get status of a relay or relay hub */
    };

    /**
     * @brief command types for sta Mode
     */
    enum class StaCommand
    {
        NONE,            /** <@brief default command, do nothing */
        NTP_SYNC,        /** <@brief synchronize rtc time with ntp server */
        EXTERNAL_COMMAND /** <@brief placeholder for more external commands */
    };
    /**
     * @brief message for sta queue
     */
    struct StaMessage
    {
        StaCommand command;     /** <@brief sta command */
        char payload[256] = {}; /** <@brief optional payload */
    };

}