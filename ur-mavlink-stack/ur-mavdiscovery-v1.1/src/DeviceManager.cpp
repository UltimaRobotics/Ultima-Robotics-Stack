#include "DeviceManager.hpp"
#ifdef HTTP_ENABLED
#include "HTTPClient.hpp"
#endif
#include "RpcClient.hpp"
#include "CallbackDispatcher.hpp"
#include "Logger.hpp"
#include <chrono>
#include <thread>

DeviceManager::DeviceManager() : running_(false), rpcRunning_(false) {}

DeviceManager::~DeviceManager() {
    shutdown();
}

bool DeviceManager::initialize(const std::string& configPath) {
    if (!config_.loadFromFile(configPath)) {
        LOG_ERROR("Failed to load configuration");
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

    // Set log file
    Logger::getInstance().setLogFile(deviceConfig.logFile);

    // Initialize ThreadManager with initial capacity
    threadManager_ = std::make_unique<ThreadMgr::ThreadManager>(20);
    ThreadMgr::ThreadManager::setLogLevel(ThreadMgr::LogLevel::Info);

    LOG_INFO("ThreadManager initialized with capacity: 20");

    monitor_ = std::make_unique<DeviceMonitor>(deviceConfig, *threadManager_);

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
    return true;
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
        // Send RPC device removed notification
        sendDeviceRemovedRpcNotifications(devicePath);

#ifdef HTTP_ENABLED
        DeviceConfig deviceConfig = config_.getConfig();
        if (deviceConfig.enableHTTP) {
            // Send stop request to MAVRouter mainloop
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
    
    // Send RPC notifications for verified device
    sendDeviceAddedRpcNotifications(info);
}

void DeviceManager::sendDeviceAddedRpcNotifications(const DeviceInfo& info) {
    try {
        // Use the main RPC client instead of creating a temporary one
        if (!rpcClient_ || !rpcClient_->isRunning()) {
            LOG_ERROR("RPC client not available for device added notifications");
            return;
        }
        
        // Build device info JSON for the request using helper method
        json deviceJson = info.toJson();
        
        // Create RPC request for mavlink added method
        json mavlinkAddedRequest;
        mavlinkAddedRequest["jsonrpc"] = "2.0";
        mavlinkAddedRequest["method"] = "mavlink_added";
        mavlinkAddedRequest["params"] = deviceJson;
        mavlinkAddedRequest["id"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        std::string requestPayload = mavlinkAddedRequest.dump();
        
        // Send to ur-mavrouter
        LOG_INFO("Sending mavlink_added request to ur-mavrouter");
        rpcClient_->getClient().publishMessage("direct_messaging/ur-mavrouter/requests", requestPayload);
        
        // Send to ur-mavcollector
        LOG_INFO("Sending mavlink_added request to ur-mavcollector");
        rpcClient_->getClient().publishMessage("direct_messaging/ur-mavcollector/requests", requestPayload);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send RPC device added notifications: " + std::string(e.what()));
    }
}

void DeviceManager::sendDeviceRemovedRpcNotifications(const std::string& devicePath) {
    try {
        // Use the main RPC client instead of creating a temporary one
        if (!rpcClient_ || !rpcClient_->isRunning()) {
            LOG_ERROR("RPC client not available for device removal notifications");
            return;
        }
        
        // Build device removal info JSON
        json deviceRemovedJson;
        deviceRemovedJson["devicePath"] = devicePath;
        deviceRemovedJson["timestamp"] = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        // Create RPC request for device removed method
        json deviceRemovedRequest;
        deviceRemovedRequest["jsonrpc"] = "2.0";
        deviceRemovedRequest["method"] = "device_removed";
        deviceRemovedRequest["params"] = deviceRemovedJson;
        deviceRemovedRequest["id"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        std::string requestPayload = deviceRemovedRequest.dump();
        
        // Send to ur-mavrouter
        LOG_INFO("Sending device_removed request to ur-mavrouter");
        rpcClient_->getClient().publishMessage("direct_messaging/ur-mavrouter/requests", requestPayload);
        
        // Send to ur-mavcollector
        LOG_INFO("Sending device_removed request to ur-mavcollector");
        rpcClient_->getClient().publishMessage("direct_messaging/ur-mavcollector/requests", requestPayload);
        
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
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize RPC system: " + std::string(e.what()));
        return false;
    }
}

void DeviceManager::shutdownRpc() {
    if (rpcRunning_.load()) {
        rpcRunning_.store(false);
        
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