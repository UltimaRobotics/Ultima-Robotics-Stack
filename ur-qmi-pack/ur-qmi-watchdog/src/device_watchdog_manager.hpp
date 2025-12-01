#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "qmi_watchdog.h"
#include "ThreadManager.hpp"

// Forward declaration
class RpcClient;

struct DeviceWatchdogInfo {
    std::unique_ptr<QMIWatchdog> watchdog;
    unsigned int thread_id;
    std::string device_path;
    bool is_running;
    
    DeviceWatchdogInfo() : thread_id(0), is_running(false) {}
};

class DeviceWatchdogManager {
public:
    DeviceWatchdogManager();
    ~DeviceWatchdogManager();
    
    // Start monitoring for a specific device
    bool startDeviceMonitoring(const std::string& device_path, 
                              const Json::Value& monitoring_config,
                              const Json::Value& failure_detection_config);
    
    // Stop monitoring for a specific device
    bool stopDeviceMonitoring(const std::string& device_path);
    
    // Check if device is being monitored
    bool isDeviceMonitored(const std::string& device_path) const;
    
    // Stop all device monitoring
    void stopAllMonitoring();
    
    // Get list of monitored devices
    std::vector<std::string> getMonitoredDevices() const;
    
    // Set RPC client for publishing data
    void setRpcClient(std::shared_ptr<RpcClient> rpc_client);
    
    // Set verbose mode for all watchdog instances
    void setVerbose(bool verbose);
    
private:
    mutable std::mutex devices_mutex_;
    std::unordered_map<std::string, std::unique_ptr<DeviceWatchdogInfo>> monitored_devices_;
    ThreadMgr::ThreadManager* thread_manager_;
    std::shared_ptr<RpcClient> rpc_client_;
    bool verbose_ = false;
    
    void watchdogThreadFunction(const std::string& device_path,
                                const Json::Value& monitoring_config,
                                const Json::Value& failure_detection_config);
};
