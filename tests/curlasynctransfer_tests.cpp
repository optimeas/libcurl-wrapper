#include "httpmockserver/httpmockserver.hpp"
#include "libcurl-wrapper/curlmultiasync.hpp"
#include "libcurl-wrapper/curlasynctransfer.hpp"
#include "cpp-utils/loggingstdout.hpp"

#include <fmt/core.h>
#include <gmock/gmock.h>

extern int port;
extern cu::Logger logger;

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

    auto transfer = std::make_shared<curl::CurlAsyncTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    curlMultiAsync.performTransfer(transfer);
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

    std::string url = "/get-url";
    std::string content;

    content += R"(<?xml version="1.0" encoding="UTF-8"?>\n)";
    content += R"(<root>\n)";
    content += R"(    <attention text="File=/sde/preview/20220629/20220629_115750.osfz Tag=preview"/>\n)";
    content += R"(</root>)";
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

    auto transfer = std::make_shared<curl::CurlAsyncTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);
    transfer->setHeader("content-description", "Message-ID: 4711");
    transfer->setHeader("content-description", "Message-ID: 4712"); // test overwrite with different value
    transfer->setHeader("Content-Type", "application/json");
    curl_easy_setopt(transfer->curl().handle, CURLOPT_POSTFIELDS, content.c_str());

    curlMultiAsync.performTransfer(transfer);
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
        EXPECT_TRUE(mockServer.lastConnectionData()->header["Content-Description"]  == "Message-ID: 4712");
        EXPECT_TRUE(mockServer.lastConnectionData()->header["Content-Type"]         == "application/json");
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

    auto transfer = std::make_shared<curl::CurlAsyncTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    std::string filename = tmpnam(nullptr); // cppcheck-suppress tmpnamCalled
    transfer->setOutputFilename(filename);

    curlMultiAsync.performTransfer(transfer);
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

    auto transfer = std::make_shared<curl::CurlAsyncTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    transfer->setProgressTimeout_s(1);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    transfer->setTransferCallback([&](curl::CurlAsyncTransfer *transfer)
    {
        EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::TIMEOUT);
    });

    curlMultiAsync.performTransfer(transfer);
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

    auto transfer = std::make_shared<curl::CurlAsyncTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    transfer->setMaxTransferDuration_s(1);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    transfer->setTransferCallback([&](curl::CurlAsyncTransfer *transfer)
    {
        EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::TIMEOUT);
    });

    curlMultiAsync.performTransfer(transfer);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::TIMEOUT);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);
}
