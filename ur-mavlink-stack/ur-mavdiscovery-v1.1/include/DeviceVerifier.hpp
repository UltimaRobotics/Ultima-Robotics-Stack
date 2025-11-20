
#pragma once
#include "DeviceInfo.hpp"
#include "ConfigLoader.hpp"
#include "../ur-threadder-api/cpp/include/ThreadManager.hpp"
#include <string>
#include <atomic>

class DeviceVerifier {
public:
    DeviceVerifier(const std::string& devicePath, const DeviceConfig& config, ThreadMgr::ThreadManager& threadManager);
    ~DeviceVerifier();
    
    void start();
    void stop();
    bool isRunning() const;
    
private:
    void verifyThread();
    bool testBaudrate(int baudrate, DeviceInfo& info);
    void extractUSBInfo(DeviceInfo& info);
    void saveDeviceToRuntimeFile(const DeviceInfo& info);
    
    std::string devicePath_;
    DeviceConfig config_;
    std::atomic<bool> running_;
    std::atomic<bool> shouldStop_;
    ThreadMgr::ThreadManager& threadManager_;
    unsigned int threadId_;
    bool threadCreated_;
};
