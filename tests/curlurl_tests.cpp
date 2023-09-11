#include "libcurl-wrapper/curlurl.hpp"
#include "cpp-utils/loggingstdout.hpp"

#include <curl/curl.h>
#include <fmt/core.h>
#include <gmock/gmock.h>

cu::Logger logger;

TEST(Url, EmptyInvalid)
{
    curl::Url url;
    EXPECT_FALSE(url.isValid());
}

TEST(Url, ImplicitConversion)
{
    curl::Url url1 = "https://domain.de/index.html";
    EXPECT_TRUE(url1.isValid());
    EXPECT_EQ(url1.toString() , "https://domain.de/index.html");

    curl::Url url2 = std::string("https://domain.de/index.html");
    EXPECT_TRUE(url1.isValid());
    EXPECT_EQ(url2.toString() , "https://domain.de/index.html");
}

TEST(Url, MalformedUrls)
{
    curl::Url url1 = "";
    EXPECT_FALSE(url1.isValid());

    curl::Url url2 = "http://www.exam ple.com";
    EXPECT_FALSE(url2.isValid());
}

TEST(Url, AllowMissingScheme)
{
    curl::Url url1 = "optimeas.de";
    EXPECT_FALSE(url1.isValid());

    curl::Url url2("optimeas.de", true);
    EXPECT_TRUE(url2.isValid());
    EXPECT_EQ(url2.toString() , "https://optimeas.de/");
}

TEST(Url, SetPathDomain)
{
    curl::Url url("domain.de", true);
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.setPath("/index.html"));
    EXPECT_EQ(url.toString() , "https://domain.de/index.html");
}

TEST(Url, SetPathIPv4)
{
    curl::Url url("http://192.168.101.1:8080/heartbeat.php");
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.setPath("/index.html", false));
    EXPECT_EQ(url.toString() , "http://192.168.101.1:8080/heartbeat.php");
}

TEST(Url, SetPathIPv6)
{
    curl::Url url1("http://[fd00:a41::50]");
    EXPECT_TRUE(url1.isValid());
    EXPECT_TRUE(url1.setPath("/index.html"));
    EXPECT_EQ(url1.toString() , "http://[fd00:a41::50]/index.html");

    curl::Url url2("[2a00:1450:4001:831::2003]:1234", true);
    EXPECT_TRUE(url2.isValid());
    EXPECT_TRUE(url2.setPath("/index.html"));
    EXPECT_EQ(url2.toString() , "https://[2a00:1450:4001:831::2003]:1234/index.html");

    curl::Url url3("http://[fe80::3ad1:35ff:fe08:cd%eth0]:80/");
    EXPECT_TRUE(url3.isValid());
    EXPECT_TRUE(url3.setPath("/index.html"));
    EXPECT_EQ(url3.toString() , "http://[fe80::3ad1:35ff:fe08:cd%25eth0]:80/index.html");
}

TEST(Url, SetPageNoSlashNoPage)
{
    curl::Url url("http://192.168.101.1:8080");
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.setPage("/index.html", false));
    EXPECT_EQ(url.toString() , "http://192.168.101.1:8080/index.html");
}

TEST(Url, SetPageOnlySlash)
{
    curl::Url url("http://192.168.101.1:8080/folder/");
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.setPage("/index.html", false));
    EXPECT_EQ(url.toString() , "http://192.168.101.1:8080/folder/index.html");
}

TEST(Url, SetPagePageOrFolder)
{
    curl::Url url("http://192.168.101.1:8080/page_or_folder");
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.setPage("/index.html", false));
    EXPECT_EQ(url.toString() , "http://192.168.101.1:8080/page_or_folder/index.html");
}

TEST(Url, SetPagePageWithoutOverwrite)
{
    curl::Url url("http://192.168.101.1:8080/folder/page.html");
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.setPage("/index.html", false));
    EXPECT_EQ(url.toString() , "http://192.168.101.1:8080/folder/page.html");
}

TEST(Url, SetPagePageWithOverwrite)
{
    curl::Url url("http://192.168.101.1:8080/folder/page.html");
    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.setPage("/index.html", true));
    EXPECT_EQ(url.toString() , "http://192.168.101.1:8080/folder/index.html");
}

int main(int argc, char *argv[])
{
    logger = std::make_shared<cu::StandardOutputLogger>();
    //    logger = std::make_shared<cu::NullLogger>();
    curl_global_init(CURL_GLOBAL_ALL);

    ::testing::InitGoogleTest(&argc, argv);
    int returnValue = RUN_ALL_TESTS();

    curl_global_cleanup();
    return returnValue;
}
