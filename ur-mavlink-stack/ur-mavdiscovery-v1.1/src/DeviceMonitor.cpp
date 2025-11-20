
#include "DeviceMonitor.hpp"
#include "DeviceStateDB.hpp"
#include "Logger.hpp"
#include <algorithm>

DeviceMonitor::DeviceMonitor(const DeviceConfig& config, ThreadMgr::ThreadManager& threadManager)
    : config_(config), udev_(nullptr), monitor_(nullptr), running_(false),
      threadManager_(threadManager), threadId_(0), threadCreated_(false) {}

DeviceMonitor::~DeviceMonitor() {
    stop();
}

bool DeviceMonitor::start() {
    udev_ = udev_new();
    if (!udev_) {
        LOG_ERROR("Failed to create udev context");
        return false;
    }
    
    monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
    if (!monitor_) {
        LOG_ERROR("Failed to create udev monitor");
        udev_unref(udev_);
        return false;
    }
    
    udev_monitor_filter_add_match_subsystem_devtype(monitor_, "tty", nullptr);
    udev_monitor_enable_receiving(monitor_);
    
    // Enumerate existing devices before starting monitor thread
    enumerateExistingDevices();
    
    running_ = true;
    
    // Create monitor thread using ThreadManager
    threadId_ = threadManager_.createThread([this]() {
        // Register thread from within the thread to avoid race condition
        try {
            threadManager_.registerThread(threadId_, "device_monitor");
            LOG_DEBUG("Monitor thread registered with attachment: device_monitor");
        } catch (const ThreadMgr::ThreadManagerException& e) {
            LOG_WARNING("Failed to register monitor thread: " + std::string(e.what()));
        }
        this->monitorThread();
    });
    
    threadCreated_ = true;
    
    LOG_INFO("Device monitor started with thread ID: " + std::to_string(threadId_));
    return true;
}

void DeviceMonitor::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Unregister thread FIRST to free up the attachment
    if (threadCreated_) {
        try {
            threadManager_.unregisterThread("device_monitor");
            LOG_DEBUG("Unregistered monitor thread attachment");
        } catch (const ThreadMgr::ThreadManagerException& e) {
            LOG_DEBUG("Monitor thread already unregistered: " + std::string(e.what()));
        }
        threadCreated_ = false;
    }
    
    // Wait for monitor thread to complete with timeout
    if (threadManager_.isThreadAlive(threadId_)) {
        bool completed = threadManager_.joinThread(threadId_, std::chrono::seconds(5));
        
        if (!completed) {
            LOG_WARNING("Monitor thread " + std::to_string(threadId_) + " did not complete in time, stopping forcefully");
            threadManager_.stopThread(threadId_);
            threadManager_.joinThread(threadId_, std::chrono::seconds(2));
        }
    }
    
    if (monitor_) {
        udev_monitor_unref(monitor_);
        monitor_ = nullptr;
    }
    
    if (udev_) {
        udev_unref(udev_);
        udev_ = nullptr;
    }
    
    LOG_INFO("Device monitor stopped");
}

void DeviceMonitor::monitorThread() {
    int fd = udev_monitor_get_fd(monitor_);
    
    while (running_) {
        fd_set fds;
        struct timeval tv;
        
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret > 0 && FD_ISSET(fd, &fds)) {
            struct udev_device* dev = udev_monitor_receive_device(monitor_);
            if (dev) {
                const char* action = udev_device_get_action(dev);
                const char* devnode = udev_device_get_devnode(dev);
                
                if (devnode) {
                    std::string devicePath(devnode);
                    
                    if (action && std::string(action) == "add") {
                        handleDeviceAdd(devicePath);
                    } else if (action && std::string(action) == "remove") {
                        handleDeviceRemove(devicePath);
                    }
                }
                
                udev_device_unref(dev);
            }
        }
    }
}

void DeviceMonitor::handleDeviceAdd(const std::string& devicePath) {
    if (!matchesFilter(devicePath)) {
        LOG_DEBUG("Device ignored (filter): " + devicePath);
        return;
    }
    
    LOG_INFO("Device added: " + devicePath);
    
    DeviceStateDB::getInstance().addDevice(devicePath);
    
    // Notify via callback (DeviceManager handles verification)
    if (addCallback_) {
        addCallback_(devicePath);
    }
}

void DeviceMonitor::handleDeviceRemove(const std::string& devicePath) {
    LOG_INFO("Device removed: " + devicePath);
    
    DeviceStateDB::getInstance().removeDevice(devicePath);
    
    // Notify via callback (DeviceManager handles cleanup)
    if (removeCallback_) {
        removeCallback_(devicePath);
    }
}

bool DeviceMonitor::matchesFilter(const std::string& devicePath) {
    for (const auto& filter : config_.devicePathFilters) {
        if (devicePath.find(filter) == 0) {
            return true;
        }
    }
    return false;
}

void DeviceMonitor::enumerateExistingDevices() {
    LOG_INFO("Enumerating existing devices...");
    
    struct udev_enumerate* enumerate = udev_enumerate_new(udev_);
    if (!enumerate) {
        LOG_ERROR("Failed to create udev enumerate");
        return;
    }
    
    // Filter for tty subsystem
    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_scan_devices(enumerate);
    
    struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry;
    
    int deviceCount = 0;
    udev_list_entry_foreach(entry, devices) {
        const char* path = udev_list_entry_get_name(entry);
        struct udev_device* dev = udev_device_new_from_syspath(udev_, path);
        
        if (dev) {
            const char* devnode = udev_device_get_devnode(dev);
            
            if (devnode) {
                std::string devicePath(devnode);
                
                if (matchesFilter(devicePath)) {
                    LOG_INFO("Found existing device: " + devicePath);
                    handleDeviceAdd(devicePath);
                    deviceCount++;
                }
            }
            
            udev_device_unref(dev);
        }
    }
    
    udev_enumerate_unref(enumerate);
    
    if (deviceCount == 0) {
        LOG_INFO("No existing devices found");
    } else {
        LOG_INFO("Found " + std::to_string(deviceCount) + " existing device(s)");
    }
}
