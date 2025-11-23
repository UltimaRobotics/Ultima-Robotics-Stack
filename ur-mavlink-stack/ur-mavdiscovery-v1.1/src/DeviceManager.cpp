#include "DeviceManager.hpp"
#ifdef HTTP_ENABLED
#include "HTTPClient.hpp"
#endif
#include "RpcClient.hpp"
#include "CallbackDispatcher.hpp"
#include "Logger.hpp"
#include "DeviceStateDB.hpp"
#include "USBDeviceTracker.hpp"
#include <thread>
#include <chrono>

DeviceManager::DeviceManager() : running_(false), rpcRunning_(false) {}

DeviceManager::~DeviceManager() {
    shutdown();
}

bool DeviceManager::initialize(const std::string& configPath) {
    try {
        config_ = ConfigLoader();
        if (!config_.loadFromFile(configPath)) {
            LOG_ERROR("Failed to load device manager configuration");
            return false;
        }

        DeviceConfig deviceConfig = config_.getConfig();

        // Set log level from config
        if (deviceConfig.logLevel == "DEBUG") {
            Logger::getInstance().setLogLevel(LogLevel::DEBUG);
        } else if (deviceConfig.logLevel == "INFO") {
            Logger::getInstance().setLogLevel(LogLevel::INFO);
        } else if (deviceConfig.logLevel == "WARNING") {
            Logger::getInstance().setLogLevel(LogLevel::WARNING);
        } else if (deviceConfig.logLevel == "ERROR") {
            Logger::getInstance().setLogLevel(LogLevel::ERROR);
        }

        // Initialize thread manager
        threadManager_ = std::make_unique<ThreadMgr::ThreadManager>(20);

        // Initialize device monitor
        monitor_ = std::make_unique<DeviceMonitor>(deviceConfig, *threadManager_);
        if (!monitor_) {
            LOG_ERROR("Failed to create device monitor");
            return false;
        }

        // Set up callbacks
        monitor_->setAddCallback([this](const std::string& path) {
            this->onDeviceAdded(path);
        });

        monitor_->setRemoveCallback([this](const std::string& path) {
            this->onDeviceRemoved(path);
        });

        if (!monitor_->start()) {
            LOG_ERROR("Failed to start device monitor");
            return false;
        }

        // Register callback for device verification events
        CallbackDispatcher::getInstance().registerCallback([this](const DeviceInfo& info) {
            if (info.state == DeviceState::VERIFIED) {
                this->onDeviceVerified(info.devicePath, info);
            }
        });

        running_ = true;
        
        // Send init process discovery notification after initialization
        LOG_INFO("Sending init process discovery notification");
        sendInitProcessDiscoveryNotification();
        
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize device manager: " + std::string(e.what()));
        return false;
    }
}

void DeviceManager::run() {
    LOG_INFO("Device manager running... Press Ctrl+C to exit");

    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Log active thread count periodically
        unsigned int threadCount = threadManager_->getThreadCount();
        if (threadCount > 0) {
            LOG_DEBUG("Active verification threads: " + std::to_string(threadCount));
        }
    }
}

void DeviceManager::shutdown() {
    if (!running_) return;

    LOG_INFO("Shutting down device manager...");
    running_ = false;

    // Shutdown RPC system first
    shutdownRpc();

    if (monitor_) {
        monitor_->stop();
    }

    // Stop all verifiers
    std::lock_guard<std::mutex> lock(verifiersMutex_);
    for (auto& pair : verifiers_) {
        pair.second->stop();
    }
    verifiers_.clear();

    // Log remaining threads
    unsigned int remainingThreads = threadManager_->getThreadCount();
    if (remainingThreads > 0) {
            LOG_WARNING("Remaining threads after shutdown: " + std::to_string(remainingThreads));

        auto threadIds = threadManager_->getAllThreadIds();
        for (auto id : threadIds) {
            LOG_DEBUG("Active thread ID: " + std::to_string(id));
        }
    }

    LOG_INFO("Device manager shutdown complete");
}

