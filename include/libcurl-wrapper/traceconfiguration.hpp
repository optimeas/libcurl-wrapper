#pragma once

#include "cpp-utils/logging.hpp"
#include "libcurl-wrapper/tracing.hpp"

#include <string>

namespace curl
{

class TraceConfiguration : public TraceConfigurationInterface
{
public:
    explicit TraceConfiguration(const cu::Logger& logger);

    virtual void configureTracing(std::shared_ptr<CurlAsyncTransfer> transfer) override;

    void enableTracing(int32_t newTransfersToTrace = -1);
    void enableSslData(bool newEnableSslData);

    void setMaxDataSize(uint32_t newMaxDataSize);    
    void setPath(const std::string &newPath);
    void setFilenamePrefix(const std::string &newFilenamePrefix);

    uint32_t rotationPoolSize() const;
    void setRotationPoolSize(uint32_t newRotationPoolSize);

private:
    std::string generateNextFilename();

    cu::Logger m_logger;
    uint32_t m_maxDataSize{0};
    bool m_enableSslData{false};
    std::string m_path{"/tmp"};
    std::string m_filenamePrefix{"curl_trace"};
    uint32_t m_rotationPoolSize{0};
    uint32_t m_nextRotationPoolIndex{0};
    int32_t m_transfersToTrace{0};
};

}
