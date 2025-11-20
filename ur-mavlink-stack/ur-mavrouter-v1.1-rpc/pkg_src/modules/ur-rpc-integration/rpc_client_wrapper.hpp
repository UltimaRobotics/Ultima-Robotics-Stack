#ifndef RPC_CLIENT_WRAPPER_HPP
#define RPC_CLIENT_WRAPPER_HPP

#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include "ur-rpc-template.h"
#include "direct_template.h"
#include "../ur-threadder-api/cpp/include/ThreadManager.hpp"

namespace URRpcIntegration {

/**
 * @brief RPC Client wrapper that integrates UR-RPC Template with ur-threadder-api
 * 
 * This class provides a modern C++ interface for RPC communication using the
 * UR-RPC Template while leveraging the ur-threadder-api for thread management.
 */
class RpcClientWrapper {
public:
    using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;
    using ConnectionStatusCallback = std::function<void(bool connected, const std::string& reason)>;

    /**
     * @brief Constructor
     * @param configPath Path to RPC configuration JSON file
     * @param clientId Unique client identifier
     * @param threadManager Reference to ThreadManager instance
     */
    RpcClientWrapper(const std::string& configPath, 
                     const std::string& clientId,
                     ThreadMgr::ThreadManager& threadManager);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~RpcClientWrapper();

    /**
     * @brief Initialize the RPC client
     * @return true if successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Start the RPC client
     * @return true if successful, false otherwise
     */
    bool start();

    /**
     * @brief Stop the RPC client
     */
    void stop();

    /**
     * @brief Check if the client is running
     * @return true if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Set message handler for incoming messages
     * @param handler Message handler function
     */
    void setMessageHandler(MessageHandler handler);

    /**
     * @brief Send a response message
     * @param topic Topic to publish to
     * @param response Response message content
     */
    void sendResponse(const std::string& topic, const std::string& response);

    /**
     * @brief Send an RPC request
     * @param method RPC method name
     * @param service Target service
     * @param params JSON parameters string
     * @param authority Authority level
     * @return true if sent successfully, false otherwise
     */
    bool sendRequest(const std::string& method, 
                    const std::string& service,
                    const std::string& params = "",
                    int authority = UR_RPC_AUTHORITY_USER);

    /**
     * @brief Set connection status callback
     * @param callback Function to call on connection status changes
     */
    void setConnectionStatusCallback(ConnectionStatusCallback callback);

    /**
     * @brief Get current connection status
     * @return Connection status enum value
     */
    ur_rpc_connection_status_t getConnectionStatus() const;

    /**
     * @brief Get client statistics
     * @return JSON object with statistics
     */
    nlohmann::json getStatistics() const;

private:
    // Configuration and identification
    std::string configPath_;
    std::string clientId_;
    
    // Thread management
    ThreadMgr::ThreadManager& threadManager_;
    unsigned int rpcThreadId_{0};
    
    // UR-RPC Template client
    ur_rpc_client_t* client_{nullptr};
    direct_client_thread_t* clientThread_{nullptr};
    
    // State management
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    mutable std::mutex clientMutex_;
    
    // Callbacks
    MessageHandler messageHandler_;
    ConnectionStatusCallback connectionCallback_;
    
    // Synchronization
    std::condition_variable readyCondition_;
    std::mutex readyMutex_;
    bool ready_{false};

    // Private methods
    void rpcClientThreadFunc();
    bool createClientContext();
    void destroyClientContext();
    void waitForConnection();
    void handleConnectionStatus(bool connected, const std::string& reason);

    // Static callback for C interoperability
    static void staticMessageHandler(const char* topic, const char* payload, 
                                     size_t payload_len, void* user_data);
    static void staticConnectionHandler(bool connected, const char* reason, void* user_data);
};

/**
 * @brief Factory class for creating RPC client instances
 */
class RpcClientFactory {
public:
    /**
     * @brief Create a new RPC client wrapper instance
     * @param configPath Path to configuration file
     * @param clientId Client identifier
     * @param threadManager Thread manager reference
     * @return Shared pointer to RPC client wrapper
     */
    static std::shared_ptr<RpcClientWrapper> create(const std::string& configPath,
                                                     const std::string& clientId,
                                                     ThreadMgr::ThreadManager& threadManager);
};

/**
 * @brief Configuration loader for RPC client
 */
class RpcConfigLoader {
public:
    /**
     * @brief Load RPC configuration from JSON file
     * @param configPath Path to configuration file
     * @return JSON configuration object
     * @throws std::runtime_error if configuration cannot be loaded
     */
    static nlohmann::json loadConfig(const std::string& configPath);

    /**
     * @brief Validate RPC configuration
     * @param config JSON configuration object
     * @return true if valid, false otherwise
     */
    static bool validateConfig(const nlohmann::json& config);

    /**
     * @brief Create default RPC configuration
     * @param clientId Client identifier
     * @return Default configuration JSON object
     */
    static nlohmann::json createDefaultConfig(const std::string& clientId);
};

} // namespace URRpcIntegration

#endif // RPC_CLIENT_WRAPPER_HPP
