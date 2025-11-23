#pragma once

#include "../ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "../ur-mavdiscovery-shared/include/MavlinkDeviceStructs.hpp"
#include "../ur-mavdiscovery-shared/include/MavlinkEventSerializer.hpp"
#include "RpcClient.hpp"
#include "DeviceStateDB.hpp"
#include "USBDeviceTracker.hpp"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

class DeviceDiscoveryCronJob {
public:
    DeviceDiscoveryCronJob(ThreadMgr::ThreadManager& threadManager, 
                           RpcClient& rpcClient);
    ~DeviceDiscoveryCronJob();

    bool start();
    void stop();
    bool isRunning() const;

private:
    void cronJobThreadFunc();
    void sendDeviceListNotification();
    std::vector<MavlinkShared::DeviceInfo> getVerifiedDevices();

    ThreadMgr::ThreadManager& threadManager_;
    RpcClient& rpcClient_;
    std::atomic<bool> running_;
    unsigned int cronThreadId_;
    mutable std::mutex mutex_;
};
