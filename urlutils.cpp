#include "libcurl-wrapper/urlutils.hpp"

#include <curl/curl.h>

#include <iostream>
#include <memory>
#include <cstring>

namespace curl
{

using CurlUrlPtr = std::unique_ptr<CURLU, decltype(curl_url_cleanup) *>;

bool urlValidate(const std::string &url, bool allowMissingScheme)
{
    CURLUcode result;
    CurlUrlPtr handle(curl_url(), curl_url_cleanup);
    unsigned int flags = 0;

    if(allowMissingScheme)
        flags = CURLU_DEFAULT_SCHEME;

    if(!handle)
        return false;

    result = curl_url_set(handle.get(), CURLUPART_URL, url.c_str(), flags);
    if(result != CURLUE_OK)
        return false;

    return true;
}

bool urlReplacePath(std::string &url, const std::string &path, bool overwriteExisting)
{
    CURLUcode result;
    CurlUrlPtr handle(curl_url(), curl_url_cleanup);

    if(!handle)
        return false;

    result = curl_url_set(handle.get(), CURLUPART_URL, url.c_str(), CURLU_DEFAULT_SCHEME);
    if(result != CURLUE_OK)
        return false;

    char *content;
    int len = 0;
    result = curl_url_get(handle.get(), CURLUPART_PATH, &content, 0);
    if(result != CURLUE_OK)
    {
        curl_free(content);
        return false;
    }

    len = std::strlen(content);
    curl_free(content);

    if((len < 2) || (overwriteExisting == true)) // path is at least "/"
    {
        result = curl_url_set(handle.get(), CURLUPART_PATH, path.c_str(), 0);
        if(result != CURLUE_OK)
            return false;
    }

    char *urlFromCurl;
    result = curl_url_get(handle.get(), CURLUPART_URL, &urlFromCurl, 0);

    if(result != CURLUE_OK)
    {
        curl_free(urlFromCurl);
        return false;
    }

    url = std::string(urlFromCurl);
    curl_free(urlFromCurl);
    return true;
}

}
