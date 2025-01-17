#include "httpmockserver/httpmockserver.hpp"
#include "libcurl-wrapper/curlmultiasync.hpp"
#include "libcurl-wrapper/curlhttptransfer.hpp"
#include "cpp-utils/loggingstdout.hpp"

#include <fmt/core.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <unistd.h>

extern int port;
extern cu::Logger logger;

// avoid linker warnings like: the use of `tmpnam' is dangerous, better use `mkstemp'
std::string generateUniqueTemporaryFilename(void)
{
    std::string executableName;
    std::ifstream("/proc/self/comm") >> executableName;
    executableName += "XXXXXX";

    int fd = mkstemp(executableName.data());

    std::string symlink = fmt::format("/proc/{}/fd/{}", getpid(), fd);
    std::filesystem::path symlinkPath{symlink};

    std::string filename = std::filesystem::read_symlink(symlinkPath);

    close(fd);
    std::remove(filename.c_str());

    return filename;
}

TEST(CurlAsyncTransfer, SslError)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    auto transfer = std::make_shared<curl::CurlHttpTransfer>(logger);
    transfer->setUrl("https://expired.badssl.com/");
    transfer->setVerifySslCertificates(false);

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CURL_DONE);
    EXPECT_EQ(transfer->curlResult(), CURLE_OK) << curl_easy_strerror(transfer->curlResult());

    EXPECT_EQ(transfer->responseCode(), 200);
    EXPECT_TRUE(transfer->responseHeader("Content-Type") == "text/html");
}

TEST(CurlAsyncTransfer, Get)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/get-url";
    std::string response = "<html><body>HttpMockServer</body></html>";

    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseHeader["Content-Description"] = "Message-ID: 152018";
        connectionData->responseHeader["Content-Type"] = "application/om-scpi-app";
        connectionData->responseBody = response;
        connectionData->responseCode = 200;
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlHttpTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CURL_DONE);
    EXPECT_EQ(transfer->curlResult(), CURLE_OK) << curl_easy_strerror(transfer->curlResult());
    EXPECT_EQ(transfer->responseCode(), 200);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);

    EXPECT_TRUE(transfer->responseHeader("Content-Description") == "Message-ID: 152018");
    EXPECT_TRUE(transfer->responseHeader("Content-Type")        == "application/om-scpi-app");
    EXPECT_TRUE(std::string_view(transfer->responseData().data(), transfer->responseData().size()) == response);

    if(success) // otherwise we dereference a null pointer
    {
        EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
        EXPECT_EQ(  mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::Get);
    }
}

TEST(CurlAsyncTransfer, Post)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/post-url";
    std::string content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<root>\r\n    <attention text=\"File=/sde/preview/20220629/20220629_115750.osfz Tag=preview\"/>\r\n</root>\r\n";
    std::string response = "<html><body>HttpMockServer</body></html>";

    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseHeader["Content-Description"] = "Message-ID: 152018";
        connectionData->responseHeader["Content-Type"] = "application/om-scpi-app";
        connectionData->responseBody = response;
        connectionData->responseCode = 200;
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlHttpTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);
    transfer->setHeader("content-description", "Message-ID: 4711");
    transfer->setHeader("content-description", "Message-ID: 4712"); // test overwrite with different value
    transfer->setHeader("Content-Type", "application/json");
    transfer->setPostData(content.c_str());

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CURL_DONE);
    EXPECT_EQ(transfer->curlResult(), CURLE_OK) << curl_easy_strerror(transfer->curlResult());
    EXPECT_EQ(transfer->responseCode(), 200);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);

    EXPECT_TRUE(transfer->responseHeader("Content-Description") == "Message-ID: 152018");
    EXPECT_TRUE(transfer->responseHeader("Content-Type")        == "application/om-scpi-app");
    EXPECT_TRUE(std::string_view(transfer->responseData().data(), transfer->responseData().size()) == response);

    if(success) // otherwise we dereference a null pointer
    {
        EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
        EXPECT_EQ(  mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::PostRawData);
        EXPECT_EQ(content.size(), mockServer.lastConnectionData()->postData.size());
        EXPECT_EQ(std::memcmp(mockServer.lastConnectionData()->postData.data(), content.c_str(), content.size()), 0);
        EXPECT_TRUE(mockServer.lastConnectionData()->header["Content-Description"]  == "Message-ID: 4712");
        EXPECT_TRUE(mockServer.lastConnectionData()->header["Content-Type"]         == "application/json");
    }
}

