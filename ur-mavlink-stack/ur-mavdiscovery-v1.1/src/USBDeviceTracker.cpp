#include "USBDeviceTracker.hpp"
#include "Logger.hpp"
#include <algorithm>

USBDeviceTracker& USBDeviceTracker::getInstance() {
    static USBDeviceTracker instance;
    return instance;
}

bool USBDeviceTracker::hasPhysicalDevice(const std::string& physicalDeviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return physicalDevices_.find(physicalDeviceId) != physicalDevices_.end();
}

std::string USBDeviceTracker::getPrimaryDevicePath(const std::string& physicalDeviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = physicalDevices_.find(physicalDeviceId);
    if (it != physicalDevices_.end()) {
        return it->second->primaryDevicePath;
    }
    return "";
}

void USBDeviceTracker::registerDevice(const std::string& devicePath, const DeviceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if this device path is already registered
    auto pathIt = pathToPhysicalId_.find(devicePath);
    if (pathIt != pathToPhysicalId_.end()) {
        LOG_WARNING("Device path already registered: " + devicePath + " -> " + pathIt->second);
        return;
    }
    
    // Use physical device ID if available, otherwise fall back to serial number
    std::string physicalId = info.usbInfo.physicalDeviceId;
    if (physicalId.empty()) {
        physicalId = "serial:" + info.usbInfo.serialNumber;
        LOG_WARNING("Using serial-based fallback ID for device: " + devicePath + " -> " + physicalId);
    }
    
    auto it = physicalDevices_.find(physicalId);
    if (it == physicalDevices_.end()) {
        // New physical device
        auto physicalDevice = std::make_shared<PhysicalDevice>();
        physicalDevice->physicalDeviceId = physicalId;
        physicalDevice->primaryDevicePath = devicePath;
        physicalDevice->devicePaths.push_back(devicePath);
        
        // Manually copy DeviceInfo fields (avoid atomic copy issue)
        physicalDevice->deviceInfo.devicePath = info.devicePath;
        physicalDevice->deviceInfo.state.store(info.state.load());
        physicalDevice->deviceInfo.baudrate = info.baudrate;
        physicalDevice->deviceInfo.sysid = info.sysid;
        physicalDevice->deviceInfo.compid = info.compid;
        physicalDevice->deviceInfo.messages = info.messages;
        physicalDevice->deviceInfo.mavlinkVersion = info.mavlinkVersion;
        physicalDevice->deviceInfo.timestamp = info.timestamp;
        physicalDevice->deviceInfo.usbInfo = info.usbInfo;
        
        physicalDevices_[physicalId] = physicalDevice;
        pathToPhysicalId_[devicePath] = physicalId;
        
        LOG_INFO("Registered new physical device: " + physicalId + " with primary path: " + devicePath);
    } else {
        // Existing physical device - add additional path
        auto& physicalDevice = it->second;
        
        // Check if this path should become primary (prefer lower numbered ACM devices)
        bool shouldBecomePrimary = false;
        if (devicePath.find("/dev/ttyACM") != std::string::npos && 
            physicalDevice->primaryDevicePath.find("/dev/ttyACM") != std::string::npos) {
            
            // Extract ACM numbers and compare
            try {
                size_t currentPos = physicalDevice->primaryDevicePath.find_last_of("0123456789");
                size_t newPos = devicePath.find_last_of("0123456789");
                
                if (currentPos != std::string::npos && newPos != std::string::npos) {
                    int currentNum = std::stoi(physicalDevice->primaryDevicePath.substr(currentPos));
                    int newNum = std::stoi(devicePath.substr(newPos));
                    
                    if (newNum < currentNum) {
                        shouldBecomePrimary = true;
                    }
                }
            } catch (...) {
                // If parsing fails, keep current primary
            }
        }
        
        physicalDevice->devicePaths.push_back(devicePath);
        pathToPhysicalId_[devicePath] = physicalId;
        
        if (shouldBecomePrimary) {
            LOG_INFO("Changing primary path for " + physicalId + " from " + 
                    physicalDevice->primaryDevicePath + " to " + devicePath);
            physicalDevice->primaryDevicePath = devicePath;
            
            // Manually copy DeviceInfo fields (avoid atomic copy issue)
            physicalDevice->deviceInfo.devicePath = info.devicePath;
            physicalDevice->deviceInfo.state.store(info.state.load());
            physicalDevice->deviceInfo.baudrate = info.baudrate;
            physicalDevice->deviceInfo.sysid = info.sysid;
            physicalDevice->deviceInfo.compid = info.compid;
            physicalDevice->deviceInfo.messages = info.messages;
            physicalDevice->deviceInfo.mavlinkVersion = info.mavlinkVersion;
            physicalDevice->deviceInfo.timestamp = info.timestamp;
            physicalDevice->deviceInfo.usbInfo = info.usbInfo;
        }
        
        LOG_INFO("Added additional path to physical device " + physicalId + ": " + devicePath + 
                " (primary: " + physicalDevice->primaryDevicePath + ")");
    }
}

