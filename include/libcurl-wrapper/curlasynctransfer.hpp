#pragma once

#include "cpp-utils/logging.hpp"

#include "curlholder.hpp"
#include "tracing.hpp"

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
    void setVerifySslCertificates(bool doVerifySslCertificates = true);
    void setReuseExistingConnection(bool doReuseExistingConnection = true);

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

    void setTracing(std::unique_ptr<TracingInterface> newTracing);

    float transferDuration_s() const;
    uint64_t transferSpeed_BytesPerSecond() const;
    uint64_t transferredBytes() const;

    void setProgressLogging_s(unsigned int newProgressLogging_s);
    void setLogPrefix(const std::string &newLogPrefix);

protected:
    static size_t staticOnProgressCallback(void *token, curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow);
    size_t onProgressCallback(curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow);
    static int staticOnDebugCallback(CURL *handle, curl_infotype type, char *data, size_t size, void *token);
    int onDebugCallback(CURL *handle, curl_infotype type, char *data, size_t size);

    cu::Logger m_logger;
    CurlHolder m_curl;
    CURLcode   m_curlResult{CURL_LAST}; // result from the curl transfer; only valid if AsyncResult == CURL_DONE
    AsyncResult m_asyncResult{NONE};    // state of the async operation
    TransferCallback m_transferCallback;
    std::chrono::steady_clock::time_point m_timepointTransferBegin;
    std::chrono::steady_clock::time_point m_timepointLastProgress;
    std::chrono::steady_clock::time_point m_timepointLastProgressLogEntry;
    unsigned int m_progressTimeout_s{300};
    unsigned int m_progressLogging_s{0};
    unsigned int m_maxTransferDuration_s{0};
    long m_responseCode{-1};
    std::unique_ptr<TracingInterface> m_tracing;
    float m_transferDuration_s{0.0};
    uint64_t m_transferSpeed_BytesPerSecond{0};
    uint64_t m_transferredBytes{0};
    std::string m_logPrefix;
};

}
