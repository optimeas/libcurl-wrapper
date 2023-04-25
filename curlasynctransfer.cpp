#include "libcurl-wrapper/curlasynctransfer.hpp"
#include "cpp-utils/hexdump.hpp"
#include "cpp-utils/stringutils.hpp"

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <regex>

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

std::string CurlAsyncTransfer::responseHeader(std::string headerName)
{
    auto header = cu::simpleCase(headerName);
    try
    {
        return m_responseHeaders.at(header);
    }
    catch(std::out_of_range &e)
    {
        m_logger->warning(fmt::format("header {} does not exist", header));
    }

    return "";
}

std::unordered_map<std::string, std::string> &CurlAsyncTransfer::responseHeaders()
{
    return m_responseHeaders;
}

std::vector<char> &CurlAsyncTransfer::responseData()
{
    return m_responseData;
}

void CurlAsyncTransfer::setOutputFilename(const std::string &fileNameWithPath)
{
    m_outputFileName = fileNameWithPath;
}

void CurlAsyncTransfer::setHeader(const std::string &name, const std::string &content)
{
    auto headerName = cu::simpleCase(name);
    m_requestHeaders[headerName] = content;
}

void CurlAsyncTransfer::clearHeaders()
{
    m_requestHeaders.clear();
}

void CurlAsyncTransfer::setUploadFilename(const std::string &fileNameWithPath)
{
    m_uploadFileName = fileNameWithPath;
}

void CurlAsyncTransfer::setUploadDataPointer(const char *data, long size, bool copyData)
{
    if(size != -1)
        curl_easy_setopt(m_curl.handle, CURLOPT_POSTFIELDSIZE, size);

    if(copyData)
        curl_easy_setopt(m_curl.handle, CURLOPT_COPYPOSTFIELDS, data);
    else
        curl_easy_setopt(m_curl.handle, CURLOPT_POSTFIELDS, data);
}

unsigned int CurlAsyncTransfer::progressTimeout_s() const
{
    return m_progressTimeout_s;
}

void CurlAsyncTransfer::setProgressTimeout_s(unsigned int newProgressTimeout_s)
{
    m_progressTimeout_s = newProgressTimeout_s;
}

