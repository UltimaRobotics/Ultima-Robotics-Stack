#pragma once
#include "DeviceMonitor.hpp"
#include "DeviceVerifier.hpp"
#include "ConfigLoader.hpp"
#include "RpcClient.hpp"
#include "RpcOperationProcessor.hpp"
#include "../ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "../thirdparty/nholmann/json.hpp"
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    bool initialize(const std::string& configPath = "");
    void run();
    void shutdown();
    
    // RPC-related methods
    bool initializeRpc(const std::string& rpcConfigPath = "");
    void shutdownRpc();
    bool isRpcRunning() const;

private:
    void onDeviceAdded(const std::string& devicePath);
    void onDeviceRemoved(const std::string& devicePath);
    void onDeviceVerified(const std::string& devicePath, const DeviceInfo& info);
    void sendDeviceAddedRpcNotifications(const DeviceInfo& info);
    void sendDeviceRemovedRpcNotifications(const std::string& devicePath);
    
    // RPC message handling
    void onRpcMessage(const std::string& topic, const std::string& payload);
    void setupRpcMessageHandler();

    ConfigLoader config_;
    std::unique_ptr<DeviceMonitor> monitor_;
    std::unique_ptr<ThreadMgr::ThreadManager> threadManager_;
    std::map<std::string, std::unique_ptr<DeviceVerifier>> verifiers_;
    std::mutex verifiersMutex_;
    std::atomic<bool> running_;
    
    // RPC components
    std::unique_ptr<RpcClient> rpcClient_;
    std::unique_ptr<RpcOperationProcessor> operationProcessor_;
    std::atomic<bool> rpcRunning_;
};