
#pragma once
#include "DeviceInfo.hpp"
#include <map>
#include <mutex>
#include <memory>

class DeviceStateDB {
public:
    static DeviceStateDB& getInstance();
    
    void addDevice(const std::string& devicePath);
    void updateDevice(const std::string& devicePath, const DeviceInfo& info);
    void removeDevice(const std::string& devicePath);
    std::shared_ptr<DeviceInfo> getDevice(const std::string& devicePath);
    std::vector<std::shared_ptr<DeviceInfo>> getAllDevices();
    
private:
    DeviceStateDB() = default;
    ~DeviceStateDB() = default;
    DeviceStateDB(const DeviceStateDB&) = delete;
    DeviceStateDB& operator=(const DeviceStateDB&) = delete;
    
    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<DeviceInfo>> devices_;
};
