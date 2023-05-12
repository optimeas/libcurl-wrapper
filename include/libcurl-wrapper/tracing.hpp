#pragma once

#include <curl/curl.h>

#include <string_view>
#include <memory>

namespace curl
{

class TracingInterface
{
public:
    virtual ~TracingInterface() = default; // Prevent undefined behavior when used as base class and delete per base class pointer

    virtual void traceCurlDebugInfo(CURL *handle, curl_infotype type, char *data, size_t size) = 0;
    virtual void traceMessage(std::string_view entry) = 0;

    virtual void initialize() = 0;
    virtual void finalize() = 0;
};

class CurlAsyncTransfer;
class TraceConfigurationInterface
{
public:

    virtual ~TraceConfigurationInterface() = default; // Prevent undefined behavior when used as base class and delete per base class pointer

    virtual void configureTracing(std::shared_ptr<CurlAsyncTransfer> transfer) = 0;
};

}