void DeviceManager::onDeviceAdded(const std::string& devicePath) {
    LOG_INFO("Device added: " + devicePath);

    std::lock_guard<std::mutex> lock(verifiersMutex_);

    // Check if already verifying
    if (verifiers_.find(devicePath) != verifiers_.end()) {
            LOG_WARNING("Device already being verified: " + devicePath);
        return;
    }

    // Create and start verifier with ThreadManager
    auto verifier = std::make_unique<DeviceVerifier>(
        devicePath, 
        config_.getConfig(),
        *threadManager_
    );

    verifier->start();
    verifiers_[devicePath] = std::move(verifier);

    LOG_INFO("Started verification for: " + devicePath);
}

void DeviceManager::onDeviceRemoved(const std::string& devicePath) {
    LOG_INFO("Device removed: " + devicePath);

    std::lock_guard<std::mutex> lock(verifiersMutex_);

    auto it = verifiers_.find(devicePath);
    if (it != verifiers_.end()) {
        // Use USB device tracker to handle device removal
        auto& tracker = USBDeviceTracker::getInstance();
        std::string physicalId = tracker.getPhysicalDeviceId(devicePath);
        
        // Check if this is the primary path before removing
        bool wasPrimary = tracker.isPrimaryPath(devicePath);
        std::string newPrimaryPath;
        
        if (!physicalId.empty()) {
            // Get the new primary path before removal
            auto devicePaths = tracker.getDevicePaths(physicalId);
            for (const auto& path : devicePaths) {
                if (path != devicePath) {
                    newPrimaryPath = path;
                    break;
                }
            }
        }
        
        // Remove from tracker
        tracker.removeDevice(devicePath);
        
        // Only send RPC notifications if this was the primary path
        if (wasPrimary) {
            LOG_INFO("Primary device path removed: " + devicePath + " for physical device: " + physicalId);
            
            // Send RPC device removed notification using ur-rpc-template structure
            sendDeviceRemovedRpcNotifications(devicePath);

            // Send shared bus notification
            sendDeviceRemovedSharedNotification(devicePath);
            
            // If there's a new primary path, send notifications for it
            if (!newPrimaryPath.empty()) {
                LOG_INFO("Notifying new primary path: " + newPrimaryPath + " for physical device: " + physicalId);
                
                // Get the device info for the new primary path from DeviceStateDB
                auto& deviceDB = DeviceStateDB::getInstance();
                auto newDeviceInfo = deviceDB.getDevice(newPrimaryPath);
                
                if (newDeviceInfo && newDeviceInfo->state.load() == DeviceState::VERIFIED) {
                    // Send notifications for the new primary device
                    sendDeviceAddedRpcNotifications(*newDeviceInfo);
                    sendDeviceVerifiedNotification(*newDeviceInfo);
                }
            }
        } else {
            LOG_INFO("Secondary device path removed: " + devicePath + " for physical device: " + physicalId + 
                    " - skipping notifications (primary still available)");
        }

#ifdef HTTP_ENABLED
        DeviceConfig deviceConfig = config_.getConfig();
        if (deviceConfig.enableHTTP && wasPrimary) {
            // Send stop request to MAVRouter mainloop only for primary device removal
            LOG_INFO("Sending mainloop stop request to MAVRouter for device removal: " + devicePath);
            std::string stopEndpoint = "http://" + deviceConfig.httpConfig.serverAddress + ":" + 
                                      std::to_string(deviceConfig.httpConfig.serverPort) + "/api/threads/mainloop/stop";
            HTTPClient routerClient(stopEndpoint, deviceConfig.httpConfig.timeoutMs);
            DeviceInfo stopCmd;
            stopCmd.devicePath = devicePath;
            routerClient.postDeviceInfo(stopCmd);
        }
#endif

        it->second->stop();
        verifiers_.erase(it);
        LOG_INFO("Stopped verification for: " + devicePath);
    }
}

