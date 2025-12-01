#include "device_watchdog_manager.hpp"
#include "ThreadManager.hpp"
#include "rpc_client.hpp"
#include <iostream>
#include <thread>

DeviceWatchdogManager::DeviceWatchdogManager() {
    thread_manager_ = new ThreadMgr::ThreadManager(20); // Support multiple device threads
}

DeviceWatchdogManager::~DeviceWatchdogManager() {
    stopAllMonitoring();
    delete thread_manager_;
}

bool DeviceWatchdogManager::startDeviceMonitoring(const std::string& device_path, 
                                                  const Json::Value& monitoring_config,
                                                  const Json::Value& failure_detection_config) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    // Check if device is already being monitored
    if (monitored_devices_.find(device_path) != monitored_devices_.end()) {
        std::cout << "[DeviceManager] Device " << device_path << " is already being monitored" << std::endl;
        return true;
    }
    
    // Create device info
    auto device_info = std::make_unique<DeviceWatchdogInfo>();
    device_info->device_path = device_path;
    device_info->watchdog = std::make_unique<QMIWatchdog>();
    
    // Set RPC client for publishing data
    if (rpc_client_) {
        device_info->watchdog->setRpcClient(rpc_client_);
    }
    
    // Set verbose mode
    device_info->watchdog->setVerbose(verbose_);
    
    // Configure the watchdog with device settings
    // Create a simple device config based on the received device data
    DeviceConfig device_config;
    device_config.device_path = device_path;
    device_config.timeout_ms = monitoring_config.get("timeout_ms", 3000).asInt();
    device_config.collection_interval_ms = monitoring_config.get("collection_interval_ms", 2000).asInt();
    device_config.enable_health_scoring = monitoring_config.get("enable_health_scoring", true).asBool();
    
    // Set health weights if available
    if (monitoring_config.isMember("health_weights")) {
        const Json::Value& weights = monitoring_config["health_weights"];
        device_config.signal_weight = weights.get("signal_weight", 0.5).asDouble();
        device_config.network_weight = weights.get("network_weight", 0.35).asDouble();
        device_config.rf_weight = weights.get("rf_weight", 0.15).asDouble();
    }
    
    // Set failure detection thresholds
    if (failure_detection_config.isMember("critical_rssi_threshold")) {
        device_config.critical_rssi_threshold = failure_detection_config["critical_rssi_threshold"].asInt();
    }
    if (failure_detection_config.isMember("warning_rssi_threshold")) {
        device_config.warning_rssi_threshold = failure_detection_config["warning_rssi_threshold"].asInt();
    }
    if (failure_detection_config.isMember("max_consecutive_failures")) {
        device_config.max_consecutive_failures = failure_detection_config["max_consecutive_failures"].asInt();
    }
    
    // Set the device configuration
    if (!device_info->watchdog->setDeviceConfig(device_config)) {
        std::cerr << "[DeviceManager] Failed to set device configuration for " << device_path << std::endl;
        return false;
    }
    
    // Set failure detection callback
    device_info->watchdog->setFailureDetectionCallback([](const std::string& event_type, const std::vector<std::string>& failures) {
        std::cout << "\n!!! DEVICE FAILURE DETECTED !!!\n";
        std::cout << "Event: " << event_type << "\n";
        for (const auto& failure : failures) {
            std::cout << "- " << failure << "\n";
        }
    });
    
    // Create monitoring thread
    auto thread_func = [this, device_path, monitoring_config, failure_detection_config]() {
        this->watchdogThreadFunction(device_path, monitoring_config, failure_detection_config);
    };
    
    device_info->thread_id = thread_manager_->createThread(thread_func);
    
    if (device_info->thread_id == 0) {
        std::cerr << "[DeviceManager] Failed to create monitoring thread for " << device_path << std::endl;
        return false;
    }
    
    device_info->is_running = true;
    monitored_devices_[device_path] = std::move(device_info);
    
    std::cout << "[DeviceManager] Started monitoring for device: " << device_path 
              << " (Thread ID: " << monitored_devices_[device_path]->thread_id << ")" << std::endl;
    
    return true;
}

bool DeviceWatchdogManager::stopDeviceMonitoring(const std::string& device_path) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = monitored_devices_.find(device_path);
    if (it == monitored_devices_.end()) {
        std::cout << "[DeviceManager] Device " << device_path << " is not being monitored" << std::endl;
        return false;
    }
    
    auto& device_info = it->second;
    
    std::cout << "[DeviceManager] Stopping monitoring for device: " << device_path << std::endl;
    
    // Stop the watchdog
    if (device_info->watchdog) {
        device_info->watchdog->stopMonitoring();
    }
    
    // Stop the thread
    if (device_info->thread_id != 0) {
        thread_manager_->stopThread(device_info->thread_id);
    }
    
    device_info->is_running = false;
    
    // Remove from monitored devices
    monitored_devices_.erase(it);
    
    std::cout << "[DeviceManager] Successfully stopped monitoring for device: " << device_path << std::endl;
    
    return true;
}

bool DeviceWatchdogManager::isDeviceMonitored(const std::string& device_path) const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    return monitored_devices_.find(device_path) != monitored_devices_.end();
}

void DeviceWatchdogManager::stopAllMonitoring() {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::cout << "[DeviceManager] Stopping all device monitoring..." << std::endl;
    
    for (auto& pair : monitored_devices_) {
        auto& device_info = pair.second;
        
        std::cout << "[DeviceManager] Stopping device: " << pair.first << std::endl;
        
        // Stop the watchdog
        if (device_info->watchdog) {
            device_info->watchdog->stopMonitoring();
        }
        
        // Stop the thread
        if (device_info->thread_id != 0) {
            thread_manager_->stopThread(device_info->thread_id);
        }
        
        device_info->is_running = false;
    }
    
    monitored_devices_.clear();
    std::cout << "[DeviceManager] All device monitoring stopped" << std::endl;
}

