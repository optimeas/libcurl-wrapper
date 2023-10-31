#include "httpmockserver/httpmockserver.hpp"
#include "libcurl-wrapper/curlmultiasync.hpp"
#include "libcurl-wrapper/curlasynctransfer.hpp"
#include "cpp-utils/loggingstdout.hpp"

#include <fmt/core.h>
#include <gmock/gmock.h>

int port = 57567;
cu::Logger logger;

TEST(CurlMultiAsync, StartStop)
{
    curl::CurlMultiAsync curlMultiAsync(logger);
}

TEST(CurlMultiAsync, cancelAllTransfers)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/get-url";
    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseCode = 200;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlAsyncTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    transfer->setTransferCallback([&](curl::CurlAsyncTransfer *transfer)
    {
        EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CANCELED);
    });

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    curlMultiAsync.cancelAllTransfers();
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CANCELED);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);
}

TEST(CurlMultiAsync, cancelTransfer)
{
    curl::CurlMultiAsync curlMultiAsync(logger);

    std::string url = "/get-url";
    httpmock::HttpMockServer mockServer(port);
    mockServer.setGenerateResponseCallback([&](httpmock::ConnectionData *connectionData)
    {
        connectionData->responseCode = 200;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    });

    mockServer.start();
    EXPECT_TRUE(mockServer.isRunning());

    auto transfer = std::make_shared<curl::CurlAsyncTransfer>(logger);
    EXPECT_NE(transfer->curl().handle, nullptr);
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::NONE);

    std::string requestUrl = "http://127.0.0.1:" + std::to_string(port) + url;
    transfer->setUrl(requestUrl);

    transfer->setTransferCallback([&](curl::CurlAsyncTransfer *transfer)
    {
        EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CANCELED);
    });

    curlMultiAsync.performTransfer(transfer);
    EXPECT_TRUE(curlMultiAsync.waitForStarted(1000));
    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::RUNNING);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    curlMultiAsync.cancelTransfer(transfer);
    curlMultiAsync.waitForCompletion();

    EXPECT_EQ(transfer->asyncResult(), curl::AsyncResult::CANCELED);

    bool success = mockServer.waitForRequestCompleted(1, 1000);
    EXPECT_TRUE(success);
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