void DeviceManager::onDeviceVerified(const std::string& devicePath, const DeviceInfo& info) {
    LOG_INFO("Device verified: " + devicePath);
    
    // Use USB device tracker to handle duplicate devices
    auto& tracker = USBDeviceTracker::getInstance();
    
    // Check if this device is already registered as part of a physical device
    if (tracker.hasPhysicalDevice(info.usbInfo.physicalDeviceId)) {
        std::string primaryPath = tracker.getPrimaryDevicePath(info.usbInfo.physicalDeviceId);
        
        if (tracker.isPrimaryPath(devicePath)) {
            // This is the primary path - send notifications
            LOG_INFO("Primary device path verified: " + devicePath + " for physical device: " + 
                    info.usbInfo.physicalDeviceId);
            
            // Send RPC notifications for verified device using ur-rpc-template structure
            sendDeviceAddedRpcNotifications(info);
            
            // Send shared bus notification
            sendDeviceVerifiedNotification(info);
        } else {
            // This is a secondary path - don't send duplicate notifications
            LOG_INFO("Secondary device path verified: " + devicePath + " for physical device: " + 
                    info.usbInfo.physicalDeviceId + " (primary: " + primaryPath + 
                    ") - skipping duplicate notifications");
        }
    } else {
        // New physical device - register it and send notifications
        tracker.registerDevice(devicePath, info);
        LOG_INFO("New physical device registered: " + info.usbInfo.physicalDeviceId);
        
        // Send RPC notifications for verified device using ur-rpc-template structure
        sendDeviceAddedRpcNotifications(info);
        
        // Send shared bus notification
        sendDeviceVerifiedNotification(info);
    }
}

void DeviceManager::sendDeviceAddedRpcNotifications(const DeviceInfo& info) {
    try {
        // Wait for RPC client to be available with retry mechanism
        int retryCount = 0;
        const int maxRetries = 10;
        const int retryDelayMs = 500;
        
        while ((!rpcClient_ || !rpcClient_->isRunning()) && retryCount < maxRetries) {
            LOG_INFO("Waiting for RPC client to be available for device added notifications... (attempt " + 
                     std::to_string(retryCount + 1) + "/" + std::to_string(maxRetries) + ")");
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
            retryCount++;
        }
        
        if (!rpcClient_ || !rpcClient_->isRunning()) {
            LOG_ERROR("RPC client still not available for device added notifications after " + 
                     std::to_string(maxRetries) + " attempts");
            return;
        }
        
        LOG_INFO("RPC client is now available, sending device added notifications");
        
        // Convert to shared DeviceInfo format
        auto sharedDeviceInfo = convertToSharedDeviceInfo(info);
        
        // Create device added event using ur-rpc-template structure
        MavlinkShared::DeviceAddedEvent deviceAddedEvent(sharedDeviceInfo);
        
        // Create RPC request using ur-rpc-template format
        auto requestJson = MavlinkShared::MavlinkEventSerializer::createDeviceAddedRequest(deviceAddedEvent);
        std::string requestPayload = requestJson.dump();
        
        // Extract params from the full JSON-RPC request for ur-rpc-template
        std::string paramsJson = requestJson["params"].dump();
        
        // Send proper RPC requests to ur-mavrouter and ur-mavcollector
        LOG_INFO("Sending mavlink_device_added RPC request to ur-mavrouter");
        rpcClient_->sendRpcRequest("ur-mavrouter", "mavlink_device_added", paramsJson);
        
        LOG_INFO("Sending mavlink_device_added RPC request to ur-mavcollector");
        rpcClient_->sendRpcRequest("ur-mavcollector", "mavlink_device_added", paramsJson);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send RPC device added notifications: " + std::string(e.what()));
    }
}