std::vector<std::string> DeviceWatchdogManager::getMonitoredDevices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<std::string> devices;
    for (const auto& pair : monitored_devices_) {
        devices.push_back(pair.first);
    }
    
    return devices;
}

void DeviceWatchdogManager::watchdogThreadFunction(const std::string& device_path,
                                                   const Json::Value& /* monitoring_config */,
                                                   const Json::Value& /* failure_detection_config */) {
    std::cout << "[DeviceManager] Watchdog thread started for device: " << device_path << std::endl;
    
    // Find the device info
    DeviceWatchdogInfo* device_info = nullptr;
    {
        std::lock_guard<std::mutex> lock(devices_mutex_);
        auto it = monitored_devices_.find(device_path);
        if (it != monitored_devices_.end()) {
            device_info = it->second.get();
        }
    }
    
    if (!device_info || !device_info->watchdog) {
        std::cerr << "[DeviceManager] Invalid device info for " << device_path << std::endl;
        return;
    }
    
    // Start monitoring
    if (!device_info->watchdog->startMonitoring()) {
        std::cerr << "[DeviceManager] Failed to start monitoring for " << device_path << std::endl;
        return;
    }
    
    // Run the monitoring loop
    device_info->watchdog->monitoringLoop();
    
    std::cout << "[DeviceManager] Watchdog thread exiting for device: " << device_path << std::endl;
}

void DeviceWatchdogManager::setRpcClient(std::shared_ptr<RpcClient> rpc_client) {
    rpc_client_ = rpc_client;
    
    // Set RPC client for all existing watchdogs
    std::lock_guard<std::mutex> lock(devices_mutex_);
    for (auto& pair : monitored_devices_) {
        if (pair.second->watchdog) {
            pair.second->watchdog->setRpcClient(rpc_client_);
        }
    }
}

void DeviceWatchdogManager::setVerbose(bool verbose) {
    verbose_ = verbose;
    
    // Set verbose mode for all existing watchdogs
    std::lock_guard<std::mutex> lock(devices_mutex_);
    for (auto& pair : monitored_devices_) {
        if (pair.second->watchdog) {
            pair.second->watchdog->setVerbose(verbose);
        }
    }
}

bool DeviceWatchdogManager::forceRemoveDevice(const std::string& device_path) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = monitored_devices_.find(device_path);
    if (it == monitored_devices_.end()) {
        std::cout << "[DeviceManager] Device " << device_path << " not found in monitored list" << std::endl;
        return true; // Already removed
    }
    
    auto& device_info = it->second;
    
    std::cout << "[DeviceManager] Force removing device: " << device_path << std::endl;
    
    // Stop the watchdog immediately
    if (device_info->watchdog) {
        device_info->watchdog->stopMonitoring();
    }
    
    // Stop the thread immediately
    if (device_info->thread_id != 0) {
        thread_manager_->stopThread(device_info->thread_id);
    }
    
    device_info->is_running = false;
    
    // Remove from monitored devices immediately
    monitored_devices_.erase(it);
    
    std::cout << "[DeviceManager] Force removed device: " << device_path << std::endl;
    return true;
}

void DeviceWatchdogManager::cleanupUnavailableDevices() {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<std::string> devices_to_remove;
    
    for (const auto& pair : monitored_devices_) {
        const auto& device_info = pair.second;
        
        // Check if device is physically unavailable
        if (device_info->watchdog && !device_info->watchdog->isDeviceAvailable()) {
            std::cout << "[DeviceManager] Device " << pair.first 
                      << " is physically unavailable, forcing cleanup" << std::endl;
            devices_to_remove.push_back(pair.first);
        }
    }
    
    // Remove unavailable devices
    for (const auto& device_path : devices_to_remove) {
        auto it = monitored_devices_.find(device_path);
        if (it != monitored_devices_.end()) {
            auto& device_info = it->second;
            
            // Stop the watchdog first
            if (device_info->watchdog) {
                device_info->watchdog->stopMonitoring();
            }
            
            // Ensure the thread is stopped
            if (device_info->thread_id != 0) {
                thread_manager_->stopThread(device_info->thread_id);
            }
            
            device_info->is_running = false;
            monitored_devices_.erase(it);
            
            std::cout << "[DeviceManager] Force cleaned up unavailable device: " << device_path << std::endl;
        }
    }
}

void DeviceWatchdogManager::cleanupStoppedDevices() {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<std::string> devices_to_remove;
    
    for (const auto& pair : monitored_devices_) {
        const auto& device_info = pair.second;
        
        // Check if the watchdog is still monitoring or if device is physically unavailable
        if (device_info->watchdog && 
            (!device_info->watchdog->isMonitoring() || !device_info->watchdog->isDeviceAvailable())) {
            std::cout << "[DeviceManager] Device " << pair.first 
                      << " has stopped monitoring or is unavailable, scheduling cleanup" << std::endl;
            devices_to_remove.push_back(pair.first);
        }
    }
    
    // Remove stopped devices
    for (const auto& device_path : devices_to_remove) {
        auto it = monitored_devices_.find(device_path);
        if (it != monitored_devices_.end()) {
            auto& device_info = it->second;
            
            // Ensure the thread is stopped
            if (device_info->thread_id != 0) {
                thread_manager_->stopThread(device_info->thread_id);
            }
            
            device_info->is_running = false;
            monitored_devices_.erase(it);
            
            std::cout << "[DeviceManager] Cleaned up stopped device: " << device_path << std::endl;
        }
    }
}
