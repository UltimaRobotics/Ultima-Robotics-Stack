#ifndef RPC_CLIENT_HPP
#define RPC_CLIENT_HPP

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include "../thirdparty/nlohmann/json.hpp"
#include "../thirdparty/ur-rpc-template/extensions/direct_template.h"
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"

using json = nlohmann::json;
using namespace ThreadMgr;

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
    bool isConnected() const;
    
    // Message handling configuration
    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);
    
    // Response transmission
    void sendResponse(const std::string& topic, const std::string& response);
    
    // Message publishing for status updates
    void publishMessage(const std::string& topic, const std::string& message);
    
    // Get client ID
    const std::string& getClientId() const { return clientId_; }
    
    // Get statistics
    void getStatistics(direct_client_statistics_t* stats) const;
    
private:
    // Configuration
    std::string configPath_;
    std::string clientId_;
    
    // Thread management
    std::unique_ptr<ThreadManager> threadManager_;
    unsigned int rpcThreadId_{0};
    
    // Internal state
    std::atomic<bool> running_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;
    
    // Direct client thread context
    direct_client_thread_t* ctx_{nullptr};
    
    // Core thread function
    void rpcClientThreadFunc();
    
    // Static callback for C interoperability
    static void staticMessageHandler(const char* topic, const char* payload, 
                                   size_t payload_len, void* user_data);
    
    // Utility methods
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
};

#endif // RPC_CLIENT_HPP
