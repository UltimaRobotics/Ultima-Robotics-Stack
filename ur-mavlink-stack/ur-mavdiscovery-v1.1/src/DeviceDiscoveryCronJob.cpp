#include "DeviceDiscoveryCronJob.hpp"
#include "Logger.hpp"
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

DeviceDiscoveryCronJob::DeviceDiscoveryCronJob(ThreadMgr::ThreadManager& threadManager, 
                                               RpcClient& rpcClient)
    : threadManager_(threadManager), rpcClient_(rpcClient), running_(false), cronThreadId_(0) {
}

DeviceDiscoveryCronJob::~DeviceDiscoveryCronJob() {
    stop();
}

bool DeviceDiscoveryCronJob::start() {
    if (running_.load()) {
        LOG_WARNING("Device discovery cron job already running");
        return true;
    }

    try {
        LOG_INFO("Starting device discovery cron job initialization...");
        
        // Set running flag before creating thread
        running_.store(true);
        
        // Create and start cron job thread using std::thread instead of ur-threadder-api for testing
        LOG_INFO("Creating cron job thread with std::thread...");
        std::thread cronThread(&DeviceDiscoveryCronJob::cronJobThreadFunc, this);
        cronThread.detach();
        
        LOG_INFO("Device discovery cron job started successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start device discovery cron job: " + std::string(e.what()));
        running_.store(false);
        return false;
    }
}

void DeviceDiscoveryCronJob::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    
    // Wait for thread to finish
    if (cronThreadId_ != 0) {
        try {
            bool completed = threadManager_.joinThread(cronThreadId_, std::chrono::seconds(5));
            if (!completed) {
                LOG_WARNING("Cron job thread did not complete in time, stopping forcefully");
                threadManager_.stopThread(cronThreadId_);
                threadManager_.joinThread(cronThreadId_, std::chrono::seconds(2));
            }
            
            // Unregister the thread
            threadManager_.unregisterThread("device_discovery_cron");
            
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to join cron job thread: " + std::string(e.what()));
        }
        cronThreadId_ = 0;
    }

    LOG_INFO("Device discovery cron job stopped");
}

bool DeviceDiscoveryCronJob::isRunning() const {
    return running_.load();
}

void DeviceDiscoveryCronJob::cronJobThreadFunc() {
    LOG_INFO("Device discovery cron job thread started successfully");
    
    int counter = 0;
    while (running_.load()) {
        try {
            counter++;
            LOG_INFO("Cron job heartbeat #" + std::to_string(counter) + " - running every 1 second");
            
            // Send device list notification to shared bus
            sendDeviceListNotification();
            
            // Sleep for 1 second (1000ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in cron job thread: " + std::string(e.what()));
            // Continue running even if there's an error
        }
    }
    
    LOG_INFO("Device discovery cron job thread stopped");
}

void DeviceDiscoveryCronJob::sendDeviceListNotification() {
    try {
        LOG_DEBUG("sendDeviceListNotification called");
        
        // Check if RPC client is running
        if (!rpcClient_.isRunning()) {
            LOG_DEBUG("RPC client not available, skipping device list notification");
            return;
        }

        LOG_DEBUG("RPC client is running, getting verified devices...");

        // Get all verified devices at runtime
        auto verifiedDevices = getVerifiedDevices();
        
        LOG_DEBUG("Found " + std::to_string(verifiedDevices.size()) + " verified devices");
        
        // Create notification payload
        nlohmann::json notificationPayload;
        notificationPayload["eventType"] = "DEVICE_LIST_UPDATE";
        notificationPayload["source"] = "ur-mavdiscovery";
        
        // Generate real timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        notificationPayload["timestamp"] = ss.str();
        
        // Convert devices to JSON array
        nlohmann::json devicesArray = nlohmann::json::array();
        for (const auto& device : verifiedDevices) {
            nlohmann::json deviceJson;
            
            // Basic device information
            deviceJson["devicePath"] = device.devicePath;
            deviceJson["baudrate"] = device.baudrate;
            deviceJson["sysid"] = device.sysid;
            deviceJson["compid"] = device.compid;
            deviceJson["mavlinkVersion"] = device.mavlinkVersion;
            deviceJson["timestamp"] = device.timestamp;
            
            // USB hardware information
            deviceJson["deviceName"] = device.usbInfo.deviceName;
            deviceJson["manufacturer"] = device.usbInfo.manufacturer;
            deviceJson["serialNumber"] = device.usbInfo.serialNumber;
            deviceJson["vendorId"] = device.usbInfo.vendorId;
            deviceJson["productId"] = device.usbInfo.productId;
            
            // USB bus tracking information
            deviceJson["usbBusNumber"] = device.usbInfo.usbBusNumber;
            deviceJson["usbDeviceAddress"] = device.usbInfo.usbDeviceAddress;
            deviceJson["physicalDeviceId"] = device.usbInfo.physicalDeviceId;
            
            // Flight controller identification
            deviceJson["boardClass"] = device.usbInfo.boardClass;
            deviceJson["boardName"] = device.usbInfo.boardName;
            deviceJson["autopilotType"] = device.usbInfo.autopilotType;
            
            // Device state
            deviceJson["state"] = "VERIFIED";
            
            devicesArray.push_back(deviceJson);
        }
        
        notificationPayload["payload"] = devicesArray;
        notificationPayload["deviceCount"] = static_cast<int>(verifiedDevices.size());
        notificationPayload["targetTopic"] = "ur-shared-bus/ur-mavlink-stack/notifications";
        
        LOG_DEBUG("Created notification payload with " + std::to_string(verifiedDevices.size()) + " devices");
        
        // Send notification to shared bus
        std::string notificationJson = notificationPayload.dump();
        LOG_DEBUG("Sending notification to shared bus: " + notificationJson.substr(0, 200) + "...");
        
        rpcClient_.sendResponse("ur-shared-bus/ur-mavlink-stack/notifications", notificationJson);
        
        LOG_INFO("Sent device list notification with " + std::to_string(verifiedDevices.size()) + " verified devices");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send device list notification: " + std::string(e.what()));
    }
}

