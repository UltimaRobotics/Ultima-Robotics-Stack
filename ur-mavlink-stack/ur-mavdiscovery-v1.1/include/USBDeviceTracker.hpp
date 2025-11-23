#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "DeviceInfo.hpp"

class USBDeviceTracker {
public:
    static USBDeviceTracker& getInstance();
    
    // Check if a device with the same physical ID already exists
    bool hasPhysicalDevice(const std::string& physicalDeviceId) const;
    
    // Get the primary device path for a physical device
    std::string getPrimaryDevicePath(const std::string& physicalDeviceId) const;
    
    // Register a new physical device or add an additional path to existing device
    void registerDevice(const std::string& devicePath, const DeviceInfo& info);
    
    // Remove a device path (may remove physical device if no paths remain)
    void removeDevice(const std::string& devicePath);
    
    // Get all device paths for a physical device
    std::vector<std::string> getDevicePaths(const std::string& physicalDeviceId) const;
    
    // Check if a device path is the primary path for its physical device
    bool isPrimaryPath(const std::string& devicePath) const;
    
    // Get physical device ID for a device path
    std::string getPhysicalDeviceId(const std::string& devicePath) const;
    
    // Get all registered physical devices
    std::vector<std::string> getAllPhysicalDevices() const;

private:
    USBDeviceTracker() = default;
    ~USBDeviceTracker() = default;
    USBDeviceTracker(const USBDeviceTracker&) = delete;
    USBDeviceTracker& operator=(const USBDeviceTracker&) = delete;
    
    struct PhysicalDevice {
        std::string physicalDeviceId;
        std::string primaryDevicePath;
        std::vector<std::string> devicePaths;
        DeviceInfo deviceInfo;
    };
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<PhysicalDevice>> physicalDevices_;
    std::unordered_map<std::string, std::string> pathToPhysicalId_;
};
