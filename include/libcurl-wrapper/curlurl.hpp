#pragma once

#include <curl/curl.h>

#include <string>

namespace curl
{

class Url
{
public:
    Url();
    Url(const char *url, bool allowMissingScheme = false);        // cppcheck-suppress noExplicitConstructor
    Url(const std::string &url, bool allowMissingScheme = false); // cppcheck-suppress noExplicitConstructor
    ~Url();

    Url(const Url &other) = delete;
    Url& operator=(const Url &other) = delete;

    bool fromString(const char *url, bool allowMissingScheme = false);
    bool fromString(const std::string &url, bool allowMissingScheme = false);
    std::string toString();

    bool setPath(const std::string &path, bool overwriteExisting = true);
    bool setPage(const std::string &page, bool overwriteExisting = true);

    bool isValid();

private:
    CURLU* m_handle{nullptr}; // TODO: try to replace with uniqe_ptr
    bool m_isValid{false};
};

}
