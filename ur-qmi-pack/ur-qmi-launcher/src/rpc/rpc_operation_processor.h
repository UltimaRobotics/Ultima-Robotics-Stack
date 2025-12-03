#ifndef RPC_OPERATION_PROCESSOR_H
#define RPC_OPERATION_PROCESSOR_H

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <set>
#include <nlohmann/json.hpp>

// Forward declaration
struct PackageConfig;

extern "C" {
#include "../ur-rpc-template/extensions/direct_template.h"
}

#include "core/ThreadManager.hpp"

// External global ThreadManager from main.cpp
extern ThreadMgr::ThreadManager* g_threadManager;

class RpcOperationProcessor {
public:
    // Constructor with configuration and verbosity settings
    RpcOperationProcessor(const PackageConfig& config, bool verbose);
    ~RpcOperationProcessor();
    
    // Core processing interface
    void processRequest(const char* payload, size_t payload_len);
    void setResponseTopic(const std::string& topic);

private:
    // Thread management for concurrent operations - using global ThreadManager
    std::set<unsigned int> activeThreads_;
    std::mutex threadsMutex_;
    std::atomic<bool> isShuttingDown_{false};
    
    // Configuration
    std::shared_ptr<const PackageConfig> config_;
    bool verbose_;
    std::string responseTopic_;
    
    // Request context for thread-safe data passing
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string responseTopic;
        std::shared_ptr<const PackageConfig> config;
        bool verbose;
        
        // Thread synchronization primitives
        std::shared_ptr<std::promise<unsigned int>> threadIdPromise;
        std::shared_future<unsigned int> threadIdFuture;
        std::atomic<unsigned int> threadId{0};
        
        // Thread management references - using global ThreadManager
        std::set<unsigned int>* activeThreads;
        std::mutex* threadsMutex;
        
        // Constructor
        RequestContext(const std::string& reqJson, const std::string& transId, 
                      const std::string& respTopic, std::shared_ptr<const PackageConfig> cfg,
                      bool verb, std::set<unsigned int>* active, std::mutex* mutex)
            : requestJson(reqJson), transactionId(transId), responseTopic(respTopic)
            , config(cfg), verbose(verb), activeThreads(active), threadsMutex(mutex)
        {
            threadIdPromise = std::make_shared<std::promise<unsigned int>>();
            threadIdFuture = threadIdPromise->get_future();
        }
    };
    
    // Processing methods
    void processOperationThread(std::shared_ptr<RequestContext> context);
    static void processOperationThreadStatic(std::shared_ptr<RequestContext> context);
    
    // Response handling
    void sendResponse(const std::string& transactionId, bool success, 
                      const std::string& result, const std::string& error = "");
    static void sendResponseStatic(const std::string& transactionId, bool success,
                                   const std::string& result, const std::string& error,
                                   const std::string& responseTopic);
    
    // Thread cleanup
    void cleanupCompletedThreads();
};

#endif // RPC_OPERATION_PROCESSOR_H
