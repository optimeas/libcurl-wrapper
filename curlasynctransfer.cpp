#include "libcurl-wrapper/curlasynctransfer.hpp"
#include "cpp-utils/timeutils.hpp"

#include <fmt/core.h>

namespace curl
{

CurlAsyncTransfer::CurlAsyncTransfer(const cu::Logger &logger)
    : m_logger(logger)
{
}

const CurlHolder &CurlAsyncTransfer::curl() const
{
    return m_curl;
}

void CurlAsyncTransfer::setUrl(const std::string &url)
{
    curl_easy_setopt(m_curl.handle, CURLOPT_URL, url.c_str());
}

void CurlAsyncTransfer::setVerifySslCertificates(bool doVerifySslCertificates)
{
    if(doVerifySslCertificates)
    {
        curl_easy_setopt(m_curl.handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(m_curl.handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    else
    {
        curl_easy_setopt(m_curl.handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(m_curl.handle, CURLOPT_SSL_VERIFYHOST, 0L);

    }
}

void CurlAsyncTransfer::setReuseExistingConnection(bool doReuseExistingConnection)
{
    if(doReuseExistingConnection)
        curl_easy_setopt(m_curl.handle, CURLOPT_FORBID_REUSE, 0L);
    else
        curl_easy_setopt(m_curl.handle, CURLOPT_FORBID_REUSE, 1L);
}

void CurlAsyncTransfer::setTransferCallback(const TransferCallback &newTransferCallback)
{
    m_transferCallback = newTransferCallback;
}

CURLcode CurlAsyncTransfer::curlResult() const
{
    return m_curlResult;
}

AsyncResult CurlAsyncTransfer::asyncResult() const
{
    return m_asyncResult;
}

unsigned int CurlAsyncTransfer::maxTransferDuration_s() const
{
    return m_maxTransferDuration_s;
}

void CurlAsyncTransfer::setMaxTransferDuration_s(unsigned int newMaxTransferDuration_s)
{
    m_maxTransferDuration_s = newMaxTransferDuration_s;
}

long CurlAsyncTransfer::responseCode() const
{
    return m_responseCode;
}

unsigned int CurlAsyncTransfer::progressTimeout_s() const
{
    return m_progressTimeout_s;
}

void CurlAsyncTransfer::setProgressTimeout_s(unsigned int newProgressTimeout_s)
{
    m_progressTimeout_s = newProgressTimeout_s;
}

void CurlAsyncTransfer::_prepareTransfer()
{
    if(m_asyncResult == RUNNING)
    {
        std::string errMsg = "transfer is already running";
        m_logger->error(errMsg);
        throw std::runtime_error(errMsg);
    }

    curl_easy_setopt(m_curl.handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl.handle, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl.handle, CURLOPT_XFERINFOFUNCTION, &staticOnProgressCallback);

    if(m_tracing)
    {
        curl_easy_setopt(m_curl.handle, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(m_curl.handle, CURLOPT_DEBUGDATA, this);
        curl_easy_setopt(m_curl.handle, CURLOPT_DEBUGFUNCTION, &staticOnDebugCallback);
    }
    else
        curl_easy_setopt(m_curl.handle, CURLOPT_VERBOSE, 0L);

    if(m_tracing)
    {
        m_tracing->initialize();
        m_tracing->traceMessage(fmt::format("### Timestamp: {} ###\n", cu::getCurrentTimestampUtcIso8601(true)));
    }

    prepareTransfer();

    m_transferDuration_s = 0.0;
    m_transferredBytes = 0;
    m_transferSpeed_BytesPerSecond = 0;

    m_responseCode = -1;
    m_asyncResult = RUNNING;
    m_timepointTransferBegin = std::chrono::steady_clock::now();
    m_timepointLastProgressLogEntry = m_timepointTransferBegin;
    m_timepointLastProgress = m_timepointTransferBegin;
}

void CurlAsyncTransfer::_processResponse(AsyncResult asyncResult, CURLcode curlResult)
{
    m_curlResult = curlResult;

    if(m_asyncResult != TIMEOUT) // timeout is handled in CurlAsyncTransfer, everything else in CurlMultiAsync
        m_asyncResult = asyncResult;

    // auto end = std::chrono::steady_clock::now();
    // m_logger->debug(fmt::format("transfer took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(end - m_timepointTransferBegin).count()));

    if(curlResult == CURLE_OK)
    {
        if(curl_easy_getinfo(m_curl.handle, CURLINFO_RESPONSE_CODE, &m_responseCode) != CURLE_OK)
            m_responseCode = -1;
    }

    curl_off_t uploadSpeed, downloadSpeed, uploadSize, downloadSize;
    curl_easy_getinfo(m_curl.handle, CURLINFO_SPEED_UPLOAD_T,   &uploadSpeed);
    curl_easy_getinfo(m_curl.handle, CURLINFO_SPEED_DOWNLOAD_T, &downloadSpeed);
    curl_easy_getinfo(m_curl.handle, CURLINFO_SIZE_UPLOAD_T,    &uploadSize);
    curl_easy_getinfo(m_curl.handle, CURLINFO_SIZE_DOWNLOAD_T,  &downloadSize);
    m_transferredBytes += downloadSize + uploadSize;
    m_transferSpeed_BytesPerSecond = std::max(uploadSpeed, downloadSpeed);

    if(m_tracing)
    {
        m_tracing->finalize();
        m_tracing.reset(); // do not overwrite the trace file if the transfer object is reused
    }

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> diff = now - m_timepointTransferBegin;
    m_transferDuration_s = diff.count();

    processResponse();

    if(m_transferCallback)
        m_transferCallback(this);
}

size_t CurlAsyncTransfer::staticOnProgressCallback(void *token, curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal, curl_off_t uploadNow)
{
    if(token != nullptr)
        return static_cast<CurlAsyncTransfer*>(token)->onProgressCallback(downloadTotal, downloadNow, uploadTotal, uploadNow);
    else
        return -1;
}

size_t CurlAsyncTransfer::onProgressCallback([[maybe_unused]] curl_off_t downloadTotal, curl_off_t downloadNow, [[maybe_unused]] curl_off_t uploadTotal, curl_off_t uploadNow)
{
    auto now = std::chrono::steady_clock::now();

    if((downloadNow != 0) || (uploadNow != 0))
        m_timepointLastProgress = now;

    if(m_progressLogging_s > 0)
    {
        if(std::chrono::duration_cast<std::chrono::seconds>(now - m_timepointLastProgressLogEntry).count() > m_progressLogging_s)
        {
            m_timepointLastProgressLogEntry = now;
            std::string logEntry = m_logPrefix;

            if(downloadTotal > 0)
            {
                int progressValue = float(100.0 / downloadTotal * downloadNow);
                logEntry += fmt::format("down {}% [{} bytes]", progressValue, downloadNow);
            }

            if(uploadTotal)
            {
                if(downloadTotal != 0)
                    logEntry += " ";

                int progressValue = float(100.0 / uploadTotal * uploadNow);
                logEntry += fmt::format("up {}% [{} bytes]", progressValue, uploadNow);
            }
            m_logger->info(logEntry);
        }
    }

    if(std::chrono::duration_cast<std::chrono::seconds>(now - m_timepointLastProgress).count() > m_progressTimeout_s)
    {
        m_logger->error(fmt::format("progress timeout of {} seconds exceeded", m_progressTimeout_s));
        m_asyncResult = TIMEOUT;
        return -1;
    }

    if(m_maxTransferDuration_s)
    {
        if(std::chrono::duration_cast<std::chrono::seconds>(now - m_timepointTransferBegin).count() > m_maxTransferDuration_s)
        {
            m_logger->error(fmt::format("max transfer duration of {} seconds exceeded", m_maxTransferDuration_s));
            m_asyncResult = TIMEOUT;
            return -1;
        }
    }

    return 0; // all is good
}

int CurlAsyncTransfer::staticOnDebugCallback(CURL *handle, curl_infotype type, char *data, size_t size, void *token)
{
    if(token != nullptr)
        return static_cast<CurlAsyncTransfer*>(token)->onDebugCallback(handle, type, data, size);
    else
        return -1;
}

int CurlAsyncTransfer::onDebugCallback(CURL *handle, curl_infotype type, char *data, size_t size)
{
    if(m_tracing)
        m_tracing->traceCurlDebugInfo(handle, type, data, size);

    return 0;
}

void CurlAsyncTransfer::setLogPrefix(const std::string &newLogPrefix)
{
    m_logPrefix = newLogPrefix;
}

void CurlAsyncTransfer::setProgressLogging_s(unsigned int newProgressLogging_s)
{
    m_progressLogging_s = newProgressLogging_s;
}

uint64_t CurlAsyncTransfer::transferSpeed_BytesPerSecond() const
{
    return m_transferSpeed_BytesPerSecond;
}

uint64_t CurlAsyncTransfer::transferredBytes() const
{
    return m_transferredBytes;
}

float CurlAsyncTransfer::transferDuration_s() const
{
    return m_transferDuration_s;
}

void CurlAsyncTransfer::setTracing(std::unique_ptr<TracingInterface> newTracing)
{
    m_tracing = std::move(newTracing);
}

}
