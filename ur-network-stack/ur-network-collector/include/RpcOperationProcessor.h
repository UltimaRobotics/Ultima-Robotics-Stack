#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <set>
#include <functional>
#include <future>
#include <nlohmann/json.hpp>

// Include ThreadManager for ur-threadder-api integration
#include "ThreadManager.hpp"

using json = nlohmann::json;

/**
 * @brief Configuration structure for network collector operations
 */
struct NetworkCollectorConfig {
    bool collectVlan = false;
    bool collectNat = false;
    bool collectFirewall = false;
    bool collectRoutes = false;
    bool collectBridges = false;
    bool collectAll = true;
    bool outputText = false;
    bool quietMode = false;
    std::string outputFile;
    int collectionInterval = 5; // seconds between collections
    bool enableMqttPublishing = false;
    std::string mqttRuntimeTopic = "ur-shared-bus/ur-network-stack/ur-net-collector/runtime";
    
    // Serialization support
    static NetworkCollectorConfig from_json(const json& j);
    json to_json() const;
};

/**
 * @brief RPC Operation Processor for network collector
 * 
 * This class handles concurrent processing of RPC requests for network
 * data collection operations, using the ur-threadder-api for thread management.
 */
class RpcOperationProcessor {
public:
    /**
     * @brief Constructor with configuration and verbosity settings
     * @param config Network collector configuration
     * @param verbose Enable verbose output
     */
    RpcOperationProcessor(const NetworkCollectorConfig& config, bool verbose = false);
    
    /**
     * @brief Destructor - ensures proper thread cleanup
     */
    ~RpcOperationProcessor();

    // Disable copy construction and assignment
    RpcOperationProcessor(const RpcOperationProcessor&) = delete;
    RpcOperationProcessor& operator=(const RpcOperationProcessor&) = delete;

    // Enable move construction and assignment
    RpcOperationProcessor(RpcOperationProcessor&& other) noexcept;
    RpcOperationProcessor& operator=(RpcOperationProcessor&& other) noexcept;

    /**
     * @brief Process incoming RPC request
     * @param payload Request payload (JSON-RPC 2.0)
     * @param payload_len Payload length
     */
    void processRequest(const char* payload, size_t payload_len);

    /**
     * @brief Set the response topic for RPC responses
     * @param topic MQTT topic for responses
     */
    void setResponseTopic(const std::string& topic);

    /**
     * @brief Set RPC client for sending responses
     * @param rpcClient Shared pointer to RPC client
     */
    void setRpcClient(std::shared_ptr<class RpcClient> rpcClient);

    /**
     * @brief Get current configuration
     * @return Current network collector configuration
     */
    NetworkCollectorConfig getConfig() const;

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void updateConfig(const NetworkCollectorConfig& config);

    /**
     * @brief Get number of active operation threads
     * @return Count of active threads
     */
    size_t getActiveThreadCount() const;

    /**
     * @brief Initiate graceful shutdown
     */
    void shutdown();

private:
    // Thread management for concurrent operations
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    std::set<unsigned int> activeThreads_;
    mutable std::mutex threadsMutex_;
    std::atomic<bool> isShuttingDown_{false};
    
    // Configuration and state
    NetworkCollectorConfig config_;
    bool verbose_;
    std::string responseTopic_;
    std::weak_ptr<class RpcClient> rpcClient_;

    /**
     * @brief Request context for thread-safe data passing
     */
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string responseTopic;
        std::shared_ptr<const NetworkCollectorConfig> config;
        bool verbose;
        std::weak_ptr<class RpcClient> rpcClient;
        
        // Thread synchronization primitives
        std::shared_ptr<std::promise<unsigned int>> threadIdPromise;
        std::shared_future<unsigned int> threadIdFuture;
        
        RequestContext(const std::string& reqJson, 
                      const std::string& transId,
                      const std::string& respTopic,
                      std::shared_ptr<const NetworkCollectorConfig> cfg,
                      bool verb,
                      std::weak_ptr<class RpcClient> client)
            : requestJson(reqJson)
            , transactionId(transId)
            , responseTopic(respTopic)
            , config(cfg)
            , verbose(verb)
            , rpcClient(client)
            , threadIdPromise(std::make_shared<std::promise<unsigned int>>())
            , threadIdFuture(threadIdPromise->get_future()) {}
    };

    /**
     * @brief Process operation in worker thread
     * @param context Shared request context
     */
    void processOperationThread(std::shared_ptr<RequestContext> context);

    /**
     * @brief Static entry point for worker thread
     * @param context Shared request context
     */
    static void processOperationThreadStatic(std::shared_ptr<RequestContext> context);

    /**
     * @brief Send RPC response
     * @param transactionId Transaction identifier
     * @param success Operation success status
     * @param result Operation result
     * @param error Error message (if any)
     */
    void sendResponse(const std::string& transactionId, bool success, 
                      const std::string& result, const std::string& error = "");

    /**
     * @brief Static response sender for worker threads
     * @param transactionId Transaction identifier
     * @param success Operation success status
     * @param result Operation result
     * @param error Error message (if any)
     * @param responseTopic Response topic
     * @param rpcClient Weak pointer to RPC client
     */
    static void sendResponseStatic(const std::string& transactionId, bool success,
                                   const std::string& result, const std::string& error,
                                   const std::string& responseTopic,
                                   std::weak_ptr<class RpcClient> rpcClient);

    /**
     * @brief Extract transaction ID from JSON-RPC request
     * @param request JSON request object
     * @return Transaction ID string
     */
    std::string extractTransactionId(const json& request);

    /**
     * @brief Execute network collection operation
     * @param method RPC method name
     * @param params Method parameters
     * @param config Operation configuration
     * @param verbose Verbose output flag
     * @return Operation result as JSON string
     */
    std::string executeNetworkOperation(const std::string& method,
                                        const json& params,
                                        const NetworkCollectorConfig& config,
                                        bool verbose);

    /**
     * @brief Cleanup completed threads from tracking
     */
    void cleanupCompletedThreads();

    /**
     * @brief Remove thread from active tracking
     * @param threadId Thread identifier to remove
     */
    void removeThreadFromTracking(unsigned int threadId);
};
