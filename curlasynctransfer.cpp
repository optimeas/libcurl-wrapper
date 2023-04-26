#include "libcurl-wrapper/curlasynctransfer.hpp"

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
    curl_easy_setopt(m_curl.handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl.handle, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl.handle, CURLOPT_XFERINFOFUNCTION, &staticOnProgressCallback);

    prepareTransfer();

    m_responseCode = -1;
    m_asyncResult = RUNNING;
    m_timepointTransferBegin = std::chrono::steady_clock::now();
    m_timepointLastProgress = m_timepointTransferBegin;
}

void CurlAsyncTransfer::_processResponse(AsyncResult asyncResult, CURLcode curlResult)
{
    m_curlResult = curlResult;

    if(m_asyncResult != TIMEOUT) // timeout is handled in CurlAsyncTransfer, everything else in CurlMultiAsync
        m_asyncResult = asyncResult;

    auto end = std::chrono::steady_clock::now();
    m_logger->debug(fmt::format("transfer took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(end - m_timepointTransferBegin).count()));

    if(curlResult == CURLE_OK)
    {
        if(curl_easy_getinfo(m_curl.handle, CURLINFO_RESPONSE_CODE, &m_responseCode) != CURLE_OK)
            m_responseCode = -1;
    }

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

}
