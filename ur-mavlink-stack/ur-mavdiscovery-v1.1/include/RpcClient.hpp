#pragma once
#include "ur-rpc-template.hpp"
#include <ThreadManager.hpp>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <cstddef>
#include <thread>
#include <chrono>

class RpcClient {
public:
    RpcClient(const std::string& configPath, const std::string& clientId);
    ~RpcClient();

    // Lifecycle management
    bool start();
    void stop();
    bool isRunning() const;

    // Message handling configuration
    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);
    void sendResponse(const std::string& topic, const std::string& response);

    // Getters for internal components
    UrRpc::Client& getClient() { return *client_; }
    std::shared_ptr<ThreadMgr::ThreadManager> getThreadManager() { return threadManager_; }

private:
    // Thread management
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    unsigned int rpcThreadId_{0};
    
    // Internal state
    std::atomic<bool> running_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;
    
    // UR-RPC components
    std::unique_ptr<UrRpc::Library> library_;
    std::unique_ptr<UrRpc::Client> client_;
    std::string configPath_;
    std::string clientId_;
    
    // Core thread function
    void rpcClientThreadFunc();
    
    // Static callback for C interoperability
    static void staticMessageHandler(const char* topic, const char* payload, 
                                   size_t payload_len, void* user_data);
};
