#ifndef RPC_OPERATION_PROCESSOR_HPP
#define RPC_OPERATION_PROCESSOR_HPP

#include <memory>
#include <string>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <atomic>
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include "../../modules/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "rpc_client_wrapper.hpp"

// Forward declarations
namespace RpcMechanisms {
    class RpcController;
}

namespace MavlinkExtensions {
    class ExtensionManager;
}

namespace URRpcIntegration {

/**
 * @brief Operation processor for handling RPC requests with thread management
 * 
 * This class processes incoming RPC requests by parsing them, creating
 * appropriate operation contexts, and executing them in separate threads
 * managed by the ur-threadder-api.
 */
class RpcOperationProcessor {
public:
    /**
     * @brief Constructor
     * @param threadManager Reference to ThreadManager instance
     * @param routerConfigPath Path to the router configuration file
     * @param verbose Enable verbose logging
     */
    RpcOperationProcessor(ThreadMgr::ThreadManager& threadManager, const std::string& routerConfigPath = "", bool verbose = false);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~RpcOperationProcessor();

    /**
     * @brief Process an incoming RPC request
     * @param payload Request payload (JSON-RPC 2.0 format)
     * @param payload_len Length of payload
     */
    void processRequest(const char* payload, size_t payload_len);

    /**
     * @brief Set the response topic for outgoing responses
     * @param topic Response topic
     */
    void setResponseTopic(const std::string& topic);

    /**
     * @brief Set RPC client wrapper for sending responses
     * @param rpcClient Shared pointer to RPC client
     */
    void setRpcClient(std::shared_ptr<RpcClientWrapper> rpcClient);

    /**
     * @brief Register a custom operation handler
     * @param method Method name
     * @param handler Handler function
     */
    void registerOperationHandler(const std::string& method, 
                                 std::function<int(const nlohmann::json&, const std::string&, bool)> handler);

    /**
     * @brief Get processor statistics
     * @return JSON object with statistics
     */
    nlohmann::json getStatistics() const;

    /**
     * @brief Shutdown the processor and clean up resources
     */
    void shutdown();

    /**
     * @brief Set RPC controller for thread management operations
     * @param rpcController Pointer to RPC controller
     */
    void setRpcController(RpcMechanisms::RpcController* rpcController);
    
    /**
     * @brief Set extension manager for extension operations
     * @param extensionManager Pointer to extension manager
     */
    void setExtensionManager(MavlinkExtensions::ExtensionManager* extensionManager);

private:
    // Thread management
    ThreadMgr::ThreadManager& threadManager_;
    std::set<unsigned int> activeThreads_;
    mutable std::mutex threadsMutex_;
    std::atomic<bool> isShuttingDown_{false};

    // Configuration
    bool verbose_;
    std::string routerConfigPath_;  // Path to router configuration file
    std::string responseTopic_;
    std::shared_ptr<RpcClientWrapper> rpcClient_;

    // External components for operation handling
    RpcMechanisms::RpcController* rpcController_{nullptr};
    MavlinkExtensions::ExtensionManager* extensionManager_{nullptr};
    
    // Operation handlers
    std::map<std::string, std::function<int(const nlohmann::json&, const std::string&, bool)>> operationHandlers_;
    std::mutex handlersMutex_;

    // Statistics
    std::atomic<uint64_t> requestsProcessed_{0};
    std::atomic<uint64_t> requestsSuccessful_{0};
    std::atomic<uint64_t> requestsFailed_{0};
    std::atomic<uint64_t> activeRequests_{0};

    /**
     * @brief Request context for thread-safe data passing
     */
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string method;
        nlohmann::json params;
        std::string responseTopic;
        bool verbose;
        
        RequestContext() : verbose(false) {}
    };

    // Private methods
    void processOperationThread(std::shared_ptr<RequestContext> context);
    void processOperationThreadWithInstance(std::shared_ptr<RequestContext> context);
    static void processOperationThreadStatic(std::shared_ptr<RequestContext> context);

    std::string extractTransactionId(const nlohmann::json& request);
    std::shared_ptr<RequestContext> createRequestContext(const nlohmann::json& request);
    void launchProcessingThread(std::shared_ptr<RequestContext> context);
    void sendResponse(const std::string& transactionId, bool success, 
                      const std::string& result, const std::string& error = "");
    void cleanupThreadTracking(unsigned int threadId);

    // Built-in operation handlers
    int handleGetStatus(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleGetMetrics(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleRouterControl(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleEndpointInfo(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleConfigUpdate(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    
    // Thread management operations
    int handleGetAllThreads(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleGetMainloopThread(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleStartMainloop(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleStopMainloop(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handlePauseMainloop(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleResumeMainloop(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    
    // Extension management operations
    int handleGetAllExtensions(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleGetExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleAddExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleStartExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleStopExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose);
    int handleDeleteExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose);

    // Initialize built-in handlers
    void initializeBuiltInHandlers();
};

/**
 * @brief Helper class for JSON-RPC 2.0 response formatting
 */
class JsonResponseBuilder {
public:
    /**
     * @brief Create a successful JSON-RPC response
     * @param transactionId Transaction ID
     * @param result Result data
     * @return JSON response string
     */
    static std::string createSuccessResponse(const std::string& transactionId, const nlohmann::json& result);

    /**
     * @brief Create an error JSON-RPC response
     * @param transactionId Transaction ID
     * @param errorCode Error code
     * @param errorMessage Error message
     * @return JSON response string
     */
    static std::string createErrorResponse(const std::string& transactionId, int errorCode, const std::string& errorMessage);

    /**
     * @brief Parse JSON-RPC request and validate format
     * @param payload Request payload
     * @return Parsed JSON object, empty if invalid
     */
    static nlohmann::json parseAndValidateRequest(const std::string& payload);
};

} // namespace URRpcIntegration

#endif // RPC_OPERATION_PROCESSOR_HPP