void CurlAsyncTransfer::prepareTransfer()
{
    curl_easy_setopt(m_curl.handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl.handle, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl.handle, CURLOPT_XFERINFOFUNCTION, &staticOnProgressCallback);

    curl_easy_setopt(m_curl.handle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_curl.handle, CURLOPT_WRITEFUNCTION, &staticOnWriteCallback);

    curl_easy_setopt(m_curl.handle, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(m_curl.handle, CURLOPT_HEADERFUNCTION, &staticOnHeaderCallback);

    m_responseHeaders.clear();
    m_responseData.clear();

    if(!m_outputFileName.empty())
    {
        m_outputFile.open(m_outputFileName, std::ios::out | std::ios::binary | std::ios::trunc);
        if(!m_outputFile.is_open())
        {
            //TODO: error handling => callback ?
            m_logger->error("file open error");
            return;
        }
    }

    if(!m_uploadFileName.empty())
    {
        m_uploadFileHandle = std::fopen(m_uploadFileName.c_str(), "r");
        if(m_uploadFileHandle == nullptr)
        {
            //TODO: error handling => callback ?
            m_logger->error("file open error");
            return;
        }

        curl_easy_setopt(m_curl.handle, CURLOPT_POST, 1L);
        curl_easy_setopt(m_curl.handle, CURLOPT_READDATA, m_uploadFileHandle);
        curl_easy_setopt(m_curl.handle, CURLOPT_READFUNCTION , nullptr);
        // If you set this callback pointer to NULL, or do not set it at all, the default internal read function will be used.
        // It is doing an fread() on the FILE * userdata set with CURLOPT_READDATA.
    }

    curl_slist_free_all(m_curl.requestHeader);
    m_curl.requestHeader = nullptr;
    for(const auto& [name, value] : m_requestHeaders)
    {
        std::string header;
        if(value.size() == 0)
            header = fmt::format("{}:", name);
        else
            header = fmt::format("{}: {}", name, value);

        m_curl.requestHeader = curl_slist_append(m_curl.requestHeader, header.c_str());
    }

    // libcurl would prepare the header "Expect: 100-continue" by default when uploading files larger than 1 MB.
    // Here we would like to disable this feature:
    m_curl.requestHeader = curl_slist_append(m_curl.requestHeader, "Expect:");

    curl_easy_setopt(m_curl.handle, CURLOPT_HTTPHEADER, m_curl.requestHeader);

    m_responseCode = -1;
    m_asyncResult = RUNNING;
    m_timepointTransferBegin = std::chrono::steady_clock::now();
    m_timepointLastProgress = m_timepointTransferBegin;
}

void CurlAsyncTransfer::processResponse(AsyncResult asyncResult, CURLcode curlResult)
{
    m_curlResult = curlResult;

    if(m_asyncResult != TIMEOUT) // timeout is handled in CurlAsyncTransfer, everything else in CurlMultiAsync
        m_asyncResult = asyncResult;

    if(m_outputFile.is_open())
        m_outputFile.close();

    auto end = std::chrono::steady_clock::now();
    m_logger->debug(fmt::format("transfer took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(end - m_timepointTransferBegin).count()));

    if(curlResult == CURLE_OK)
    {
        if(curl_easy_getinfo(m_curl.handle, CURLINFO_RESPONSE_CODE, &m_responseCode) != CURLE_OK)
            m_responseCode = -1;
    }

    if(m_uploadFileHandle != nullptr)
    {
        fclose(m_uploadFileHandle);
        m_uploadFileHandle = nullptr;
    }

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

size_t CurlAsyncTransfer::staticOnWriteCallback(char *ptr, size_t size, size_t nmemb, void *token)
{
    size_t realsize = size * nmemb;

    if(token != nullptr)
        static_cast<CurlAsyncTransfer*>(token)->onWriteCallback(ptr, realsize);

    return realsize;
}

void CurlAsyncTransfer::onWriteCallback(const char *ptr, size_t realsize)
{
    if((ptr == nullptr) || (realsize == 0))
    {
            m_logger->warning("write: no data");
            return;
    }

    if(m_outputFile.is_open())
        m_outputFile.write(ptr, realsize);
    else
        m_responseData.insert(m_responseData.end(), ptr, ptr + realsize);
}

size_t CurlAsyncTransfer::staticOnHeaderCallback(char *buffer, size_t size, size_t nitems, void *token)
{
    size_t realsize = size * nitems;

    if(token != nullptr)
        static_cast<CurlAsyncTransfer*>(token)->onHeaderCallback(buffer, realsize);

    return nitems * size;
}

void CurlAsyncTransfer::onHeaderCallback(const char *buffer, size_t realsize)
{
    if((buffer == nullptr) || (realsize == 0))
    {
            m_logger->warning("header: no data");
            return;
    }

    std::string header{buffer, realsize};
    cu::trim(header);

    if(header.size() == 0)
        return;

    auto const regexSplit = std::regex(R"(([\w-]+): *([[:print:]]+))");
    std::smatch matches;
    if(std::regex_search(header, matches, regexSplit))
    {
        if(matches.size() == 3)
        {
            auto headerName = cu::simpleCase(matches[1].str());
            auto headerValue = matches[2].str();
            m_responseHeaders[headerName] = headerValue;

            if(headerName == "Content-Length")
            {
                try
                {
                    auto length = stol(headerValue);
                    if(length > 0)
                    {
                        m_responseData.reserve(length);
                    }
                }
                catch(const std::exception &e)
                {
                    m_logger->error(fmt::format("failed to parse Content-Length: {} => {} ", headerValue, e.what()));
                }
            }
        }
    }
    else
    {
        if(header.rfind("HTTP", 0) == 0)
        {
            std::istringstream iss(header);
            std::vector<std::string> results(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());
            m_responseHeaders["HTTP-Version"] = results.at(0);
        }
    }
}

}
