
#include "CallbackDispatcher.hpp"
#include "Logger.hpp"

CallbackDispatcher& CallbackDispatcher::getInstance() {
    static CallbackDispatcher instance;
    return instance;
}

void CallbackDispatcher::registerCallback(DeviceCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(callback);
    LOG_INFO("Callback registered, total callbacks: " + std::to_string(callbacks_.size()));
}

void CallbackDispatcher::notify(const DeviceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_INFO("Notifying " + std::to_string(callbacks_.size()) + " callbacks for device: " + info.devicePath);
    
    for (const auto& callback : callbacks_) {
        try {
            callback(info);
        } catch (const std::exception& e) {
            LOG_ERROR("Callback exception: " + std::string(e.what()));
        }
    }
}
