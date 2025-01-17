#include "libcurl-wrapper/curlhttptransfer.hpp"
#include "cpp-utils/stringutils.hpp"

#include <fmt/ostream.h>

#include <regex>

namespace curl
{

CurlHttpTransfer::CurlHttpTransfer(const cu::Logger &logger)
    : CurlAsyncTransfer(logger)
{

}

std::string CurlHttpTransfer::responseHeader(const std::string &headerName)
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

std::unordered_map<std::string, std::string> &CurlHttpTransfer::responseHeaders()
{
    return m_responseHeaders;
}

std::vector<char> &CurlHttpTransfer::responseData()
{
    return m_responseData;
}

void CurlHttpTransfer::setOutputFilename(const std::string &fileNameWithPath)
{
    m_outputFileName = fileNameWithPath;
}

void CurlHttpTransfer::setHeader(const std::string &name, const std::string &content)
{
    auto headerName = cu::simpleCase(name);
    m_requestHeaders[headerName] = content;
}

void CurlHttpTransfer::clearHeaders()
{
    m_requestHeaders.clear();
}

void CurlHttpTransfer::setUploadFilename(const std::string &fileNameWithPath)
{
    m_uploadFileName = fileNameWithPath;
}

void CurlHttpTransfer::setPostData(const char *data, long size, bool copyData)
{
    if(size != -1)
        curl_easy_setopt(m_curl.handle, CURLOPT_POSTFIELDSIZE, size);

    if(copyData)
        curl_easy_setopt(m_curl.handle, CURLOPT_COPYPOSTFIELDS, data);
    else
        curl_easy_setopt(m_curl.handle, CURLOPT_POSTFIELDS, data);
}

void CurlHttpTransfer::prepareTransfer()
{
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
            std::string errMsg = fmt::format("failed to open output file {}", m_outputFileName);
            m_logger->error(errMsg);
            throw std::runtime_error(errMsg);
        }
    }

    if(!m_uploadFileName.empty())
    {
        m_uploadFileHandle = std::fopen(m_uploadFileName.c_str(), "r");
        if(m_uploadFileHandle == nullptr)
        {
            std::string errMsg = fmt::format("failed to open upload file {}", m_uploadFileName);
            m_logger->error(errMsg);
            throw std::runtime_error(errMsg);
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

    if(m_followRedirects)
    {
        curl_easy_setopt(m_curl.handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(m_curl.handle, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
    }
    else
        curl_easy_setopt(m_curl.handle, CURLOPT_FOLLOWLOCATION, 0L);
}

void CurlHttpTransfer::processResponse()
{
    if(m_outputFile.is_open())
        m_outputFile.close();

    if(m_uploadFileHandle != nullptr)
    {
        fclose(m_uploadFileHandle);
        m_uploadFileHandle = nullptr;
    }
}

size_t CurlHttpTransfer::staticOnWriteCallback(const char *ptr, size_t size, size_t nmemb, void *token)
{
    size_t realsize = size * nmemb;

    if(token != nullptr)
        static_cast<CurlHttpTransfer*>(token)->onWriteCallback(ptr, realsize);

    return realsize;
}

void CurlHttpTransfer::onWriteCallback(const char *ptr, size_t realsize)
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

size_t CurlHttpTransfer::staticOnHeaderCallback(const char *buffer, size_t size, size_t nitems, void *token)
{
    size_t realsize = size * nitems;

    if(token != nullptr)
    {
        static_cast<CurlHttpTransfer*>(token)->onHeaderCallback(buffer, realsize);
    }

    return realsize;
}

void CurlHttpTransfer::onHeaderCallback(const char *buffer, size_t realsize)
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

void CurlHttpTransfer::setFollowRedirects(bool newFollowRedirects)
{
    m_followRedirects = newFollowRedirects;
}

}
