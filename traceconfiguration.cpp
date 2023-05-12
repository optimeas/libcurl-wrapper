#include "libcurl-wrapper/traceconfiguration.hpp"
#include "libcurl-wrapper/tracefile.hpp"
#include "libcurl-wrapper/curlasynctransfer.hpp"
#include "cpp-utils/timeutils.hpp"

#include <fmt/core.h>

#include <iostream>
#include <iomanip>
#include <ctime>

namespace curl
{

TraceConfiguration::TraceConfiguration(const cu::Logger &logger)
    : m_logger(logger)
{

}

void TraceConfiguration::configureTracing(std::shared_ptr<CurlAsyncTransfer> transfer)
{
    if(m_transfersToTrace == 0) // disabled
        return;

    std::unique_ptr<TraceFile> tracer = std::make_unique<TraceFile>(m_logger);

    tracer->setFilename(generateNextFilename());
    tracer->setMaxDataSize(m_maxDataSize);
    tracer->enableSslData(m_enableSslData);

    transfer->setTracing(std::move(tracer));
}

std::string TraceConfiguration::generateNextFilename()
{
    std::string filename;

    if(m_transfersToTrace > 0) // -1 => continuous tracing
        m_transfersToTrace--;

    if(m_rotationPoolSize > 0)
    {
        filename = fmt::format("{}/{}_{}.ctf", m_path, m_filenamePrefix, m_nextRotationPoolIndex);

        m_nextRotationPoolIndex++;
        if(m_nextRotationPoolIndex >= m_rotationPoolSize)
            m_nextRotationPoolIndex = 0;

    }
    else
        filename = fmt::format("{}/{}_{}.ctf", m_path, m_filenamePrefix, cu::getCurrentTimestampUtcFilename(true));

    return filename;
}

void TraceConfiguration::enableTracing(int32_t newTransfersToTrace)
{
    m_transfersToTrace = newTransfersToTrace;
}

void TraceConfiguration::enableSslData(bool newEnableSslData)
{
    m_enableSslData = newEnableSslData;
}

void TraceConfiguration::setMaxDataSize(uint32_t newMaxDataSize)
{
    m_maxDataSize = newMaxDataSize;
}

void TraceConfiguration::setPath(const std::string &newPath)
{
    m_path = newPath;
}

void TraceConfiguration::setFilenamePrefix(const std::string &newFilenamePrefix)
{
    m_filenamePrefix = newFilenamePrefix;
}

uint32_t TraceConfiguration::rotationPoolSize() const
{
    return m_rotationPoolSize;
}

void TraceConfiguration::setRotationPoolSize(uint32_t newRotationPoolSize)
{
    m_rotationPoolSize = newRotationPoolSize;
    m_nextRotationPoolIndex = 0;
}

}
