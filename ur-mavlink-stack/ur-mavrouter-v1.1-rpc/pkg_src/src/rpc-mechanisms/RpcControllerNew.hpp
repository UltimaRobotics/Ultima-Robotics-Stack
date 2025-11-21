#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include "../../modules/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "RpcClientInterfaceNew.hpp"
#include "RpcOperations.hpp"
#include "../../ur-mavdiscovery-shared/include/MavlinkDeviceStructs.hpp"

// Forward declaration for ExtensionManager
namespace MavlinkExtensions {
    class ExtensionManager;
}

// Forward declaration for DeviceDiscoveryCronJob
class DeviceDiscoveryCronJob;

namespace RpcMechanisms {

/**
 * @brief RPC Controller that coordinates RPC client and operations
 * 
 * This class acts as a facade that combines the RPC client functionality
 * with the thread management operations.
 */
class RpcController {
public:
    /**
     * @brief Constructor
     * @param threadManager Reference to the thread manager
     */
    explicit RpcController(ThreadMgr::ThreadManager& threadManager);
    
    /**
     * @brief Destructor
     */
    ~RpcController();
    
    // Disable copy
    RpcController(const RpcController&) = delete;
    RpcController& operator=(const RpcController&) = delete;
    
    /**
     * @brief Initialize UR-RPC client integration
     * @param configPath Path to RPC configuration file
     * @param clientId RPC client identifier
     * @return true if successful, false otherwise
     */
    bool initializeRpcIntegration(const std::string& configPath, const std::string& clientId = "ur-mavrouter");

    /**
     * @brief Start UR-RPC client
     * @return true if successful, false otherwise
     */
    bool startRpcClient();

    /**
     * @brief Stop UR-RPC client
     */
    void stopRpcClient();

    /**
     * @brief Check if RPC client is running
     * @return true if running, false otherwise
     */
    bool isRpcClientRunning() const;

    /**
     * @brief Get RPC client statistics
     * @return JSON string with statistics
     */
    std::string getRpcClientStatistics() const;
    
    /**
     * @brief Register a thread for RPC control
     * @param threadName Logical name for the thread
     * @param threadId Thread ID from thread manager
     * @param attachmentId Attachment identifier
     */
    void registerThread(const std::string& threadName, 
                       unsigned int threadId,
                       const std::string& attachmentId);
    
    /**
     * @brief Unregister a thread
     * @param threadName Thread name
     */
    void unregisterThread(const std::string& threadName);
    
    /**
     * @brief Execute an RPC request
     * @param request RPC request structure
     * @return RPC response structure
     */
    RpcResponse executeRequest(const RpcRequest& request);
    
    /**
     * @brief Get status of all threads
     * @return RPC response with all thread states
     */
    RpcResponse getAllThreadStatus();
    
    /**
     * @brief Get status of a specific thread
     * @param threadName Thread name
     * @return RPC response with thread state
     */
    RpcResponse getThreadStatus(const std::string& threadName);
    
    /**
     * @brief Execute operation on a specific thread
     * @param threadName Thread name
     * @param operation Operation to execute
     * @return RPC response with operation result
     */
    RpcResponse executeOperationOnThread(const std::string& threadName, 
                                        ThreadOperation operation);
    
    /**
     * @brief Register a restart callback for a thread
     * @param threadName Thread name
     * @param restartCallback Function to call for restart
     */
    void registerRestartCallback(const std::string& threadName, 
                               std::function<unsigned int()> restartCallback);
    
    /**
     * @brief Set extension manager
     * @param extensionManager Pointer to extension manager
     */
    void setExtensionManager(MavlinkExtensions::ExtensionManager* extensionManager);
    
    /**
     * @brief Start thread for target
     * @param target Thread target
     * @return RPC response
     */
    RpcResponse startThread(ThreadTarget target);
    
    /**
     * @brief Stop thread for target
     * @param target Thread target
     * @return RPC response
     */
    RpcResponse stopThread(ThreadTarget target);
    
    /**
     * @brief Pause thread for target
     * @param target Thread target
     * @return RPC response
     */
    RpcResponse pauseThread(ThreadTarget target);
    
    /**
     * @brief Resume thread for target
     * @param target Thread target
     * @return RPC response
     */
    RpcResponse resumeThread(ThreadTarget target);
    
    /**
     * @brief Get access to the RPC client (for HTTP server)
     * @return Pointer to RPC client interface
     */
    RpcClientInterface* getRpcClient() { return rpcClient_.get(); }
    
    /**
     * @brief Get access to the RPC client wrapper (for internal use)
     * @return Pointer to RPC client wrapper
     */
    RpcClientWrapper* getRpcClientWrapper() { return dynamic_cast<RpcClientWrapper*>(rpcClient_.get()); }
    
    /**
     * @brief Get access to the operations (for HTTP server)
     * @return Pointer to operations
     */
    RpcOperations* getOperations() { return operations_.get(); }
    
    /**
     * @brief Get startup status information
     * @return JSON object with startup status
     */
    nlohmann::json getStartupStatus() const;
    
    /**
     * @brief Handle incoming RPC message (public for manual trigger)
     * @param topic Message topic
     * @param payload Message payload
     */
    void handleRpcMessage(const std::string& topic, const std::string& payload);

private:
    // RPC client and operations
    std::unique_ptr<RpcClientInterface> rpcClient_;
    std::unique_ptr<RpcOperations> operations_;
    
    // Configuration
    std::string rpcConfigPath_;
    std::string rpcClientId_;
    std::atomic<bool> rpcInitialized_{false};
    mutable std::mutex rpcMutex_;
    
    // Message handler for RPC client (used for temporary handler management)
    std::function<void(const std::string&, const std::string&)> messageHandler_;
    
    // UR-RPC private methods
    void setupRpcMessageHandlers();
    
    // Startup mechanism private methods
    void handleHeartbeatMessage(const std::string& payload);
    void triggerDeviceDiscovery();
    void handleDeviceDiscoveryResponse(const std::string& payload);
    void handleDeviceAddedEvent(const MavlinkShared::DeviceAddedEvent& event);
    bool isMainloopRunning() const;
    void checkHeartbeatTimeout();
    
    // Startup state tracking
    std::atomic<bool> discoveryTriggered_{false};
    std::atomic<bool> mainloopStarted_{false};
    std::chrono::steady_clock::time_point lastHeartbeatTime_;
    mutable std::mutex startupMutex_;
    
    // Thread management for startup mechanism
    std::vector<std::thread> startupThreads_;
    std::mutex threadsMutex_;
    std::atomic<bool> shutdown_{false};
    
    // Device discovery cron job (new implementation)
    std::unique_ptr<DeviceDiscoveryCronJob> discoveryCronJob_;
    
    // Heartbeat timeout configuration
    static constexpr std::chrono::seconds HEARTBEAT_TIMEOUT{30}; // 30 seconds timeout
};

} // namespace RpcMechanisms
