#include "libcurl-wrapper/curlholder.hpp"

#include <cassert>

namespace curl
{

CurlHolder::CurlHolder()
{
    // Allow multithreaded access by locking curl_easy_init().
    // curl_easy_init() is not thread safe.
    // References:
    // https://curl.haxx.se/libcurl/c/curl_easy_init.html
    // https://curl.haxx.se/libcurl/c/threadsafe.html

    curl_easy_init_mutex().lock();
    handle = curl_easy_init();
    curl_easy_init_mutex().unlock();
    assert(handle);
}

CurlHolder::~CurlHolder()
{
    curl_easy_cleanup(handle);
}

std::mutex &CurlHolder::curl_easy_init_mutex()
{
    // Avoids initalization order problems
    static std::mutex curl_easy_init_mutex;
    return curl_easy_init_mutex;
}

}
