#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <ThreadManager.hpp>

extern "C" {
#include "ur-rpc-template.h"
}

#include "ur-rpc-template.hpp"

class RpcClientThread {
public:
    RpcClientThread(ThreadMgr::ThreadManager* threadManager, 
                   const std::string& rpcConfigPath,
                   const std::string& clientId = "ur-mavcollector");
    ~RpcClientThread();

    bool start();
    void stop();
    bool isRunning() const;

    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);
    void sendResponse(const std::string& topic, const std::string& response);
    void sendRpcRequest(const std::string& service, const std::string& method, const std::string& paramsJson);

    UrRpc::Client* getUrRpcClient() { return urpcClient_.get(); }
    ThreadMgr::ThreadManager* getThreadManager() { return threadManager_; }

private:
    std::string rpcConfigPath_;
    std::string clientId_;
    std::atomic<bool> running_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;

    ThreadMgr::ThreadManager* threadManager_;
    unsigned int rpcThreadId_{0};

    std::unique_ptr<UrRpc::Client> urpcClient_;
    std::unique_ptr<UrRpc::ClientConfig> clientConfig_;
    std::unique_ptr<UrRpc::TopicConfig> topicConfig_;

    void rpcClientThreadFunc();
    std::string generateTransactionId();
    static void messageCallbackWrapper(const std::string& topic, const std::string& payload);
    mutable std::mutex mutex_;
};
