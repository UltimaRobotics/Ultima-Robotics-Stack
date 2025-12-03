#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>

extern "C" {
#include "../ur-rpc-template/extensions/direct_template.h"
}

#include "../core/ThreadManager.hpp"

class RpcClient {
public:
    // Constructor with configuration path and client identifier
    RpcClient(const std::string& configPath, const std::string& clientId);
    
    // Destructor
    ~RpcClient();
    
    // Lifecycle management
    bool start();
    void stop();
    bool isRunning() const;
    
    // Message handling configuration
    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);
    
    // Message transmission
    void sendResponse(const std::string& topic, const std::string& response);
    void publishMessage(const std::string& topic, const std::string& message);

private:
    // Configuration
    std::string configPath_;
    std::string clientId_;
    
    // Thread management
    std::unique_ptr<ThreadMgr::ThreadManager> threadManager_;
    unsigned int rpcThreadId_{0};
    
    // Internal state
    std::atomic<bool> running_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;
    
    // Core thread function
    void rpcClientThreadFunc();
    
    // Static callback for C interoperability
    static void staticMessageHandler(const char* topic, const char* payload, 
                                   size_t payload_len, void* user_data);
};

#endif // RPC_CLIENT_H