void DeviceManager::sendDeviceRemovedRpcNotifications(const std::string& devicePath) {
    try {
        // Wait for RPC client to be available with retry mechanism
        int retryCount = 0;
        const int maxRetries = 10;
        const int retryDelayMs = 500;
        
        while ((!rpcClient_ || !rpcClient_->isRunning()) && retryCount < maxRetries) {
            LOG_INFO("Waiting for RPC client to be available for device removal notifications... (attempt " + 
                     std::to_string(retryCount + 1) + "/" + std::to_string(maxRetries) + ")");
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
            retryCount++;
        }
        
        if (!rpcClient_ || !rpcClient_->isRunning()) {
            LOG_ERROR("RPC client still not available for device removal notifications after " + 
                     std::to_string(maxRetries) + " attempts");
            return;
        }
        
        LOG_INFO("RPC client is now available, sending device removal notifications");
        
        // Create device removed event using ur-rpc-template structure
        MavlinkShared::DeviceRemovedEvent deviceRemovedEvent(devicePath);
        
        // Create RPC request using ur-rpc-template format
        auto requestJson = MavlinkShared::MavlinkEventSerializer::createDeviceRemovedRequest(deviceRemovedEvent);
        std::string requestPayload = requestJson.dump();
        
        // Extract params from the full JSON-RPC request for ur-rpc-template
        std::string paramsJson = requestJson["params"].dump();
        
        // Send proper RPC requests to ur-mavrouter and ur-mavcollector
        LOG_INFO("Sending mavlink_device_removed RPC request to ur-mavrouter");
        rpcClient_->sendRpcRequest("ur-mavrouter", "mavlink_device_removed", paramsJson);
        
        LOG_INFO("Sending mavlink_device_removed RPC request to ur-mavcollector");
        rpcClient_->sendRpcRequest("ur-mavcollector", "mavlink_device_removed", paramsJson);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send RPC device removed notifications: " + std::string(e.what()));
    }
}

bool DeviceManager::initializeRpc(const std::string& rpcConfigPath) {
    try {
        // Initialize RPC client
        rpcClient_ = std::make_unique<RpcClient>(rpcConfigPath, "ur-mavdiscovery");
        
        // Set up RPC message handler BEFORE starting the client
        setupRpcMessageHandler();
        
        // Initialize RPC operation processor with RPC client reference
        DeviceConfig deviceConfig = config_.getConfig();
        operationProcessor_ = std::make_unique<RpcOperationProcessor>(deviceConfig, *rpcClient_, true);
        
        // Start RPC client after message handler is set
        if (!rpcClient_->start()) {
            LOG_ERROR("Failed to start RPC client");
            return false;
        }
        
        rpcRunning_.store(true);
        LOG_INFO("RPC system initialized successfully");
        
        // Initialize and start cron job AFTER RPC client is connected to broker
        if (!startCronJob()) {
            LOG_ERROR("Failed to start device discovery cron job");
            // Continue running even if cron job fails
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize RPC system: " + std::string(e.what()));
        return false;
    }
}

void DeviceManager::shutdownRpc() {
    if (rpcRunning_.load()) {
        rpcRunning_.store(false);
        
        // Stop cron job first
        stopCronJob();
        
        if (operationProcessor_) {
            operationProcessor_.reset();
        }
        
        if (rpcClient_) {
            rpcClient_->stop();
            rpcClient_.reset();
        }
        
        LOG_INFO("RPC system shutdown completed");
    }
}

bool DeviceManager::isRpcRunning() const {
    return rpcRunning_.load();
}

void DeviceManager::setupRpcMessageHandler() {
    if (rpcClient_) {
        // Set up message handler for incoming RPC requests
        rpcClient_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            // Topic filtering for selective processing
            if (topic.find("direct_messaging/ur-mavdiscovery/requests") == std::string::npos) {
                return; // Ignore unrelated topics
            }
            
            this->onRpcMessage(topic, payload);
        });
        
        LOG_INFO("RPC message handler configured for topic: direct_messaging/ur-mavdiscovery/requests");
    }
}

