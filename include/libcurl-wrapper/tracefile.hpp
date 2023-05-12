#pragma once

#include "cpp-utils/logging.hpp"
#include "libcurl-wrapper/tracing.hpp"

#include <string>
#include <fstream>
#include <chrono>

namespace curl
{

class TraceFile : public TracingInterface
{
public:
    explicit TraceFile(const cu::Logger& logger);

    virtual void traceCurlDebugInfo(CURL *handle, curl_infotype type, char *data, size_t size) override;
    virtual void traceMessage(std::string_view entry) override;

    virtual void initialize() override;
    virtual void finalize() override;

    const std::string &filename() const;
    void setFilename(const std::string &newFilename);

    uint32_t maxDataSize() const;
    void setMaxDataSize(uint32_t newMaxDataSize);

    void enableSslData(bool newEnableSslData);

private:
    cu::Logger m_logger;
    std::string m_filename;
    uint32_t m_maxDataSize{0};
    uint32_t m_dataSize{0};
    std::ofstream m_file;
    bool m_enableSslData{false};
    std::chrono::steady_clock::time_point m_timepointTransferBegin;

};

}
