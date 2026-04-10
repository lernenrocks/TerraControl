/**
 * @file main.cpp
 * @brief Einstiegspunkt für die nativen Google Test Unit Tests.
 *
 * Wird von PlatformIO im [env:native] Build verwendet.
 * Registriert und führt alle TEST()-Blöcke aus.
 */

#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