void DeviceManager::onRpcMessage(const std::string& topic, const std::string& payload) {
    try {
        LOG_INFO("Received RPC message on topic: " + topic);
        
        if (operationProcessor_) {
            operationProcessor_->processRequest(payload.c_str(), payload.length());
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error processing RPC message: " + std::string(e.what()));
    }
}

// Shared bus notification methods
void DeviceManager::sendDeviceVerifiedNotification(const DeviceInfo& info) {
    try {
        if (!rpcClient_ || !rpcClient_->isRunning()) {
            LOG_ERROR("RPC client not available for device verified notifications");
            return;
        }
        
        // Convert to shared DeviceInfo format
        auto sharedDeviceInfo = convertToSharedDeviceInfo(info);
        
        // Create device verified notification
        MavlinkShared::DeviceVerifiedNotification notification(sharedDeviceInfo);
        
        // Create notification JSON
        auto notificationJson = MavlinkShared::MavlinkEventSerializer::createDeviceVerifiedNotification(notification);
        std::string notificationPayload = notificationJson.dump();
        
        // Send to shared bus
        LOG_INFO("Sending device verified notification to ur-shared-bus");
        rpcClient_->sendResponse("ur-shared-bus/ur-mavlink-stack/notifications", notificationPayload);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send device verified notification: " + std::string(e.what()));
    }
}

void DeviceManager::sendDeviceRemovedSharedNotification(const std::string& devicePath) {
    try {
        if (!rpcClient_ || !rpcClient_->isRunning()) {
            LOG_ERROR("RPC client not available for device removed notifications");
            return;
        }
        
        // Create device removed notification
        MavlinkShared::DeviceRemovedNotification notification(devicePath);
        
        // Create notification JSON
        auto notificationJson = MavlinkShared::MavlinkEventSerializer::createDeviceRemovedNotification(notification);
        std::string notificationPayload = notificationJson.dump();
        
        // Send to shared bus
        LOG_INFO("Sending device removed notification to ur-shared-bus");
        rpcClient_->sendResponse("ur-shared-bus/ur-mavlink-stack/notifications", notificationPayload);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send device removed notification: " + std::string(e.what()));
    }
}

void DeviceManager::sendInitProcessDiscoveryNotification() {
    try {
        if (!rpcClient_ || !rpcClient_->isRunning()) {
            LOG_ERROR("RPC client not available for init process discovery");
            return;
        }
        
        // Get all verified devices from DeviceStateDB
        auto& deviceDB = DeviceStateDB::getInstance();
        auto allDevices = deviceDB.getAllDevices();
        
        std::vector<MavlinkShared::DeviceInfo> sharedDevices;
        for (const auto& device : allDevices) {
            if (device->state == DeviceState::VERIFIED) {
                sharedDevices.push_back(convertToSharedDeviceInfo(*device));
            }
        }
        
        // Create init process discovery notification
        MavlinkShared::InitProcessDiscoveryEvent discoveryEvent(sharedDevices);
        
        // Create notification JSON
        auto notificationJson = MavlinkShared::MavlinkEventSerializer::createInitProcessDiscoveryNotification(discoveryEvent);
        std::string notificationPayload = notificationJson.dump();
        
        // Send to shared bus
        LOG_INFO("Sending init process discovery notification to ur-shared-bus");
        rpcClient_->sendResponse("ur-shared-bus/ur-mavlink-stack/notifications", notificationPayload);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send init process discovery notification: " + std::string(e.what()));
    }
}

// Convert between old and new DeviceInfo formats
MavlinkShared::DeviceInfo DeviceManager::convertToSharedDeviceInfo(const DeviceInfo& info) {
    MavlinkShared::DeviceInfo sharedInfo;
    
    // Basic device information
    sharedInfo.devicePath = info.devicePath;
    
    // Convert DeviceState enum
    switch (info.state.load()) {
        case DeviceState::UNKNOWN:
            sharedInfo.state = MavlinkShared::DeviceState::UNKNOWN;
            break;
        case DeviceState::VERIFYING:
            sharedInfo.state = MavlinkShared::DeviceState::VERIFYING;
            break;
        case DeviceState::VERIFIED:
            sharedInfo.state = MavlinkShared::DeviceState::VERIFIED;
            break;
        case DeviceState::NON_MAVLINK:
            sharedInfo.state = MavlinkShared::DeviceState::NON_MAVLINK;
            break;
        case DeviceState::REMOVED:
            sharedInfo.state = MavlinkShared::DeviceState::REMOVED;
            break;
    }
    
    sharedInfo.baudrate = info.baudrate;
    sharedInfo.sysid = info.sysid;
    sharedInfo.compid = info.compid;
    sharedInfo.mavlinkVersion = info.mavlinkVersion;
    sharedInfo.timestamp = info.timestamp;
    
    // Convert USB info
    sharedInfo.usbInfo.deviceName = info.usbInfo.deviceName;
    sharedInfo.usbInfo.manufacturer = info.usbInfo.manufacturer;
    sharedInfo.usbInfo.serialNumber = info.usbInfo.serialNumber;
    sharedInfo.usbInfo.vendorId = info.usbInfo.vendorId;
    sharedInfo.usbInfo.productId = info.usbInfo.productId;
    sharedInfo.usbInfo.boardClass = info.usbInfo.boardClass;
    sharedInfo.usbInfo.boardName = info.usbInfo.boardName;
    sharedInfo.usbInfo.autopilotType = info.usbInfo.autopilotType;
    sharedInfo.usbInfo.usbBusNumber = info.usbInfo.usbBusNumber;
    sharedInfo.usbInfo.usbDeviceAddress = info.usbInfo.usbDeviceAddress;
    sharedInfo.usbInfo.physicalDeviceId = info.usbInfo.physicalDeviceId;
    
    // Convert MAVLink messages
    for (const auto& msg : info.messages) {
        MavlinkShared::MAVLinkMessage sharedMsg;
        sharedMsg.msgid = msg.msgid;
        sharedMsg.name = msg.name;
        sharedInfo.messages.push_back(sharedMsg);
    }
    
    return sharedInfo;
}

// Cron job management methods
bool DeviceManager::startCronJob() {
    try {
        // Wait for RPC client to be available with retry mechanism
        int retryCount = 0;
        const int maxRetries = 20;
        const int retryDelayMs = 500;
        
        while ((!rpcClient_ || !rpcClient_->isRunning()) && retryCount < maxRetries) {
            LOG_INFO("Waiting for RPC client to be available for cron job... (attempt " + 
                     std::to_string(retryCount + 1) + "/" + std::to_string(maxRetries) + ")");
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
            retryCount++;
        }
        
        if (!rpcClient_ || !rpcClient_->isRunning()) {
            LOG_ERROR("RPC client still not available for cron job after " + 
                     std::to_string(maxRetries) + " attempts");
            return false;
        }
        
        LOG_INFO("RPC client is now available, starting device discovery cron job");
        
        // Create cron job instance
        cronJob_ = std::make_unique<DeviceDiscoveryCronJob>(*threadManager_, *rpcClient_);
        
        // Start the cron job
        if (!cronJob_->start()) {
            LOG_ERROR("Failed to start device discovery cron job");
            cronJob_.reset();
            return false;
        }
        
        LOG_INFO("Device discovery cron job started successfully - will push notifications every 1 second");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start cron job: " + std::string(e.what()));
        cronJob_.reset();
        return false;
    }
}

void DeviceManager::stopCronJob() {
    if (cronJob_) {
        LOG_INFO("Stopping device discovery cron job...");
        cronJob_->stop();
        cronJob_.reset();
        LOG_INFO("Device discovery cron job stopped");
    }
}