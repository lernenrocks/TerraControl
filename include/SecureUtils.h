#pragma once
#include <stddef.h>

namespace SecureUtils
{
    /**
     * @brief Overwrites a buffer with zeros in a way the compiler cannot optimize away
     *        (write access via a volatile pointer), unlike a plain memset() on a
     *        buffer that goes out of scope right after.
     * @param buffer buffer to clear
     * @param len number of bytes to clear
     */
    void ensureZero(void *buffer, size_t len);
}
