#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

// Include the correct ThreadManager header from ur-threadder-api
extern "C" {
#include "thread_manager.h"
}

class RpcClient {
public:
    // Construct with path to RPC config JSON and optional clientId override
    RpcClient(const std::string& configPath, const std::string& clientId);
    ~RpcClient();

    // Start the RPC client in a separate thread
    bool start();
    void stop();

    bool isRunning() const;

    // Message handler hook for application-specific processing
    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);

    // Send response back to requester
    void sendResponse(const std::string& topic, const std::string& response);
    
    // Publish message to arbitrary topic
    void publishMessage(const std::string& topic, const std::string& message);

private:
    std::string configPath_;
    std::string clientId_;
    std::atomic<bool> running_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;

    // ThreadManager for managing the RPC client thread using C API
    thread_manager_t threadManager_;
    unsigned int rpcThreadId_{0};
    
    // Flag to track if thread manager is initialized
    bool threadManagerInitialized_{false};

    // Thread function for RPC client
    void rpcClientThreadFunc();

    static void staticMessageHandler(const char* topic, const char* payload, size_t payload_len, void* user_data);
};
