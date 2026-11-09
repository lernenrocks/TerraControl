/**
 * @file test_read_line.cpp
 * @brief Unit tests for HttpClient::readLine
 */
#include <gtest/gtest.h>
#include <string>
#include "HttpClient.h"

namespace
{
    // Plays a prepared response byte by byte; "connected" until everything is read,
    // so readLine() never waits for more data (millis() does not advance in the tests).
    class FakeStream : public WiFiClient
    {
    public:
        explicit FakeStream(const char *data) : _data(data) {}
        int available() override { return _pos < _data.size() ? 1 : 0; }
        int read() override { return _pos < _data.size() ? _data[_pos++] : -1; }
        int connected() override { return _pos < _data.size() ? 1 : 0; }

    private:
        std::string _data;
        size_t _pos = 0;
    };
}

TEST(ReadLine, CrLfStripped)
{
    FakeStream stream("Hello\r\n");
    char buf[32];
    size_t len = HttpClient::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "Hello");
    EXPECT_EQ(len, 5u);
}

TEST(ReadLine, EmptyLine)
{
    FakeStream stream("\r\n");
    char buf[32];
    size_t len = HttpClient::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "");
    EXPECT_EQ(len, 0u);
}

// A full buffer ends the line early - the rest stays in the stream and comes with the next call.
TEST(ReadLine, BufferFull)
{
    FakeStream stream("ABCDE\r\n");
    char buf[4]; // room for 3 characters + \0
    size_t len = HttpClient::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "ABC");
    EXPECT_EQ(len, 3u);
    len = HttpClient::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "DE");
    EXPECT_EQ(len, 2u);
}

TEST(ReadLine, TwoLinesReadOneByOne)
{
    FakeStream stream("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n");
    char buf[64];
    HttpClient::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "HTTP/1.1 200 OK");
    HttpClient::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "Content-Length: 2");
}

// Connection closed without a final \n: the line read so far is returned.
TEST(ReadLine, EndOfStreamWithoutNewline)
{
    FakeStream stream("partial");
    char buf[32];
    size_t len = HttpClient::readLine(stream, buf, sizeof(buf));
    EXPECT_STREQ(buf, "partial");
    EXPECT_EQ(len, 7u);
}
