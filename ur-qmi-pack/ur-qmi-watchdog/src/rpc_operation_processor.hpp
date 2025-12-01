#pragma once

#include <memory>
#include <string>
#include <set>
#include <mutex>
#include <atomic>
#include <functional>
#include <ThreadManager.hpp>

// Forward declaration for minimal context
struct RequestContext {
    std::string requestJson;
    std::string transactionId;
    std::string responseTopic;
    bool verbose;
    
    RequestContext(const std::string& req, const std::string& transId, 
                   const std::string& respTopic, bool verb)
        : requestJson(req), transactionId(transId), responseTopic(respTopic), verbose(verb) {}
};

class RpcOperationProcessor {
public:
    // Constructor with verbosity settings
    RpcOperationProcessor(bool verbose = false);
    ~RpcOperationProcessor();
    
    // Core processing interface
    void processRequest(const char* payload, size_t payload_len);
    void setResponseTopic(const std::string& topic);
    
private:
    // Thread management for concurrent operations
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    std::set<unsigned int> activeThreads_;
    std::mutex threadsMutex_;
    std::atomic<bool> isShuttingDown_{false};
    std::string responseTopic_;
    bool verbose_;
    
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
    void cleanupThreadTracking(unsigned int threadId);
};
