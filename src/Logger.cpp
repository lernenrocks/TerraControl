#include "Logger.h"

namespace Logger
{
    void log(ErrorLevel err, const char *msg)
    {
        char errPrefix[16] = {};
        switch (err)
        {
        case ErrorLevel::MESSAGE:
            strncpy(errPrefix, "[MESSAGE]", sizeof(errPrefix));
            break;
        case ErrorLevel::WARN:
            strncpy(errPrefix, "[WARN]", sizeof(errPrefix));
            break;
        case ErrorLevel::ERROR:
            strncpy(errPrefix, "[ERROR]", sizeof(errPrefix));
            break;
        }
        Serial.printf("%s %s\n",errPrefix,msg);
    }
}