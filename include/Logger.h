#pragma once
#include <Arduino.h>

namespace Logger
{
    enum class ErrorLevel
    {
        MESSAGE, /** @brief log a Message */
        WARN, /** @brief low concern for logfile */
        ERROR /** @brief high concern for logfile and console */
    };

    void log(ErrorLevel err, const char* msg);
}
