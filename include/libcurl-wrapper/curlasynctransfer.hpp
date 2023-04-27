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
    void setSslVerfication(bool enable = true);

    void setTransferCallback(const TransferCallback &newTransferCallback);
    CURLcode curlResult() const;
    AsyncResult asyncResult() const;

    unsigned int progressTimeout_s() const;
    void setProgressTimeout_s(unsigned int newProgressTimeout_s);

    unsigned int maxTransferDuration_s() const;
    void setMaxTransferDuration_s(unsigned int newMaxTransferDuration_s);

    long responseCode() const;

    virtual void prepareTransfer() { }
    virtual void processResponse() { }

    // These two functions are called from CurlMultiAsync
    void _prepareTransfer();
    void _processResponse(AsyncResult asyncResult, CURLcode curlResult);

protected:
    static size_t staticOnProgressCallback(void *token, curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow);
    size_t onProgressCallback(curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow);

    cu::Logger m_logger;
    CurlHolder m_curl;
    CURLcode   m_curlResult{CURL_LAST}; // result from the curl transfer; only valid if AsyncResult == CURL_DONE
    AsyncResult m_asyncResult{NONE};    // state of the async operation
    TransferCallback m_transferCallback;
    std::chrono::steady_clock::time_point m_timepointTransferBegin;
    std::chrono::steady_clock::time_point m_timepointLastProgress;
    unsigned int m_progressTimeout_s{300};
    unsigned int m_maxTransferDuration_s{0};
    long m_responseCode{-1};
};

}
