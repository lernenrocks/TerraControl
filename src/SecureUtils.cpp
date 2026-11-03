#include "SecureUtils.h"

namespace SecureUtils
{
    void ensureZero(void *buffer, size_t len)
    {
        volatile unsigned char *p = static_cast<volatile unsigned char *>(buffer);
        while (len--)
        {
            *p++ = 0;
        }
    }
}