std::vector<MavlinkShared::DeviceInfo> DeviceDiscoveryCronJob::getVerifiedDevices() {
    std::vector<MavlinkShared::DeviceInfo> verifiedDevices;
    
    try {
        // Get all devices from DeviceStateDB
        auto& deviceDB = DeviceStateDB::getInstance();
        auto allDevices = deviceDB.getAllDevices();
        
        // Get USB device tracker instance to filter primary paths only
        auto& tracker = USBDeviceTracker::getInstance();
        
        // Filter only verified devices that are primary paths
        for (const auto& device : allDevices) {
            if (device->state.load() == DeviceState::VERIFIED) {
                // Only include primary device paths to avoid duplicates
                if (tracker.isPrimaryPath(device->devicePath)) {
                    // Convert to shared format
                    MavlinkShared::DeviceInfo sharedDevice;
                    sharedDevice.devicePath = device->devicePath;
                    sharedDevice.state = MavlinkShared::DeviceState::VERIFIED;
                    sharedDevice.baudrate = device->baudrate;
                    sharedDevice.sysid = device->sysid;
                    sharedDevice.compid = device->compid;
                    sharedDevice.mavlinkVersion = device->mavlinkVersion;
                    sharedDevice.timestamp = device->timestamp;
                    
                    // Copy USB info including bus tracking
                    sharedDevice.usbInfo.deviceName = device->usbInfo.deviceName;
                    sharedDevice.usbInfo.manufacturer = device->usbInfo.manufacturer;
                    sharedDevice.usbInfo.serialNumber = device->usbInfo.serialNumber;
                    sharedDevice.usbInfo.vendorId = device->usbInfo.vendorId;
                    sharedDevice.usbInfo.productId = device->usbInfo.productId;
                    sharedDevice.usbInfo.boardClass = device->usbInfo.boardClass;
                    sharedDevice.usbInfo.boardName = device->usbInfo.boardName;
                    sharedDevice.usbInfo.autopilotType = device->usbInfo.autopilotType;
                    sharedDevice.usbInfo.usbBusNumber = device->usbInfo.usbBusNumber;
                    sharedDevice.usbInfo.usbDeviceAddress = device->usbInfo.usbDeviceAddress;
                    sharedDevice.usbInfo.physicalDeviceId = device->usbInfo.physicalDeviceId;
                    
                    // Copy MAVLink messages
                    for (const auto& msg : device->messages) {
                        MavlinkShared::MAVLinkMessage sharedMsg;
                        sharedMsg.msgid = msg.msgid;
                        sharedMsg.name = msg.name;
                        sharedDevice.messages.push_back(sharedMsg);
                    }
                    
                    verifiedDevices.push_back(sharedDevice);
                    LOG_DEBUG("Included primary device in cron list: " + device->devicePath + 
                             " (physical: " + device->usbInfo.physicalDeviceId + ")");
                } else {
                    LOG_DEBUG("Excluded secondary device from cron list: " + device->devicePath + 
                             " (physical: " + device->usbInfo.physicalDeviceId + ")");
                }
            }
        }
        
        LOG_INFO("Device discovery cron job found " + std::to_string(verifiedDevices.size()) + 
                " primary verified devices (filtered out duplicates)");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get verified devices: " + std::string(e.what()));
    }
    
    return verifiedDevices;
}
