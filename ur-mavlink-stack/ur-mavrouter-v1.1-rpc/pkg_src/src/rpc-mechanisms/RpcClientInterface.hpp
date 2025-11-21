#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include "../../modules/nholmann/json.hpp"

// Include ur-mavrouter's ThreadManager
#include "../../modules/ur-threadder-api/cpp/include/ThreadManager.hpp"

extern "C" {
#include "../../../ur-rpc-template/ur-rpc-template.h"
}

// Include C++ wrapper
#include "../../../ur-rpc-template/pkg_src/api/wrappers/cpp/ur-rpc-template.hpp"

/**
 * @brief RPC Client implementation using ur-rpc-template C++ wrapper with fixed topics
 * 
 * This class provides RPC client functionality for ur-mavrouter using the modern C++ wrapper
 * of ur-rpc-template with fixed MQTT topics (no transaction IDs in topics).
 */
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

    // Send response back to requester (for notifications)
    void sendResponse(const std::string& topic, const std::string& response);
    
    // Send RPC request using ur-rpc-template C++ wrapper with fixed topics
    void sendRpcRequest(const std::string& service, const std::string& method, const std::string& paramsJson);

    // Getters for internal components
    UrRpc::Client* getUrRpcClient() { return urpcClient_.get(); }
    std::shared_ptr<ThreadMgr::ThreadManager> getThreadManager() { return threadManager_; }

    // Generate transaction ID for JSON-RPC requests
    std::string generateTransactionId();

private:
    std::string configPath_;
    std::string clientId_;
    std::atomic<bool> running_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;

    // ThreadManager for managing the RPC client thread
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    unsigned int rpcThreadId_{0};

    // UR-RPC components (C++ API)
    std::unique_ptr<UrRpc::Client> urpcClient_;
    std::unique_ptr<UrRpc::TopicConfig> topicConfig_;
    std::unique_ptr<UrRpc::ClientConfig> clientConfig_;

    // Internal methods
    void rpcClientThreadFunction();
    bool loadConfiguration();
    void setupTopicConfiguration();
};