void USBDeviceTracker::removeDevice(const std::string& devicePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto pathIt = pathToPhysicalId_.find(devicePath);
    if (pathIt == pathToPhysicalId_.end()) {
        LOG_WARNING("Attempting to remove unknown device path: " + devicePath);
        return;
    }
    
    std::string physicalId = pathIt->second;
    auto it = physicalDevices_.find(physicalId);
    
    if (it != physicalDevices_.end()) {
        auto& physicalDevice = it->second;
        
        // Remove the device path from the list
        auto pathIt = std::find(physicalDevice->devicePaths.begin(), 
                               physicalDevice->devicePaths.end(), devicePath);
        if (pathIt != physicalDevice->devicePaths.end()) {
            physicalDevice->devicePaths.erase(pathIt);
        }
        
        // Remove from path mapping
        pathToPhysicalId_.erase(devicePath);
        
        // If this was the primary path, select a new one if available
        if (physicalDevice->primaryDevicePath == devicePath) {
            if (!physicalDevice->devicePaths.empty()) {
                // Select the new primary (prefer lowest ACM number)
                std::string newPrimary = physicalDevice->devicePaths[0];
                for (const auto& path : physicalDevice->devicePaths) {
                    if (path.find("/dev/ttyACM") != std::string::npos && 
                        newPrimary.find("/dev/ttyACM") != std::string::npos) {
                        
                        try {
                            size_t currentPos = newPrimary.find_last_of("0123456789");
                            size_t newPos = path.find_last_of("0123456789");
                            
                            if (currentPos != std::string::npos && newPos != std::string::npos) {
                                int currentNum = std::stoi(newPrimary.substr(currentPos));
                                int newNum = std::stoi(path.substr(newPos));
                                
                                if (newNum < currentNum) {
                                    newPrimary = path;
                                }
                            }
                        } catch (...) {
                            // If parsing fails, keep current selection
                        }
                    }
                }
                
                physicalDevice->primaryDevicePath = newPrimary;
                LOG_INFO("Changed primary path for " + physicalId + " to: " + newPrimary);
            } else {
                // No more paths for this physical device - remove it
                physicalDevices_.erase(it);
                LOG_INFO("Removed physical device " + physicalId + " - no more paths available");
            }
        }
        
        LOG_INFO("Removed device path: " + devicePath + " from physical device: " + physicalId);
    }
}

std::vector<std::string> USBDeviceTracker::getDevicePaths(const std::string& physicalDeviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = physicalDevices_.find(physicalDeviceId);
    if (it != physicalDevices_.end()) {
        return it->second->devicePaths;
    }
    return {};
}

bool USBDeviceTracker::isPrimaryPath(const std::string& devicePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto pathIt = pathToPhysicalId_.find(devicePath);
    if (pathIt != pathToPhysicalId_.end()) {
        auto it = physicalDevices_.find(pathIt->second);
        if (it != physicalDevices_.end()) {
            return it->second->primaryDevicePath == devicePath;
        }
    }
    return false;
}

std::string USBDeviceTracker::getPhysicalDeviceId(const std::string& devicePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pathToPhysicalId_.find(devicePath);
    if (it != pathToPhysicalId_.end()) {
        return it->second;
    }
    return "";
}

std::vector<std::string> USBDeviceTracker::getAllPhysicalDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& pair : physicalDevices_) {
        result.push_back(pair.first);
    }
    return result;
}
