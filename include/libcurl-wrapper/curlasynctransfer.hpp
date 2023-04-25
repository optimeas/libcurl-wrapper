#pragma once

#include "cpp-utils/logging.hpp"

#include "curlholder.hpp"

#include <functional>
#include <chrono>
#include<fstream>

namespace curl
{

enum AsyncResult
{
    NONE,
    RUNNING,
    CURL_DONE,
    CANCELED,
    TIMEOUT
};

class CurlAsyncTransfer;
using TransferCallback = std::function<void (CurlAsyncTransfer *transfer)>;

class CurlAsyncTransfer
{
public:
    explicit CurlAsyncTransfer(const cu::Logger& logger);
    virtual ~CurlAsyncTransfer() = default; // Prevent undefined behavior when used as base class and delete per base class pointer

    const CurlHolder &curl() const;

    void setUrl(const std::string &url);
    void setTransferCallback(const TransferCallback &newTransferCallback);
    CURLcode curlResult() const;
    AsyncResult asyncResult() const;

    unsigned int progressTimeout_s() const;
    void setProgressTimeout_s(unsigned int newProgressTimeout_s);

    unsigned int maxTransferDuration_s() const;
    void setMaxTransferDuration_s(unsigned int newMaxTransferDuration_s);

    long responseCode() const;
    std::string responseHeader(std::string headerName);
    std::unordered_map<std::string, std::string> &responseHeaders();
    std::vector<char> &responseData();
    void setOutputFilename(const std::string& fileNameWithPath);

    void setHeader(const std::string &name, const std::string &content);
    void clearHeaders();

    // These two functions are called from CurlMultiAsync
    virtual void prepareTransfer();
    virtual void processResponse(AsyncResult asyncResult, CURLcode curlResult);

private:
    static size_t staticOnProgressCallback(void *token, curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow);
    size_t onProgressCallback(curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow);
    static size_t staticOnWriteCallback(char *ptr, size_t size, size_t nmemb, void *token);
    void onWriteCallback(const char *ptr, size_t realsize);
    static size_t staticOnHeaderCallback(char *buffer, size_t size, size_t nitems, void *token);
    void onHeaderCallback(const char *buffer, size_t realsize);

    cu::Logger m_logger;
    CurlHolder m_curl;
    CURLcode   m_curlResult{CURL_LAST}; // result from the curl transfer; only valid if AsyncResult == CURL_DONE
    AsyncResult m_asyncResult{NONE};    // state of the async operation
    TransferCallback m_transferCallback;
    std::chrono::steady_clock::time_point m_timepointTransferBegin;
    std::chrono::steady_clock::time_point m_timepointLastProgress;
    unsigned int m_progressTimeout_s;
    unsigned int m_maxTransferDuration_s;
    std::unordered_map<std::string, std::string> m_responseHeaders;
    std::vector<char> m_responseData;
    long m_responseCode{-1};
    std::string m_outputFileName;
    std::ofstream m_outputFile;
    std::unordered_map<std::string, std::string> m_requestHeaders;
};

}
