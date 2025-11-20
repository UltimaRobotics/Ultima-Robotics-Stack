
#include "DeviceStateDB.hpp"
#include "Logger.hpp"

DeviceStateDB& DeviceStateDB::getInstance() {
    static DeviceStateDB instance;
    return instance;
}

void DeviceStateDB::addDevice(const std::string& devicePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = devices_.find(devicePath);
    if (it == devices_.end()) {
        auto info = std::make_shared<DeviceInfo>();
        info->devicePath = devicePath;
        info->state = DeviceState::UNKNOWN;
        devices_[devicePath] = info;
        LOG_INFO("Device added to state DB: " + devicePath);
    }
}

void DeviceStateDB::updateDevice(const std::string& devicePath, const DeviceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = devices_.find(devicePath);
    if (it != devices_.end()) {
        it->second->state = info.state.load();
        it->second->baudrate = info.baudrate;
        it->second->sysid = info.sysid;
        it->second->compid = info.compid;
        it->second->messages = info.messages;
        it->second->mavlinkVersion = info.mavlinkVersion;
        it->second->timestamp = info.timestamp;
        LOG_INFO("Device updated in state DB: " + devicePath);
    }
}

void DeviceStateDB::removeDevice(const std::string& devicePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = devices_.find(devicePath);
    if (it != devices_.end()) {
        it->second->state = DeviceState::REMOVED;
        devices_.erase(it);
        LOG_INFO("Device removed from state DB: " + devicePath);
    }
}

std::shared_ptr<DeviceInfo> DeviceStateDB::getDevice(const std::string& devicePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = devices_.find(devicePath);
    if (it != devices_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<DeviceInfo>> DeviceStateDB::getAllDevices() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<DeviceInfo>> result;
    for (const auto& pair : devices_) {
        result.push_back(pair.second);
    }
    return result;
}
