#include "libcurl-wrapper/curlmultiasync.hpp"

#include <fmt/core.h>

namespace curl
{

CurlMultiAsync::CurlMultiAsync(const cu::Logger &logger)
    : m_logger(logger)
{
    m_multiHandle = curl_multi_init();

    if(m_multiHandle == NULL)
    {
        std::string errMsg = "curl_multi_init() failed !!!";
        m_logger->error(errMsg);
        throw std::runtime_error(errMsg);
    }
    else
        m_thread = std::make_unique<std::thread>(&CurlMultiAsync::threadedFunction, this);
}

CurlMultiAsync::~CurlMultiAsync()
{
    m_threadKeepRunning = false;
    m_thread->join();

    cancelAllTransfers();
    curl_multi_cleanup(m_multiHandle);
}

void CurlMultiAsync::performTransfer(std::shared_ptr<CurlAsyncTransfer> transfer)
{
    const std::lock_guard<std::mutex> lock(m_queueMutex);
    m_incomingTransfers.push(transfer);

    // Wake up a blocking curl_multi_poll() call
    // This is the ONLY function on CURLM handles, that is safe to call concurrently from another thread (or even multiple threads)
    curl_multi_wakeup(m_multiHandle);
}

void CurlMultiAsync::cancelTransfer(std::shared_ptr<CurlAsyncTransfer> transfer)
{
    const std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eleminatingTransfers.push(transfer);

    // Wake up a blocking curl_multi_poll() call
    // This is the ONLY function on CURLM handles, that is safe to call concurrently from another thread (or even multiple threads)
    curl_multi_wakeup(m_multiHandle);
}

void CurlMultiAsync::cancelAllTransfers()
{
    m_cancelAllTransfers = true;
}

void CurlMultiAsync::waitForCompletion()
{
    for(;;)
    {
        if(m_incomingTransfers.empty() && m_runningTransfers.empty()) // TODO: do we need a mutex here?
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool CurlMultiAsync::waitForStarted(uint32_t timeoutMs)
{
    uint32_t elapsedMs = 0;

    while(elapsedMs < timeoutMs)
    {
        if(!m_runningTransfers.empty()) // TODO: do we need a mutex here?
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        elapsedMs += 100;
    }

    return false;
}

void CurlMultiAsync::threadedFunction()
{
    while(m_threadKeepRunning)
    {
        try
        {
            if(m_runningTransfers.empty())
                handleQueues();
            else
                handleMultiStackTransfers();
        }
        catch(std::exception& e)
        {
             m_logger->error(fmt::format("C++ exception occurred: {}", e.what()));
        }
        catch(...)
        {
            m_logger->error("C++ exception occurred: unknown exception class");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    m_logger->debug("thread finished");
}

void CurlMultiAsync::handleQueues()
{
    std::shared_ptr<CurlAsyncTransfer> transfer;

    while((transfer = getNextIncomingTransfer()))
    {
        if(m_traceConfiguration)
            m_traceConfiguration->configureTracing(transfer);

        transfer->_prepareTransfer();
        CURLMcode mc = curl_multi_add_handle(m_multiHandle, transfer->curl().handle);
        if(mc != 0)
        {
            m_logger->error(fmt::format("curl_multi_add_handle error {}", static_cast<int>(mc)));
            transfer->_processResponse(CANCELED, CURL_LAST);
            return;
        }

        m_runningTransfers.emplace_back(std::move(transfer));
    }

    while((transfer = getNextEleminatingTransfer()))
    {
        removeTransferFromRunningTransfers(transfer->curl().handle);
        transfer->_processResponse(CANCELED, CURL_LAST);
        curl_multi_remove_handle(m_multiHandle, transfer->curl().handle);
    }

    if(m_cancelAllTransfers)
    {
        m_cancelAllTransfers = false;

        auto transferIterator = m_runningTransfers.begin();
        while(transferIterator != m_runningTransfers.end())
        {
            (*transferIterator)->_processResponse(CANCELED, CURL_LAST);
            curl_multi_remove_handle(m_multiHandle, (*transferIterator)->curl().handle);
            transferIterator = m_runningTransfers.erase(transferIterator); // erase will increment the iterator
        }
    }
}

void CurlMultiAsync::handleMultiStackTransfers()
{
    int transfersRunning = 1;
    while(transfersRunning)
    {
        handleQueues();

        CURLMcode mc;
        {
            const std::lock_guard<std::mutex> lock(m_queueMutex);

            mc = curl_multi_perform(m_multiHandle, &transfersRunning);
            if(mc != 0)
            {
                m_logger->error(fmt::format("curl_multi_poll error {}", static_cast<int>(mc)));
                restartMultiStack();
            }
        }

        handleMultiStackMessages();

        if(transfersRunning)
        {
            mc = curl_multi_poll(m_multiHandle, NULL, 0, 1000, NULL);
            if(mc != 0)
            {
                m_logger->error(fmt::format("curl_multi_poll error {}", static_cast<int>(mc)));
                restartMultiStack();
            }
        }
    }
}

void CurlMultiAsync::handleMultiStackMessages()
{
    CURLMsg *curlMessage;
    int messagesLeft;

    while((curlMessage = curl_multi_info_read(m_multiHandle, &messagesLeft)))
    {
        if(curlMessage->msg == CURLMSG_DONE)
        {
            auto asyncTransfer = removeTransferFromRunningTransfers(curlMessage->easy_handle);

            if(asyncTransfer)
                asyncTransfer->_processResponse(CURL_DONE, curlMessage->data.result);
            else
                m_logger->debug(fmt::format("transfer missing {}", curlMessage->easy_handle));

            curl_multi_remove_handle(m_multiHandle, curlMessage->easy_handle);
            // WARNING: CURLMsg *curlMessage is no longer valid after curl_multi_remove_handle()
        }
        else
        {
            // Until now, there are no other msg types defined in libcurl
            m_logger->warning(fmt::format("got unknown message {} from handle {}", static_cast<int>(curlMessage->msg), curlMessage->easy_handle));
        }
    }
}

void CurlMultiAsync::restartMultiStack()
{
    cancelAllTransfers();

    for(int i=0; i<3; i++)
    {
        curl_multi_cleanup(m_multiHandle);
        m_multiHandle = curl_multi_init();

        if(m_multiHandle != NULL)
            return;

        m_logger->error("curl_multi_init() failed !");
    }

    // TODO: add callback to overwrite this ...
    m_logger->error("curl_multi_init() failed multiple times !!! Exiting application");
    std::exit(-123);
}

std::shared_ptr<CurlAsyncTransfer> CurlMultiAsync::getNextIncomingTransfer()
{
    std::shared_ptr<CurlAsyncTransfer> transfer;

    const std::lock_guard<std::mutex> lock(m_queueMutex);
    if(m_incomingTransfers.empty())
        return transfer;

    transfer = m_incomingTransfers.front();
    m_incomingTransfers.pop();
    return transfer;
}

std::shared_ptr<CurlAsyncTransfer> CurlMultiAsync::getNextEleminatingTransfer()
{
    std::shared_ptr<CurlAsyncTransfer> transfer;

    const std::lock_guard<std::mutex> lock(m_queueMutex);
    if(m_eleminatingTransfers.empty())
        return transfer;

    transfer = m_eleminatingTransfers.front();
    m_eleminatingTransfers.pop();
    return transfer;
}

std::shared_ptr<CurlAsyncTransfer> CurlMultiAsync::removeTransferFromRunningTransfers(CURL *transferHandle)
{
    std::shared_ptr<CurlAsyncTransfer> transfer;

    auto iter = std::find_if(m_runningTransfers.begin(), m_runningTransfers.end(), [&transferHandle](const std::shared_ptr<CurlAsyncTransfer> &entry)
        {
            return entry->curl().handle == transferHandle;
        }
    );

    if(iter != m_runningTransfers.end())
    {
        transfer = *iter;
        m_runningTransfers.erase(iter);
    }
    else
        m_logger->error(fmt::format("failed to find matching AsyncTransfer object for handle {}", transferHandle));

    return transfer;
}

void CurlMultiAsync::setTraceConfiguration(std::shared_ptr<TraceConfigurationInterface> newTraceConfiguration)
{
    m_traceConfiguration = newTraceConfiguration;
}

}
