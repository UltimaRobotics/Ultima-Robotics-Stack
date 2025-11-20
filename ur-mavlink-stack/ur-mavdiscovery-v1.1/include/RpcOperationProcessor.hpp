#pragma once
#include "../ur-rpc-template/pkg_src/api/wrappers/cpp/ur-rpc-template.hpp"
#include "ConfigLoader.hpp"
#include "RpcClient.hpp"
#include "../ur-threadder-api/cpp/include/ThreadManager.hpp"
#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include <set>
#include <mutex>
#include <atomic>
#include <future>
#include <cstddef>

using json = nlohmann::json;

class RpcOperationProcessor {
public:
    // Constructor with configuration, RPC client, and verbosity settings
    RpcOperationProcessor(const DeviceConfig& config, RpcClient& rpcClient, bool verbose = false);
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
    
    // Configuration
    DeviceConfig config_;
    bool verbose_;
    std::string responseTopic_;
    
    // RPC client reference for sending responses (owned by DeviceManager)
    RpcClient* rpcClient_;
    
    // Request context for thread-safe data passing
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string responseTopic;
        std::shared_ptr<const DeviceConfig> config;
        bool verbose;
        RpcOperationProcessor* processor; // Pointer to processor instance
        // Thread synchronization primitives
        std::shared_ptr<std::promise<unsigned int>> threadIdPromise;
        std::shared_future<unsigned int> threadIdFuture;
    };
    
    // Processing methods
    void processOperationThread(std::shared_ptr<RequestContext> context);
    static void processOperationThreadStatic(std::shared_ptr<RequestContext> context);
    
    // Response handling
    void sendResponse(const std::string& transactionId, bool success, 
                      const std::string& result, const std::string& error = "");
    static void sendResponseStatic(const std::string& transactionId, bool success,
                                   const std::string& result, const std::string& error,
                                   const std::string& responseTopic, RpcClient* rpcClient);
    
    // Utility methods
    std::string extractTransactionId(const json& request);
    json buildErrorResponse(const std::string& transactionId, const std::string& error);
    json buildSuccessResponse(const std::string& transactionId, const std::string& result);
    
    // Operation handlers
    int handleDeviceList(const json& params, std::string& result);
    int handleDeviceInfo(const json& params, std::string& result);
    int handleDeviceVerify(const json& params, std::string& result);
    int handleDeviceStatus(const json& params, std::string& result);
    int handleSystemInfo(const json& params, std::string& result);
};
