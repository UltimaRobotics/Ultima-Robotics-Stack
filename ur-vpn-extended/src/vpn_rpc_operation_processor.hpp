#ifndef VPN_RPC_OPERATION_PROCESSOR_HPP
#define VPN_RPC_OPERATION_PROCESSOR_HPP

#include "vpn_instance_manager.hpp"
#include "ThreadManager.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <set>
#include <future>

namespace vpn_manager {

/**
 * @brief RPC Operation Processor for VPN Instance Manager
 * 
 * This class handles concurrent processing of RPC requests for VPN operations
 * following the patterns from the ur-rpc-template-integration-guide.md.
 */
class VpnRpcOperationProcessor {
public:
    /**
     * @brief Constructor
     * @param manager Reference to VPN instance manager
     * @param verbose Enable verbose logging
     */
    VpnRpcOperationProcessor(VPNInstanceManager& manager, bool verbose = false);
    
    /**
     * @brief Destructor
     */
    ~VpnRpcOperationProcessor();

    /**
     * @brief Process incoming RPC request
     * @param payload Request payload
     * @param payload_len Payload length
     */
    void processRequest(const char* payload, size_t payload_len);
    
    /**
     * @brief Set response topic for sending responses
     * @param topic Response topic
     */
    void setResponseTopic(const std::string& topic);

private:
    // Thread management for concurrent operations
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    std::set<unsigned int> activeThreads_;
    std::mutex threadsMutex_;
    std::atomic<bool> isShuttingDown_{false};
    
    // VPN Manager reference
    VPNInstanceManager& vpnManager_;
    bool verbose_;
    
    // Response topic
    std::string responseTopic_;
    
    // Request context for thread-safe data passing
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string responseTopic;
        VPNInstanceManager* vpnManager;
        VpnRpcOperationProcessor* processor;
        bool verbose;
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
                                   const std::string& responseTopic);
    
    // Operation handlers
    nlohmann::json handleParse(const nlohmann::json& params);
    nlohmann::json handleAdd(const nlohmann::json& params);
    nlohmann::json handleDelete(const nlohmann::json& params);
    nlohmann::json handleUpdate(const nlohmann::json& params);
    nlohmann::json handleStart(const nlohmann::json& params);
    nlohmann::json handleStop(const nlohmann::json& params);
    nlohmann::json handleRestart(const nlohmann::json& params);
    nlohmann::json handleEnable(const nlohmann::json& params);
    nlohmann::json handleDisable(const nlohmann::json& params);
    nlohmann::json handleStatus(const nlohmann::json& params);
    nlohmann::json handleList(const nlohmann::json& params);
    nlohmann::json handleStats(const nlohmann::json& params);
    nlohmann::json handleAddCustomRoute(const nlohmann::json& params);
    nlohmann::json handleUpdateCustomRoute(const nlohmann::json& params);
    nlohmann::json handleDeleteCustomRoute(const nlohmann::json& params);
    nlohmann::json handleListCustomRoutes(const nlohmann::json& params);
    nlohmann::json handleGetCustomRoute(const nlohmann::json& params);
    nlohmann::json handleGetInstanceRoutes(const nlohmann::json& params);
    nlohmann::json handleAddInstanceRoute(const nlohmann::json& params);
    nlohmann::json handleDeleteInstanceRoute(const nlohmann::json& params);
    nlohmann::json handleApplyInstanceRoutes(const nlohmann::json& params);
    nlohmann::json handleDetectInstanceRoutes(const nlohmann::json& params);
    
    // Utility methods
    std::string extractTransactionId(const nlohmann::json& request);
    std::shared_ptr<RequestContext> createContext(const nlohmann::json& request, 
                                                   const std::string& transactionId);
    void launchProcessingThread(std::shared_ptr<RequestContext> context);
    void cleanupThreadTracking(unsigned int threadId, std::shared_ptr<RequestContext> context);
};

} // namespace vpn_manager

#endif // VPN_RPC_OPERATION_PROCESSOR_HPP
