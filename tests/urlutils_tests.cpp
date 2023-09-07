#include "libcurl-wrapper/urlutils.hpp"
#include "cpp-utils/loggingstdout.hpp"

#include <curl/curl.h>
#include <fmt/core.h>
#include <gmock/gmock.h>

extern cu::Logger logger;

TEST(UrlValidate, MalformedUrls)
{
    std::string url;

    url = "";
    EXPECT_FALSE(curl::urlValidate(url));

    url = "http://www.exam ple.com";
    EXPECT_FALSE(curl::urlValidate(url));
}

TEST(UrlValidate, AllowMissingScheme)
{
    std::string url;

    url = "optimeas.de";
    EXPECT_FALSE(curl::urlValidate(url));
    EXPECT_TRUE(curl::urlValidate(url, true));
}

TEST(UrlReplacePath, MalformedUrls)
{
    std::string url;

    url = "";
    EXPECT_FALSE(curl::urlReplacePath(url, "/index.html"));

    url = "http://www.exam ple.com";
    EXPECT_FALSE(curl::urlReplacePath(url, "/index.html"));
}

TEST(UrlReplacePath, OnlyHostname)
{
    std::string url = "domain.de";
    EXPECT_TRUE(curl::urlReplacePath(url, "/index.html"));
    EXPECT_EQ(url, "https://domain.de/index.html");
}

TEST(UrlReplacePath, IPv4)
{
    std::string url = "http://192.168.101.1:8080/heartbeat.php";
    EXPECT_TRUE(curl::urlReplacePath(url, "/index.html", false));
    EXPECT_EQ(url, "http://192.168.101.1:8080/heartbeat.php");
}

TEST(UrlReplacePath, IPv6)
{
    std::string url;

    url = "http://[fd00:a41::50]";
    EXPECT_TRUE(curl::urlReplacePath(url, "/index.html"));
    EXPECT_EQ(url, "http://[fd00:a41::50]/index.html");

    url = "[2a00:1450:4001:831::2003]:1234";
    EXPECT_TRUE(curl::urlReplacePath(url, "/index.html"));
    EXPECT_EQ(url, "https://[2a00:1450:4001:831::2003]:1234/index.html");

    url = "http://[fe80::3ad1:35ff:fe08:cd%eth0]:80/";
    EXPECT_TRUE(curl::urlReplacePath(url, "/index.html"));
    EXPECT_EQ(url, "http://[fe80::3ad1:35ff:fe08:cd%25eth0]:80/index.html");
}
