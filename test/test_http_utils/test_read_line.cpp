/**
 * @file test_read_line.cpp
 * @brief Unit Tests für HttpUtils::readLine
 * @author Omnissiah Claude (Anthropic)
 */
#include <gtest/gtest.h>
#include <string>
#include "HttpUtils.h"

class FakeStream : public WiFiClient
{
    std::string _data;
    size_t _pos = 0;

public:
    explicit FakeStream(const char *data) : _data(data) {}
    int available() override { return _pos < _data.size() ? 1 : 0; }
    int read() override { return _pos < _data.size() ? _data[_pos++] : -1; }
    int connected() override { return 1; }
};

TEST(ReadLine, CrLfStripped)
{
    FakeStream stream("Hello\r\n");
    char buf[32];
    size_t len = HttpUtils::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "Hello");
    EXPECT_EQ(len, 5u);
}

TEST(ReadLine, EmptyLine)
{
    FakeStream stream("\r\n");
    char buf[32];
    size_t len = HttpUtils::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "");
    EXPECT_EQ(len, 0u);
}

TEST(ReadLine, BufferFull)
{
    FakeStream stream("ABCDE\r\n");
    char buf[4]; // Platz für 3 Zeichen + \0
    size_t len = HttpUtils::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "ABC");
    EXPECT_EQ(len, 3u);
}
