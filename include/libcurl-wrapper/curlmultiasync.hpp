#pragma once

#include "curlasynctransfer.hpp"
#include "tracing.hpp"

#include "cpp-utils/logging.hpp"
#include "cpp-utils/monitor.hpp"

#include <atomic>
#include <thread>
#include <memory>

namespace curl
{

class CurlMultiAsync
{
public:
    explicit CurlMultiAsync(const cu::Logger& logger);
    ~CurlMultiAsync();

    void performTransfer(std::shared_ptr<CurlAsyncTransfer> transfer);
    void cancelTransfer(std::shared_ptr<CurlAsyncTransfer> transfer);
    void cancelAllTransfers();

    void waitForCompletion();

    void setTraceConfiguration(std::shared_ptr<TraceConfigurationInterface> newTraceConfiguration);

private:
    void threadedFunction(void);

    void handleMultiStackTransfers();
    void handleMultiStackMessages();
    void restartMultiStack();

    std::shared_ptr<CurlAsyncTransfer> removeTransferFromRunningTransfers(CURL* transferHandle);

    cu::Logger m_logger;
    std::atomic<bool> m_threadKeepRunning{true};
    std::unique_ptr<std::thread> m_thread;
    CURLM *m_multiHandle{nullptr};

    cu::Monitor<std::vector<std::shared_ptr<CurlAsyncTransfer>>> m_runningTransfers;
    std::shared_ptr<TraceConfigurationInterface> m_traceConfiguration;

};

}