TEST(CurlAsyncTransfer, PostCopyData)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/post-url";
    std::string content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<root>\r\n    <attention text=\"File=/sde/preview/20220629/20220629_115750.osfz Tag=preview\"/>\r\n</root>\r\n";
    std::string response = "<html><body>HttpMockServer</body></html>";

    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseHeader["Content-Description"] = "Message-ID: 152018";
        connectionData->responseHeader["Content-Type"] = "application/om-scpi-app";
        connectionData->responseBody = response;
        connectionData->responseCode = 200;
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlHttpTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);
    transfer->setHeader("content-description", "Message-ID: 4711");
    transfer->setHeader("content-description", "Message-ID: 4712"); // test overwrite with different value
    transfer->setHeader("Content-Type", "application/json");
    transfer->setPostData(content.c_str(), content.size(), true);

    std::string contentCopy = content;
    std::fill(content.begin(), content.end(), '#');
    content.clear();

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CURL_DONE);
    EXPECT_EQ(transfer->curlResult(), CURLE_OK) << curl_easy_strerror(transfer->curlResult());
    EXPECT_EQ(transfer->responseCode(), 200);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);

    EXPECT_TRUE(transfer->responseHeader("Content-Description") == "Message-ID: 152018");
    EXPECT_TRUE(transfer->responseHeader("Content-Type")        == "application/om-scpi-app");
    EXPECT_TRUE(std::string_view(transfer->responseData().data(), transfer->responseData().size()) == response);

    if(success) // otherwise we dereference a null pointer
    {
        EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
        EXPECT_EQ(  mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::PostRawData);
        EXPECT_EQ(contentCopy.size(), mockServer.lastConnectionData()->postData.size());
        EXPECT_EQ(std::memcmp(mockServer.lastConnectionData()->postData.data(), contentCopy.c_str(), contentCopy.size()), 0);
        EXPECT_TRUE(mockServer.lastConnectionData()->header["Content-Description"]  == "Message-ID: 4712");
        EXPECT_TRUE(mockServer.lastConnectionData()->header["Content-Type"]         == "application/json");
    }
}

TEST(CurlAsyncTransfer, PostFile)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/post-file-url";
    std::string response = "<html><body>HttpMockServer</body></html>";

    int len = 1024 * 1204 * 2;
    std::string content;
    content.reserve(len);
    static const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for(int i = 0; i < len; ++i)
        content += alphabet[rand() % (sizeof(alphabet) - 1)];
    EXPECT_EQ(content.size(), len);

    std::string filename = generateUniqueTemporaryFilename();
    std::ofstream outfile (filename, std::ofstream::binary);
    outfile.write (content.data(), content.size());
    outfile.close();

    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseHeader["Content-Description"] = "Message-ID: 152018";
        connectionData->responseHeader["Content-Type"] = "application/om-scpi-app";
        connectionData->responseBody = response;
        connectionData->responseCode = 200;
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlHttpTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    transfer->setUploadFilename(filename);
    transfer->setHeader("Content-Type", "application/bin");

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CURL_DONE);
    EXPECT_EQ(transfer->curlResult(), CURLE_OK) << curl_easy_strerror(transfer->curlResult());
    EXPECT_EQ(transfer->responseCode(), 200);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);

    EXPECT_TRUE(transfer->responseHeader("Content-Description") == "Message-ID: 152018");
    EXPECT_TRUE(transfer->responseHeader("Content-Type")        == "application/om-scpi-app");
    EXPECT_TRUE(std::string_view(transfer->responseData().data(), transfer->responseData().size()) == response);
    std::remove(filename.c_str());

    if(success) // otherwise we dereference a null pointer
    {
        EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
        EXPECT_EQ(  mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::PostRawData);
        EXPECT_EQ(content.size(), mockServer.lastConnectionData()->postData.size());
        EXPECT_EQ(std::memcmp(mockServer.lastConnectionData()->postData.data(), content.c_str(), content.size()), 0);
    }
}

TEST(CurlAsyncTransfer, OutputToFile)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/get-url";
    std::string response = "<html><body>HttpMockServer</body></html>";

    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseBody = response;
        connectionData->responseCode = 200;
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlHttpTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    std::string filename = generateUniqueTemporaryFilename();
    transfer->setOutputFilename(filename);

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CURL_DONE);
    EXPECT_EQ(transfer->curlResult(), CURLE_OK) << curl_easy_strerror(transfer->curlResult());
    EXPECT_EQ(transfer->responseCode(), 200);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);

    std::ifstream fileStream(filename);
    std::string responseFileData((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(responseFileData == response);
    std::remove(filename.c_str());

    if(success) // otherwise we dereference a null pointer
    {
        EXPECT_TRUE(mockServer.lastConnectionData()->url == url);
        EXPECT_EQ(  mockServer.lastConnectionData()->httpMethod, httpmock::HttpMethod::Get);
    }
}

TEST(CurlAsyncTransfer, ProgressTimeout)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/get-url";
    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseCode = 200;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlHttpTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    transfer->setProgressTimeout_s(1);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    transfer->setTransferCallback([&](const curl::CurlAsyncTransfer *transfer)
    {
        EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::TIMEOUT);
    });

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::TIMEOUT);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);
}

TEST(CurlAsyncTransfer, MaxTransferDuration)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/get-url";
    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseCode = 200;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlHttpTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    transfer->setMaxTransferDuration_s(1);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    transfer->setTransferCallback([&](const curl::CurlAsyncTransfer *transfer)
    {
        EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::TIMEOUT);
    });

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::TIMEOUT);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);
}
