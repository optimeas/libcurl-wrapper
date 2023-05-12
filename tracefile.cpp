#include "libcurl-wrapper/tracefile.hpp"

#include "cpp-utils/hexdump.hpp"

#include <fmt/core.h>

namespace curl
{

TraceFile::TraceFile(const cu::Logger &logger)
    : m_logger(logger)
{

}

void TraceFile::initialize()
{
    m_logger->info(fmt::format("create trace file: {}", m_filename));

    m_file.open(m_filename, std::ios::out | std::ios::binary | std::ios::trunc);
    if(!m_file.is_open())
    {
        std::string errMsg = fmt::format("failed to open trace file {}", m_filename);
        m_logger->error(errMsg);
        throw std::runtime_error(errMsg);
    }

    m_timepointTransferBegin = std::chrono::steady_clock::now();
}

void TraceFile::finalize()
{
    m_file.close();
}

void TraceFile::traceCurlDebugInfo([[maybe_unused]] CURL *handle, curl_infotype type, char *data, size_t size)
{
    auto now = std::chrono::steady_clock::now();

    switch(type)
    {
    case CURLINFO_TEXT:         m_file << "### INFO: ";     break;
    case CURLINFO_HEADER_IN:    m_file << "### HDR_IN: ";   break;
    case CURLINFO_HEADER_OUT:   m_file << "### HDR_OUT: ";  break;
    case CURLINFO_DATA_IN:      m_file << "### DATA_IN: ";  break;
    case CURLINFO_DATA_OUT:     m_file << "### DATA_OUT: "; break;
    case CURLINFO_SSL_DATA_IN:  m_file << "### SSL_IN: ";   break;
    case CURLINFO_SSL_DATA_OUT: m_file << "### SSL_OUT: ";  break;
    case CURLINFO_END:                                      break;
    }
    m_file << std::to_string(size) << " bytes, time " << std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timepointTransferBegin).count()) << " ms ###" << std::endl;

    if((m_enableSslData == false) &&  ((type == CURLINFO_SSL_DATA_IN) || (type == CURLINFO_SSL_DATA_OUT)) )
        return;

    if((type == CURLINFO_DATA_IN) || (type == CURLINFO_DATA_OUT))
    {
        if((m_maxDataSize > 0) && (m_dataSize > m_maxDataSize))
        {
            m_file << "<Truncated due to max data size " << std::to_string(m_maxDataSize) << ">" << std::endl;
            return;
        }

        m_dataSize += size;
    }

    if((type == CURLINFO_TEXT) || (type == CURLINFO_HEADER_IN) || (type == CURLINFO_HEADER_OUT))
        m_file.write(data, size);
    else
        m_file << cu::Hexdump(reinterpret_cast<unsigned char *>(data), size) << std::endl;
}

void TraceFile::traceMessage(std::string_view entry)
{
    m_file.write(entry.data(), entry.size());
}

const std::string &TraceFile::filename() const
{
    return m_filename;
}

void TraceFile::setFilename(const std::string &newFilename)
{
    m_filename = newFilename;
}

uint32_t TraceFile::maxDataSize() const
{
    return m_maxDataSize;
}

void TraceFile::setMaxDataSize(uint32_t newMaxDataSize)
{
    m_maxDataSize = newMaxDataSize;
}

void TraceFile::enableSslData(bool newEnableSslData)
{
    m_enableSslData = newEnableSslData;
}

}
