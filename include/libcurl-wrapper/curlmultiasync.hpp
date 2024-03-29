#pragma once

#include "curlasynctransfer.hpp"
#include "tracing.hpp"

#include "cpp-utils/logging.hpp"

#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <queue>

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
    bool waitForStarted(uint32_t timeoutMs);

    void setTraceConfiguration(std::shared_ptr<TraceConfigurationInterface> newTraceConfiguration);

private:
    void threadedFunction(void);

    void handleQueues();
    void handleMultiStackTransfers();
    void handleMultiStackMessages();
    void restartMultiStack();

    std::shared_ptr<CurlAsyncTransfer> getNextIncomingTransfer();
    std::shared_ptr<CurlAsyncTransfer> getNextEleminatingTransfer();

    std::shared_ptr<CurlAsyncTransfer> removeTransferFromRunningTransfers(CURL* transferHandle);

    cu::Logger m_logger;
    std::atomic<bool> m_threadKeepRunning{true};
    std::unique_ptr<std::thread> m_thread;
    CURLM *m_multiHandle{nullptr};

    std::mutex m_queueMutex;
    std::queue<std::shared_ptr<CurlAsyncTransfer>> m_incomingTransfers;
    std::queue<std::shared_ptr<CurlAsyncTransfer>> m_eleminatingTransfers;
    std::atomic_bool m_cancelAllTransfers{false};

    std::vector<std::shared_ptr<CurlAsyncTransfer>> m_runningTransfers;
    std::shared_ptr<TraceConfigurationInterface> m_traceConfiguration;

};

}
