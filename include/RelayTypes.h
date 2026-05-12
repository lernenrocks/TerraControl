#pragma once

namespace RelayTypes
{
    /**
     * @brief relay mode for switch handling
     */
    enum class RelayMode{
        FORCED_OFF, /** <@brief switch is always off, should be default*/
        FORCED_ON, /** <@brief  switch is always on*/
        AUTO /** <@brief switch on or off depends on rules */
    };
} // namespace RelayTypes
