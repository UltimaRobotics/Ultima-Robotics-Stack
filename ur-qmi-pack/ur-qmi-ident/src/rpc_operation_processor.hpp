#pragma once

#include <memory>
#include <string>
#include <set>
#include <mutex>
#include <atomic>
#include <future>
#include <map>
#include <nlohmann/json.hpp>

// Include the correct ThreadManager header from ur-threadder-api
extern "C" {
#include "thread_manager.h"
}

// Forward declaration for QMI scanner
class QMIScanner;

extern "C" {
#include "cJSON.h"
}

class RpcOperationProcessor {
public:
    RpcOperationProcessor(bool verbose);
    ~RpcOperationProcessor();

    // Process an incoming RPC request and send response
    void processRequest(const char* payload, size_t payload_len);
    
    // Set the response topic prefix
    void setResponseTopic(const std::string& topic);
    
    // Set the QMI scanner instance for device operations
    void setScanner(QMIScanner* scanner);

private:
    // Request context structure for passing data to thread
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string responseTopic;
        bool verbose;
        thread_manager_t* threadManager;  // Thread manager for cleanup
        std::set<unsigned int>* activeThreads;  // Pointer to active threads set
        std::mutex* threadsMutex;  // Pointer to mutex for thread-safe access
        std::atomic<unsigned int> threadId;  // ID of this thread (atomic for thread-safe access)
        std::shared_ptr<std::promise<unsigned int>> threadIdPromise;  // Promise for thread ID initialization
        std::shared_future<unsigned int> threadIdFuture;  // Future for thread to wait on
        QMIScanner* scanner;  // Scanner instance for device operations
        
        RequestContext(std::string json, std::string id, std::string topic, 
                      bool v, thread_manager_t* tm,
                      std::set<unsigned int>* at, std::mutex* mtx, QMIScanner* sc)
            : requestJson(std::move(json))
            , transactionId(std::move(id))
            , responseTopic(std::move(topic))
            , verbose(v)
            , threadManager(tm)
            , activeThreads(at)
            , threadsMutex(mtx)
            , threadId(0)
            , threadIdPromise(std::make_shared<std::promise<unsigned int>>())
            , threadIdFuture(threadIdPromise->get_future())
            , scanner(sc)
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
    
    bool verbose_;
    std::string responseTopic_;
    thread_manager_t threadManager_;
    bool threadManagerInitialized_{false};
    std::atomic<bool> isShuttingDown_{false};
    
    // Thread tracking
    std::set<unsigned int> activeThreads_;
    std::mutex threadsMutex_;
    
    // Scanner instance for device operations
    QMIScanner* scanner_{nullptr};
};
