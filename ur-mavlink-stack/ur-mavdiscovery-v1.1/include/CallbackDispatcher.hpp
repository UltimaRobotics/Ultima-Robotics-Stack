
#pragma once
#include "DeviceInfo.hpp"
#include <functional>
#include <vector>
#include <mutex>
#include <memory>

using DeviceCallback = std::function<void(const DeviceInfo&)>;

class CallbackDispatcher {
public:
    static CallbackDispatcher& getInstance();
    
    void registerCallback(DeviceCallback callback);
    void notify(const DeviceInfo& info);
    
private:
    CallbackDispatcher() = default;
    ~CallbackDispatcher() = default;
    CallbackDispatcher(const CallbackDispatcher&) = delete;
    CallbackDispatcher& operator=(const CallbackDispatcher&) = delete;
    
    std::mutex mutex_;
    std::vector<DeviceCallback> callbacks_;
};
