#include <gtest/gtest.h>
#include "../../src/NetworkUtils.cpp"

TEST(NormalizeMac, UpperToLower){
    char mac[] = "8CBFEAA03350";
    NetworkUtils::normalizeMac(mac);
    EXPECT_STREQ(mac,"8cbfeaa03350");
}
TEST(NormalizeMac, AlreadyLower) {
    char mac[] = "8cbfeaa03350";
    NetworkUtils::normalizeMac(mac);
    EXPECT_STREQ(mac, "8cbfeaa03350");
}
TEST(NormalizeMac, EmptyString) {
    char mac[] = "";
    NetworkUtils::normalizeMac(mac);
    EXPECT_STREQ(mac, "");
}
TEST(NormalizeMac, ToShort) {
    char mac[] = "8CBFEAA";
    NetworkUtils::normalizeMac(mac);
    EXPECT_STREQ(mac, "8cbfeaa");
}
TEST(NormalizeMac, ToLong) {
    char mac[] = "8CBFEAA03350EB24";
    NetworkUtils::normalizeMac(mac);
    EXPECT_STREQ(mac,"8cbfeaa03350eb24");
}