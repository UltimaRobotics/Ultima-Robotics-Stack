
#pragma once

#include <memory>
#include <string>
#include <set>
#include <mutex>
#include <atomic>
#include <future>
#include <ThreadManager.hpp>
#include "operation_handler.hpp"
#include "package_config.hpp"

extern "C" {
#include "../thirdparty/ur-rpc-template/deps/cJSON/cJSON.h"
}

class RpcOperationProcessor {
public:
    RpcOperationProcessor(const PackageConfig& config, bool verbose);
    ~RpcOperationProcessor();

    // Process an incoming RPC request and send response
    void processRequest(const char* payload, size_t payload_len);
    
    // Set the response topic prefix
    void setResponseTopic(const std::string& topic);

private:
    // Request context structure for passing data to thread
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string responseTopic;
        std::shared_ptr<const PackageConfig> config;  // Immutable shared pointer - no copy needed
        bool verbose;
        std::shared_ptr<ThreadMgr::ThreadManager> threadManager;  // Thread manager for cleanup
        std::set<unsigned int>* activeThreads;  // Pointer to active threads set
        std::mutex* threadsMutex;  // Pointer to mutex for thread-safe access
        std::atomic<unsigned int> threadId;  // ID of this thread (atomic for thread-safe access)
        std::shared_ptr<std::promise<unsigned int>> threadIdPromise;  // Promise for thread ID initialization
        std::shared_future<unsigned int> threadIdFuture;  // Future for thread to wait on
        
        RequestContext(std::string json, std::string id, std::string topic, 
                      std::shared_ptr<const PackageConfig> cfg, bool v,
                      std::shared_ptr<ThreadMgr::ThreadManager> tm,
                      std::set<unsigned int>* at, std::mutex* mtx)
            : requestJson(std::move(json))
            , transactionId(std::move(id))
            , responseTopic(std::move(topic))
            , config(std::move(cfg))  // Share ownership, no copy
            , verbose(v)
            , threadManager(std::move(tm))
            , activeThreads(at)
            , threadsMutex(mtx)
            , threadId(0)  // Will be set via promise after thread creation
            , threadIdPromise(std::make_shared<std::promise<unsigned int>>())
            , threadIdFuture(threadIdPromise->get_future())
        {}
    };
    
    // Thread function for processing operation
    void processOperationThread(std::shared_ptr<RequestContext> context);
    
    // Static thread function that doesn't depend on 'this'
    static void processOperationThreadStatic(std::shared_ptr<RequestContext> context);
    
    // Cleanup completed threads
    void cleanupCompletedThreads();
    
    // Send RPC response
    void sendResponse(const std::string& transactionId, bool success, 
                      const std::string& result, const std::string& error = "");
    
    // Static send response that doesn't depend on 'this'
    static void sendResponseStatic(const std::string& transactionId, bool success,
                                   const std::string& result, const std::string& error,
                                   const std::string& responseTopic);
    
    std::shared_ptr<const PackageConfig> config_;  // Immutable shared pointer to prevent corruption
    bool verbose_;
    std::string responseTopic_;
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;  // Use shared_ptr for lifetime safety (same pattern as g_rpcClient)
    std::atomic<bool> isShuttingDown_{false};  // Flag to prevent new thread creation during shutdown
    
    // Thread tracking
    std::set<unsigned int> activeThreads_;  // Track active thread IDs
    std::mutex threadsMutex_;  // Protect access to activeThreads_
};
