
#pragma once
#include "ConfigLoader.hpp"
#include <ThreadManager.hpp>
#include <libudev.h>
#include <memory>
#include <atomic>
#include <functional>

class DeviceMonitor {
public:
    DeviceMonitor(const DeviceConfig& config, ThreadMgr::ThreadManager& threadManager);
    ~DeviceMonitor();
    
    bool start();
    void stop();
    
    void setAddCallback(std::function<void(const std::string&)> callback) { addCallback_ = callback; }
    void setRemoveCallback(std::function<void(const std::string&)> callback) { removeCallback_ = callback; }
    
private:
    void monitorThread();
    void handleDeviceAdd(const std::string& devicePath);
    void handleDeviceRemove(const std::string& devicePath);
    bool matchesFilter(const std::string& devicePath);
    void enumerateExistingDevices();
    
    DeviceConfig config_;
    struct udev* udev_;
    struct udev_monitor* monitor_;
    std::atomic<bool> running_;
    ThreadMgr::ThreadManager& threadManager_;
    unsigned int threadId_;
    bool threadCreated_;
    std::function<void(const std::string&)> addCallback_;
    std::function<void(const std::string&)> removeCallback_;
};
