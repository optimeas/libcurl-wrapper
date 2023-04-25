#pragma once

#include <curl/curl.h>
#include <mutex>

namespace curl
{

class CurlHolder
{
public:
    CurlHolder();
    CurlHolder(const CurlHolder &other) = delete;
    ~CurlHolder();

    CurlHolder& operator=(const CurlHolder &other) = delete;

    CURL* handle{nullptr};
    struct curl_slist* requestHeader{nullptr};

private:
    static std::mutex& curl_easy_init_mutex();
};

}
