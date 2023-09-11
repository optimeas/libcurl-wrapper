#include "libcurl-wrapper/curlurl.hpp"

#include <iostream>
#include <cstring>
#include <memory>

namespace curl
{

Url::Url()
    : m_handle(curl_url())
{
}

Url::Url(const char *url, bool allowMissingScheme)
    : Url()
{
    fromString(url, allowMissingScheme);
}

Url::Url(const std::string &url, bool allowMissingScheme)
    : Url()
{
    fromString(url, allowMissingScheme);
}

Url::~Url()
{
    curl_url_cleanup(m_handle);
}

bool Url::fromString(const char *url, bool allowMissingScheme)
{
    m_isValid = false;

    unsigned int flags = 0;
    if(allowMissingScheme)
        flags = CURLU_DEFAULT_SCHEME;

    if(curl_url_set(m_handle, CURLUPART_URL, url, flags) != CURLUE_OK)
        return false;

    m_isValid = true;
    return true;
}

bool Url::fromString(const std::string &url, bool allowMissingScheme)
{
    return fromString(url.c_str(), allowMissingScheme);
}

std::string Url::toString()
{
    if(!m_isValid)
        return "";

    char *urlFromCurl;
    if(curl_url_get(m_handle, CURLUPART_URL, &urlFromCurl, 0) != CURLUE_OK)
    {
        curl_free(urlFromCurl);
        m_isValid = false;
        return std::string();
    }

    std::string url = std::string(urlFromCurl);
    curl_free(urlFromCurl);

    return url;
}

bool Url::setPath(const std::string &path, bool overwriteExisting)
{
    if(!m_isValid)
        return false;

    char *pathFromCurl;
    if(curl_url_get(m_handle, CURLUPART_PATH, &pathFromCurl, 0) != CURLUE_OK)
    {
        curl_free(pathFromCurl);
        m_isValid = false;
        return false;
    }

    int len = std::strlen(pathFromCurl);
    curl_free(pathFromCurl);

    if((len < 2) || (overwriteExisting == true)) // path is at least "/"
    {
        if(curl_url_set(m_handle, CURLUPART_PATH, path.c_str(), 0) != CURLUE_OK)
        {
            m_isValid = false;
            return false;
        }
    }

    return true;
}

bool Url::setPage(const std::string &page, bool overwriteExisting)
{
    if(!m_isValid)
        return false;

    char *pathFromCurl;
    if(curl_url_get(m_handle, CURLUPART_PATH, &pathFromCurl, 0) != CURLUE_OK)
    {
        curl_free(pathFromCurl);
        m_isValid = false;
        return false;
    }

    std::string path = std::string(pathFromCurl);
    curl_free(pathFromCurl);

    if(path.find('.') == std::string::npos) // not contains
    {
        if(path.back() == '/')
            path.pop_back();

        if(page.front() != '/')
            path += '/';

        path += page;
        if(curl_url_set(m_handle, CURLUPART_PATH, path.c_str(), 0) != CURLUE_OK)
        {
            m_isValid = false;
            return false;
        }
    }
    else
    {
        if(overwriteExisting)
        {
            std::string::size_type slashPos;
            if((slashPos = path.rfind('/')) != std::string::npos)
            {
                path.erase(slashPos + 1);

                if(page.front() == '/')
                    path.pop_back();

                path += page;
                if(curl_url_set(m_handle, CURLUPART_PATH, path.c_str(), 0) != CURLUE_OK)
                {
                    m_isValid = false;
                    return false;
                }

            }
            // else => should not happen ...
        }
    }

    return true;
}

bool Url::isValid()
{
    return m_isValid;
}

}
