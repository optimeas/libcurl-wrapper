#pragma once

#include "libcurl-wrapper/curlasynctransfer.hpp"

namespace curl
{

class CurlHttpTransfer : public CurlAsyncTransfer
{
public:
    explicit CurlHttpTransfer(const cu::Logger& logger);
    virtual ~CurlHttpTransfer() = default; // Prevent undefined behavior when used as base class and delete per base class pointer

    std::string responseHeader(std::string headerName);
    std::unordered_map<std::string, std::string> &responseHeaders();
    std::vector<char> &responseData();
    void setOutputFilename(const std::string& fileNameWithPath);

    void setHeader(const std::string &name, const std::string &content);
    void clearHeaders();

    void setUploadFilename(const std::string& fileNameWithPath);
    void setPostData(const char *data,  long size = -1, bool copyData=false);

    virtual void prepareTransfer() override;
    virtual void processResponse() override;

private:
    static size_t staticOnWriteCallback(char *ptr, size_t size, size_t nmemb, void *token);
    void onWriteCallback(const char *ptr, size_t realsize);
    static size_t staticOnHeaderCallback(char *buffer, size_t size, size_t nitems, void *token);
    void onHeaderCallback(const char *buffer, size_t realsize);

    std::unordered_map<std::string, std::string> m_responseHeaders;
    std::vector<char> m_responseData;
    std::string m_outputFileName;
    std::ofstream m_outputFile;
    std::unordered_map<std::string, std::string> m_requestHeaders;
    std::string m_uploadFileName;
    FILE *m_uploadFileHandle{nullptr};
};

}
