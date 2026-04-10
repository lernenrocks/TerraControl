#include <gtest/gtest.h>
#include "../../src/HttpUtils.cpp"

TEST(NormalizeMac, UpperToLower){
    char mac[] = "8CBFEAA03350";
    HttpUtils::normalizeMac(mac);
    EXPECT_STREQ(mac,"8cbfeaa03350");
}
TEST(NormalizeMac, AlreadyLower) {
    char mac[] = "8cbfeaa03350";
    HttpUtils::normalizeMac(mac);
    EXPECT_STREQ(mac, "8cbfeaa03350");
}
TEST(NormalizeMac, EmptyString) {
    char mac[] = "";
    HttpUtils::normalizeMac(mac);
    EXPECT_STREQ(mac, "");
}
TEST(NormalizeMac, ToShort) {
    char mac[] = "8CBFEAA";
    HttpUtils::normalizeMac(mac);
    EXPECT_STREQ(mac, "8cbfeaa");
}
TEST(NormalizeMac, ToLong) {
    char mac[] = "8CBFEAA03350EB24";
    HttpUtils::normalizeMac(mac);
    EXPECT_STREQ(mac,"8cbfeaa03350eb24");
}