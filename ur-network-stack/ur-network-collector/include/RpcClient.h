#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

// Include ThreadManager for ur-threadder-api integration
#include "ThreadManager.hpp"

/**
 * @brief RPC Client for network collector using ur-rpc-template
 * 
 * This class provides a C++ wrapper around the ur-rpc-template library,
 * enabling MQTT-based RPC communication with thread-safe operations.
 * It follows the same patterns as ur-licence-mann implementation.
 */
class RpcClient {
public:
    /**
     * @brief Constructor with RPC configuration path and client identifier
     * @param configPath Path to RPC configuration JSON file
     * @param clientId Unique identifier for the MQTT client connection
     */
    RpcClient(const std::string& configPath, const std::string& clientId);
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~RpcClient();

    // Disable copy construction and assignment
    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;

    // Enable move construction and assignment
    RpcClient(RpcClient&& other) noexcept;
    RpcClient& operator=(RpcClient&& other) noexcept;

    /**
     * @brief Start the RPC client in a separate thread
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the RPC client and cleanup resources
     */
    void stop();

    /**
     * @brief Check if the RPC client is currently running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Set message handler for application-specific processing
     * @param handler Function to handle incoming RPC messages
     */
    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);

    /**
     * @brief Send response back to RPC requester
     * @param topic MQTT topic to send response to
     * @param response JSON response string
     */
    void sendResponse(const std::string& topic, const std::string& response);

    /**
     * @brief Get the client ID
     * @return Client identifier string
     */
    const std::string& getClientId() const;

    /**
     * @brief Get the configuration path
     * @return Configuration file path
     */
    const std::string& getConfigPath() const;

private:
    std::string configPath_;
    std::string clientId_;
    std::atomic<bool> running_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;

    // ThreadManager for managing the RPC client thread
    std::unique_ptr<ThreadMgr::ThreadManager> threadManager_;
    unsigned int rpcThreadId_{0};

    /**
     * @brief Thread function for RPC client operation
     */
    void rpcClientThreadFunc();

    /**
     * @brief Static message handler for C callback interface
     * @param topic MQTT topic
     * @param payload Message payload
     * @param payload_len Payload length
     * @param user_data User data pointer (this instance)
     */
    static void staticMessageHandler(const char* topic, const char* payload, 
                                     size_t payload_len, void* user_data);
};
