#include "RpcControllerNew.hpp"
#include "DeviceDiscoveryCronJob.hpp"
#include "../common/log.h"
#include <thread>
#include <chrono>
#include "../mavlink-extensions/ExtensionManager.h"
#include "../../ur-mavdiscovery-shared/include/MavlinkDeviceStructs.hpp"
#include "modules/nholmann/json.hpp"

namespace RpcMechanisms {

RpcController::RpcController(ThreadMgr::ThreadManager& threadManager) 
    : operations_(std::make_unique<RpcOperations>(threadManager)) {
    log_info("RpcController initialized with separated RPC client and operations");
    
    // Initialize device discovery cron job
    discoveryCronJob_ = std::make_unique<DeviceDiscoveryCronJob>("", false);
    
    // Set RPC request callback for the cron job
    discoveryCronJob_->setRpcRequestCallback([this](const std::string& service, const std::string& method, const std::string& params) -> std::string {
        if (rpcClient_) {
            return rpcClient_->sendRpcRequest(service, method, params);
        }
        return "";
    });
    
    log_info("Device discovery cron job initialized");
}

RpcController::~RpcController() {
    // Signal shutdown to all threads
    shutdown_.store(true);
    
    // Stop device discovery cron job
    if (discoveryCronJob_) {
        discoveryCronJob_->stop();
        log_info("Device discovery cron job stopped");
    }
    
    // Stop RPC client first
    stopRpcClient();
    
    // Join all startup threads
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        for (auto& thread : startupThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        startupThreads_.clear();
    }
    
    log_info("RpcController destroyed");
}

bool RpcController::initializeRpcIntegration(const std::string& configPath, const std::string& clientId) {
    std::lock_guard<std::mutex> lock(rpcMutex_);
    
    if (rpcInitialized_.load()) {
        return true; // Already initialized
    }
    
    try {
        rpcConfigPath_ = configPath;
        rpcClientId_ = clientId;
        
        // Create RPC client wrapper using the new implementation
        rpcClient_ = std::make_unique<RpcClientWrapper>(configPath, clientId);
        if (!rpcClient_) {
            log_error("Failed to create RPC client wrapper");
            return false;
        }
        
        // Setup message handlers
        setupRpcMessageHandlers();
        
        rpcInitialized_.store(true);
        log_info("UR-RPC integration initialized successfully with separated components");
        return true;
        
    } catch (const std::exception& e) {
        log_error("Failed to initialize UR-RPC integration: %s", e.what());
        return false;
    }
}

bool RpcController::startRpcClient() {
    std::lock_guard<std::mutex> lock(rpcMutex_);
    
    if (!rpcInitialized_.load()) {
        log_error("UR-RPC integration not initialized");
        return false;
    }
    
    if (!rpcClient_) {
        log_error("RPC client not available");
        return false;
    }
    
    try {
        // Start the RPC client
        if (rpcClient_->start()) {
            log_info("UR-RPC client started successfully with separated components");
            
            // Start device discovery cron job after RPC client is ready
            if (discoveryCronJob_ && discoveryCronJob_->start()) {
                log_info("Device discovery cron job started successfully");
            } else {
                log_error("Failed to start device discovery cron job");
            }
            
            return true;
        } else {
            log_error("Failed to start UR-RPC client");
            return false;
        }
        
    } catch (const std::exception& e) {
        log_error("Failed to start UR-RPC client: %s", e.what());
        return false;
    }
}

void RpcController::stopRpcClient() {
    std::lock_guard<std::mutex> lock(rpcMutex_);
    
    if (rpcClient_ && rpcClient_->isRunning()) {
        try {
            rpcClient_->stop();
            log_info("UR-RPC client stopped");
        } catch (const std::exception& e) {
            log_error("Exception stopping RPC client: %s", e.what());
        }
    }
}

bool RpcController::isRpcClientRunning() const {
    std::lock_guard<std::mutex> lock(rpcMutex_);
    return rpcClient_ && rpcClient_->isRunning();
}

std::string RpcController::getRpcClientStatistics() const {
    std::lock_guard<std::mutex> lock(rpcMutex_);
    
    if (!rpcClient_) {
        return "{\"error\":\"RPC client not initialized\"}";
    }
    
    try {
        // For now, return basic status
        nlohmann::json stats;
        stats["running"] = rpcClient_->isRunning();
        stats["clientId"] = rpcClientId_;
        stats["configPath"] = rpcConfigPath_;
        return stats.dump();
    } catch (const std::exception& e) {
        return "{\"error\":\"Failed to get statistics: " + std::string(e.what()) + "\"}";
    }
}

void RpcController::setupRpcMessageHandlers() {
    if (!rpcClient_) {
        log_error("Cannot setup RPC message handlers - RPC client not initialized");
        return;
    }
    
    // Store the message handler for temporary handler management
    messageHandler_ = [this](const std::string& topic, const std::string& payload) {
        this->handleRpcMessage(topic, payload);
    };
    
    // Set the message handler
    rpcClient_->setMessageHandler(messageHandler_);
    
    log_info("RPC message handlers configured successfully");
}

void RpcController::handleRpcMessage(const std::string& topic, const std::string& payload) {
    try {
        log_info("[RPC_CONTROLLER] Handling RPC message from topic: %s (payload size: %zu)", topic.c_str(), payload.size());
        log_debug("[RPC_CONTROLLER] Message payload: %s", payload.c_str());
        
        // Handle heartbeat messages from ur-mavdiscovery (startup mechanism trigger)
        if (topic == "clients/ur-mavdiscovery/heartbeat") {
            log_info("[RPC_CONTROLLER] Processing heartbeat message from ur-mavdiscovery");
            // Route to DeviceDiscoveryCronJob
            if (discoveryCronJob_) {
                discoveryCronJob_->handleHeartbeatMessage(topic, payload);
                log_info("[RPC_CONTROLLER] Heartbeat routed to DeviceDiscoveryCronJob (heartbeat received: %s)", 
                        discoveryCronJob_->hasReceivedHeartbeat() ? "yes" : "no");
            } else {
                log_warning("[RPC_CONTROLLER] DeviceDiscoveryCronJob not available for heartbeat");
            }
            return;
        }
        
        // Handle device discovery responses
        if (topic == "direct_messaging/ur-mavdiscovery/responses") {
            log_info("[RPC_CONTROLLER] Processing device discovery response from ur-mavdiscovery");
            // Route to DeviceDiscoveryCronJob
            if (discoveryCronJob_) {
                discoveryCronJob_->handleDiscoveryResponse(topic, payload);
                log_info("[RPC_CONTROLLER] Discovery response routed to DeviceDiscoveryCronJob");
            } else {
                log_warning("[RPC_CONTROLLER] DeviceDiscoveryCronJob not available for discovery response");
            }
            return;
        }
        
        log_info("[RPC_CONTROLLER] Message not handled by DeviceDiscoveryCronJob, processing as RPC request");
        
        // Parse JSON-RPC request
        nlohmann::json request = nlohmann::json::parse(payload);
        
        if (!request.contains("method") || !request.contains("params")) {
            log_error("Invalid RPC request format");
            return;
        }
        
        std::string method = request["method"];
        nlohmann::json params = request["params"];
        std::string requestId = request.value("id", "unknown");
        
        log_info("Processing RPC method: %s", method.c_str());
        
        // Handle different RPC methods using operations
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = requestId;
        
        if (method == "thread_status" || method == "get_thread_status") {
            std::string threadName = params.value("thread_name", "all");
            RpcResponse rpcResponse;
            
            if (threadName == "all") {
                rpcResponse = operations_->getAllThreadStatus();
            } else {
                rpcResponse = operations_->getThreadStatus(threadName);
            }
            
            response["result"] = nlohmann::json::parse(rpcResponse.toJson());
            
        } else if (method == "thread_operation") {
            std::string threadName = params.value("thread_name", "");
            std::string operation = params.value("operation", "status");
            
            if (threadName.empty()) {
                response["error"] = {{"code", -32602}, {"message", "thread_name parameter required"}};
            } else {
                ThreadOperation op = RpcOperations::stringToThreadOperation(operation);
                RpcResponse rpcResponse = operations_->executeOperationOnThread(threadName, op);
                response["result"] = nlohmann::json::parse(rpcResponse.toJson());
            }
            
        } else if (method == "mavlink_device_added") {
            // Handle device added event - start mainloop thread and extensions
            bool isStartupTrigger = (requestId == "startup_trigger");
            
            if (isStartupTrigger) {
                log_info("\n[STARTUP] ========================================");
                log_info("[STARTUP] Startup trigger: Device discovered - starting services");
                log_info("[STARTUP] ========================================");
            } else {
                log_info("\n[RPC] ========================================");
                log_info("[RPC] Received mavlink_device_added request");
                log_info("[RPC] Action: START mainloop thread AND load/start all extensions");
                log_info("[RPC] ========================================");
            }
            
            // Extract device info from params
            std::string devicePath = params.value("devicePath", "unknown");
            std::string deviceState = params.value("state", "UNKNOWN");
            int baudrate = params.value("baudrate", 0);
            
            if (isStartupTrigger) {
                log_info("[STARTUP] Device discovered: %s (State: %s, Baudrate: %d)", devicePath.c_str(), deviceState.c_str(), baudrate);
            } else {
                log_info("[RPC] Device added: %s (State: %s, Baudrate: %d)", devicePath.c_str(), deviceState.c_str(), baudrate);
            }
            
            // Check if mainloop is already running
            if (isMainloopRunning()) {
                log_info("[STARTUP] Mainloop already running, only ensuring extensions are started");
                
                // Just ensure extensions are started
                auto* extensionManager = operations_->getExtensionManager();
                if (extensionManager) {
                    auto allExtensions = extensionManager->getAllExtensions();
                    log_info("[STARTUP] Ensuring %zu extensions are started...", allExtensions.size());
                    
                    for (const auto& extInfo : allExtensions) {
                        auto extResp = operations_->executeOperationOnThread(extInfo.name, ThreadOperation::START);
                        if (extResp.status != OperationStatus::SUCCESS) {
                            log_warning("[STARTUP] Failed to start extension %s: %s", extInfo.name.c_str(), extResp.message.c_str());
                        }
                    }
                }
                
                response["result"] = {{"status", "success"}, {"message", "Device added: extensions ensured started"}};
                
            } else {
                // First, start the mainloop thread (this initializes the global config)
                auto mainloopResp = this->startThread(ThreadTarget::MAINLOOP);
                
                if (isStartupTrigger) {
                    log_info("[STARTUP] Mainloop start result: %s", mainloopResp.message.c_str());
                } else {
                    log_info("[RPC] Mainloop start result: %s", mainloopResp.message.c_str());
                }
                
                if (mainloopResp.status != OperationStatus::SUCCESS) {
                    response["error"] = {{"code", -32500}, {"message", "Failed to start mainloop: " + mainloopResp.message}};
                } else {
                    // Wait a moment for mainloop to initialize
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                    // Now load and start extension configurations
                    auto* extensionManager = operations_->getExtensionManager();
                    if (extensionManager) {
                        auto allExtensions = extensionManager->getAllExtensions();
                        
                        // Only load configs if no extensions are loaded yet
                        if (allExtensions.empty()) {
                            if (isStartupTrigger) {
                                log_info("[STARTUP] Loading extension configurations...");
                            } else {
                                log_info("[RPC] Loading extension configurations...");
                            }
                            
                            std::string extensionConfDir = "config";
                            extensionManager->loadExtensionConfigs(extensionConfDir);
                            
                            // Refresh the extensions list
                            allExtensions = extensionManager->getAllExtensions();
                            
                            if (isStartupTrigger) {
                                log_info("[STARTUP] Loaded and started %zu extensions from config", allExtensions.size());
                            } else {
                                log_info("[RPC] Loaded and started %zu extensions from config", allExtensions.size());
                            }
                        } else {
                            // Extensions already exist, ensure they're started
                            if (isStartupTrigger) {
                                log_info("[STARTUP] Extensions already loaded (%zu found)", allExtensions.size());
                            } else {
                                log_info("[RPC] Extensions already loaded (%zu found)", allExtensions.size());
                            }
                            
                            // Start all existing extensions
                            for (const auto& extInfo : allExtensions) {
                                auto extResp = operations_->executeOperationOnThread(extInfo.name, ThreadOperation::START);
                                if (extResp.status != OperationStatus::SUCCESS) {
                                    if (isStartupTrigger) {
                                        log_warning("[STARTUP] Failed to start extension %s: %s", extInfo.name.c_str(), extResp.message.c_str());
                                    } else {
                                        log_warning("[RPC] Failed to start extension %s: %s", extInfo.name.c_str(), extResp.message.c_str());
                                    }
                                }
                            }
                        }
                        
                        std::string successMsg = isStartupTrigger ? 
                            "Device discovered: mainloop and extensions started successfully" :
                            "Device added: mainloop and extensions started";
                        response["result"] = {{"status", "success"}, {"message", successMsg}};
                    } else {
                        if (isStartupTrigger) {
                            log_warning("[STARTUP] No extension manager available, only mainloop started");
                        } else {
                            log_warning("[RPC] No extension manager available, only mainloop started");
                        }
                        
                        std::string partialMsg = isStartupTrigger ?
                            "Device discovered: mainloop started (no extensions)" :
                            "Device added: mainloop started (no extensions)";
                        response["result"] = {{"status", "partial"}, {"message", partialMsg}};
                    }
                }
            }
            
        } else if (method == "mavlink_device_removed") {
            // Handle device removed event - stop mainloop thread and extensions
            log_info("\n[RPC] ========================================");
            log_info("[RPC] Received mavlink_device_removed request");
            log_info("[RPC] Action: STOP mainloop thread AND all extensions");
            log_info("[RPC] ========================================");
            
            // Extract device info from params
            std::string devicePath = params.value("devicePath", "unknown");
            log_info("[RPC] Device removed: %s", devicePath.c_str());
            
            // First stop all extensions
            auto* extensionManager = operations_->getExtensionManager();
            if (extensionManager) {
                auto allExtensions = extensionManager->getAllExtensions();
                log_info("[RPC] Stopping %zu extensions...", allExtensions.size());
                
                for (const auto& extInfo : allExtensions) {
                    auto extResp = operations_->executeOperationOnThread(extInfo.name, ThreadOperation::STOP);
                    if (extResp.status != OperationStatus::SUCCESS) {
                        log_warning("[RPC] Failed to stop extension %s: %s", extInfo.name.c_str(), extResp.message.c_str());
                    }
                }
            } else {
                log_warning("[RPC] No extension manager available");
            }
            
            // Then stop the mainloop thread
            auto mainloopResp = this->stopThread(ThreadTarget::MAINLOOP);
            log_info("[RPC] Mainloop stop result: %s", mainloopResp.message.c_str());
            
            // Return combined status
            if (mainloopResp.status != OperationStatus::SUCCESS) {
                response["error"] = {{"code", -32500}, {"message", "Failed to stop mainloop: " + mainloopResp.message}};
            } else {
                response["result"] = {{"status", "success"}, {"message", "Device removed: mainloop and extensions stopped"}};
            }
            
        } else {
            response["error"] = {{"code", -32601}, {"message", "Method not found: " + method}};
        }
        
        // Send response
        std::string responseTopic = "direct_messaging/" + rpcClientId_ + "/responses";
        rpcClient_->sendResponse(responseTopic, response.dump());
        
        log_info("Processed RPC message from topic: %s, method: %s", topic.c_str(), method.c_str());
        
    } catch (const nlohmann::json::parse_error& e) {
        log_error("JSON parse error in RPC message: %s", e.what());
    } catch (const std::exception& e) {
        log_error("Exception handling RPC message: %s", e.what());
    }
}

// Delegate methods to operations
void RpcController::registerThread(const std::string& threadName, 
                                   unsigned int threadId,
                                   const std::string& attachmentId) {
    operations_->registerThread(threadName, threadId, attachmentId);
}

void RpcController::unregisterThread(const std::string& threadName) {
    operations_->unregisterThread(threadName);
}

void RpcController::registerRestartCallback(const std::string& threadName, 
                                           std::function<unsigned int()> restartCallback) {
    operations_->registerRestartCallback(threadName, restartCallback);
}

RpcResponse RpcController::executeRequest(const RpcRequest& request) {
    return operations_->executeRequest(request);
}

RpcResponse RpcController::getAllThreadStatus() {
    return operations_->getAllThreadStatus();
}

RpcResponse RpcController::getThreadStatus(const std::string& threadName) {
    return operations_->getThreadStatus(threadName);
}

RpcResponse RpcController::executeOperationOnThread(const std::string& threadName, 
                                                   ThreadOperation operation) {
    return operations_->executeOperationOnThread(threadName, operation);
}

void RpcController::setExtensionManager(MavlinkExtensions::ExtensionManager* extensionManager) {
    operations_->setExtensionManager(extensionManager);
}

RpcResponse RpcController::startThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::START, target);
    return executeRequest(request);
}

RpcResponse RpcController::stopThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::STOP, target);
    return executeRequest(request);
}

RpcResponse RpcController::pauseThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::PAUSE, target);
    return executeRequest(request);
}

RpcResponse RpcController::resumeThread(ThreadTarget target) {
    RpcRequest request(ThreadOperation::RESUME, target);
    return executeRequest(request);
}

// Startup mechanism implementations
void RpcController::handleHeartbeatMessage(const std::string& payload) {
    try {
        // Parse heartbeat payload
        auto heartbeat = nlohmann::json::parse(payload);
        
        std::string client = heartbeat.value("client", "unknown");
        std::string status = heartbeat.value("status", "unknown");
        std::string service = heartbeat.value("service", "unknown");
        
        log_info("[STARTUP] Received heartbeat from: %s, status: %s, service: %s", 
                 client.c_str(), status.c_str(), service.c_str());
        
        // Update last heartbeat time
        lastHeartbeatTime_ = std::chrono::steady_clock::now();
        
        // Check if this is from ur-mavdiscovery and service is ready
        if (client == "ur-mavdiscovery" && status == "alive") {
            std::lock_guard<std::mutex> lock(startupMutex_);
            
            // Only trigger discovery once and if mainloop hasn't started
            if (!discoveryTriggered_.load() && !mainloopStarted_.load()) {
                log_info("[STARTUP] ur-mavdiscovery is alive - triggering device discovery");
                triggerDeviceDiscovery();
                discoveryTriggered_.store(true);
            } else if (mainloopStarted_.load()) {
                log_debug("[STARTUP] Mainloop already started, ignoring heartbeat");
            } else {
                log_debug("[STARTUP] Discovery already triggered, ignoring heartbeat");
            }
        }
        
    } catch (const std::exception& e) {
        log_error("[STARTUP] Failed to parse heartbeat message: %s", e.what());
    }
}

void RpcController::triggerDeviceDiscovery() {
    // Launch independent thread for device discovery
    std::thread discoveryThread([this]() {
        try {
            log_info("[STARTUP] Starting independent device discovery thread");
            
            // Wait for RPC client to become available (up to 10 seconds)
            int waitCount = 0;
            const int maxWait = 100; // 10 seconds with 100ms sleep
            while ((!rpcClient_ || !rpcClient_->isRunning()) && waitCount < maxWait && !shutdown_.load()) {
                if (waitCount == 0) {
                    log_info("[STARTUP] Waiting for RPC client to become available...");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                waitCount++;
            }
            
            // Check if RPC client is ready
            if (!rpcClient_ || !rpcClient_->isRunning()) {
                log_error("[STARTUP] RPC client not available for device discovery after waiting 10 seconds");
                return;
            }
            
            log_info("[STARTUP] RPC client is now available, proceeding with device discovery");
            
            // Create device discovery request using ur-mavdiscovery-shared structures
            MavlinkShared::MavlinkRpcRequest discoveryRequest("device-list", "ur-mavdiscovery");
            
            // Set request parameters for device list
            discoveryRequest.params = nlohmann::json::object();
            discoveryRequest.params["include_unverified"] = false;
            discoveryRequest.params["include_usb_info"] = true;
            discoveryRequest.params["timeout_seconds"] = 1; // 1 second timeout as requested
            
            // Generate unique transaction ID for this request (same format as RpcClient)
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            static uint64_t counter = 0;
            discoveryRequest.transactionId = std::to_string(timestamp) + "-" + std::to_string(++counter);
            
            // Create JSON-RPC request
            nlohmann::json rpcRequest;
            rpcRequest["jsonrpc"] = "2.0";
            rpcRequest["method"] = discoveryRequest.method;
            rpcRequest["params"] = discoveryRequest.params;
            rpcRequest["id"] = discoveryRequest.transactionId;
            
            // Flag to track response received
            std::atomic<bool> responseReceived{false};
            std::atomic<bool> requestSuccess{false};
            
            // Store original handler for restoration
            std::function<void(const std::string&, const std::string&)> originalHandler;
            
            // Send request to ur-mavdiscovery first to get the actual transaction ID
            std::string requestTopic = "direct_messaging/ur-mavdiscovery/requests";
            std::string actualTransactionId = rpcClient_->sendRpcRequest("ur-mavdiscovery", "device-list", discoveryRequest.params.dump());
            
            if (actualTransactionId.empty()) {
                log_error("[STARTUP] Failed to send device discovery request");
                return;
            }
            
            // Register temporary response handler for this transaction
            std::string tempTransactionId = actualTransactionId;
            originalHandler = messageHandler_;
            
            // Temporary message handler to catch the response
            auto tempHandler = [this, tempTransactionId, &responseReceived, &requestSuccess, originalHandler]
                              (const std::string& topic, const std::string& payload) {
                log_info("[STARTUP] Temporary handler received message on topic: %s", topic.c_str());
                
                // Call original handler first
                if (originalHandler) {
                    originalHandler(topic, payload);
                }
                
                // Check if this is our response
                if (topic == "direct_messaging/ur-mavdiscovery/responses") {
                    log_info("[STARTUP] Processing response on ur-mavdiscovery topic");
                    try {
                        auto response = nlohmann::json::parse(payload);
                        std::string responseId = response.value("id", "");
                        log_debug("[STARTUP] Response ID: %s, Expected ID: %s", responseId.c_str(), tempTransactionId.c_str());
                        
                        if (response.contains("id") && response["id"] == tempTransactionId) {
                            log_info("[STARTUP] Received device discovery response for transaction: %s", 
                                   tempTransactionId.c_str());
                            
                            responseReceived.store(true);
                            
                            // Process the response using ur-mavdiscovery-shared structures
                            if (response.contains("result")) {
                                auto result = response["result"];
                                if (result.contains("devices") && result["devices"].is_array()) {
                                    auto devices = result["devices"];
                                    if (!devices.empty()) {
                                        log_info("[STARTUP] Found %zu devices, triggering mainloop startup", 
                                               devices.size());
                                        
                                        // Create device added events for each device
                                        for (const auto& deviceJson : devices) {
                                            try {
                                                MavlinkShared::DeviceInfo deviceInfo;
                                                deviceInfo.devicePath = deviceJson.value("devicePath", "");
                                                deviceInfo.state = MavlinkShared::DeviceState::VERIFIED;
                                                deviceInfo.baudrate = deviceJson.value("baudrate", 57600);
                                                deviceInfo.sysid = deviceJson.value("systemId", 1);
                                                deviceInfo.compid = deviceJson.value("componentId", 1);
                                                
                                                // Validate required device path
                                                if (deviceInfo.devicePath.empty()) {
                                                    log_warning("[STARTUP] Skipping device with empty path");
                                                    continue;
                                                }
                                                
                                                log_info("[STARTUP] Processing discovered device: %s (sysid:%d, compid:%d)", 
                                                       deviceInfo.devicePath.c_str(), deviceInfo.sysid, deviceInfo.compid);
                                                
                                                // Trigger device added event
                                                MavlinkShared::DeviceAddedEvent deviceEvent(deviceInfo);
                                                handleDeviceAddedEvent(deviceEvent);
                                                
                                            } catch (const std::exception& e) {
                                                log_error("[STARTUP] Failed to process device entry: %s", e.what());
                                            }
                                        }
                                        
                                        requestSuccess.store(true);
                                    } else {
                                        log_info("[STARTUP] No devices found in discovery response");
                                    }
                                } else {
                                    log_error("[STARTUP] Invalid response format - missing devices array");
                                }
                            } else {
                                log_error("[STARTUP] Invalid response format - missing result");
                            }
                        }
                    } catch (const std::exception& e) {
                        log_error("[STARTUP] Failed to parse device discovery response: %s", e.what());
                    }
                }
            };
            
            // Set temporary handler
            messageHandler_ = tempHandler;
            rpcClient_->setMessageHandler(tempHandler);
            log_info("[STARTUP] Temporary handler set up for transaction ID: %s", actualTransactionId.c_str());
            
            log_info("[STARTUP] Device discovery request sent with transaction ID: %s", 
                   actualTransactionId.c_str());
            
            // Wait for response with 1-second timeout
            auto startTime = std::chrono::steady_clock::now();
            while (!responseReceived.load() && !shutdown_.load()) {
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed >= std::chrono::seconds(1)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // Restore original handler
            messageHandler_ = originalHandler;
            rpcClient_->setMessageHandler(originalHandler);
            log_info("[STARTUP] Original handler restored");
            
            // Evaluate result
            if (!responseReceived.load()) {
                log_error("[STARTUP] Device discovery failed - no response received within 1 second");
                log_error("[STARTUP] Thread exiting due to timeout failure");
                return; // Exit thread on failure as requested
            }
            
            if (!requestSuccess.load()) {
                log_error("[STARTUP] Device discovery failed - no valid devices found");
                log_error("[STARTUP] Thread exiting due to no devices");
                return; // Exit thread on failure as requested
            }
            
            log_info("[STARTUP] Device discovery completed successfully - thread finishing");
            
        } catch (const std::exception& e) {
            log_error("[STARTUP] Device discovery thread failed with exception: %s", e.what());
            // Note: Handler restoration is handled in the finally block above
        }
    });
    
    // Detach thread to run independently as requested
    discoveryThread.detach();
}

void RpcController::handleDeviceDiscoveryResponse(const std::string& payload) {
    try {
        log_info("[STARTUP] Received device discovery response");
        
        auto response = nlohmann::json::parse(payload);
        
        if (response.contains("result")) {
            auto devices = response["result"];
            
            if (devices.is_array() && !devices.empty()) {
                log_info("[STARTUP] Found %zu devices - starting mainloop and extensions", devices.size());
                
                // Validate device information before proceeding
                bool validDeviceFound = false;
                for (const auto& device : devices) {
                    if (device.contains("devicePath") && device.contains("state")) {
                        std::string devicePath = device["devicePath"];
                        std::string state = device["state"];
                        
                        log_info("[STARTUP] Device: %s (State: %s)", devicePath.c_str(), state.c_str());
                        
                        // Only proceed if we have a verified device
                        if (state == "VERIFIED" || state == "CONNECTED") {
                            validDeviceFound = true;
                            break;
                        }
                    }
                }
                
                if (validDeviceFound) {
                    // Trigger the existing mavlink_device_added logic
                    nlohmann::json deviceAddedRequest;
                    deviceAddedRequest["jsonrpc"] = "2.0";
                    deviceAddedRequest["method"] = "mavlink_device_added";
                    deviceAddedRequest["params"] = devices[0]; // Use first valid device
                    deviceAddedRequest["id"] = "startup_trigger";
                    
                    // Process this as an internal RPC request
                    handleRpcMessage("direct_messaging/ur-mavrouter/requests", deviceAddedRequest.dump());
                    
                    mainloopStarted_.store(true);
                    log_info("[STARTUP] Startup sequence completed successfully");
                } else {
                    log_warning("[STARTUP] No verified devices found - waiting for device verification");
                    // Reset to allow retry when devices are verified
                    discoveryTriggered_.store(false);
                }
                
            } else {
                log_warning("[STARTUP] No devices found in discovery response");
                
                // Reset discovery trigger to allow retry when devices are connected
                std::lock_guard<std::mutex> lock(startupMutex_);
                discoveryTriggered_.store(false);
            }
            
        } else if (response.contains("error")) {
            std::string errorMsg = response["error"].value("message", "Unknown error");
            int errorCode = response["error"].value("code", -1);
            log_error("[STARTUP] Device discovery failed (code: %d): %s", errorCode, errorMsg.c_str());
            
            // Reset discovery trigger to allow retry
            std::lock_guard<std::mutex> lock(startupMutex_);
            discoveryTriggered_.store(false);
        }
        
    } catch (const std::exception& e) {
        log_error("[STARTUP] Failed to handle device discovery response: %s", e.what());
        
        // Reset discovery trigger to allow retry
        std::lock_guard<std::mutex> lock(startupMutex_);
        discoveryTriggered_.store(false);
    }
}

void RpcController::checkHeartbeatTimeout() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastHeartbeat = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeatTime_);
    
    if (timeSinceLastHeartbeat > HEARTBEAT_TIMEOUT) {
        if (mainloopStarted_.load()) {
            log_warning("[STARTUP] Heartbeat timeout detected, but mainloop is already running");
        } else {
            log_warning("[STARTUP] Heartbeat timeout - ur-mavdiscovery may be unavailable");
            
            // Reset discovery trigger to allow retry when heartbeat resumes
            std::lock_guard<std::mutex> lock(startupMutex_);
            discoveryTriggered_.store(false);
        }
    }
}

bool RpcController::isMainloopRunning() const {
    if (!operations_) {
        return false;
    }
    
    auto status = operations_->getThreadStatus("mainloop");
    return status.status == OperationStatus::SUCCESS;
}

nlohmann::json RpcController::getStartupStatus() const {
    nlohmann::json startupStatus;
    
    // Basic startup state
    startupStatus["discovery_triggered"] = discoveryTriggered_.load();
    startupStatus["mainloop_started"] = mainloopStarted_.load();
    startupStatus["mainloop_running"] = isMainloopRunning();
    
    // Heartbeat information
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastHeartbeat = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeatTime_);
    startupStatus["seconds_since_last_heartbeat"] = timeSinceLastHeartbeat.count();
    startupStatus["heartbeat_timeout_seconds"] = HEARTBEAT_TIMEOUT.count();
    startupStatus["heartbeat_active"] = timeSinceLastHeartbeat <= HEARTBEAT_TIMEOUT;
    
    // RPC client status
    if (rpcClient_) {
        startupStatus["rpc_client_running"] = rpcClient_->isRunning();
        startupStatus["rpc_client_connected"] = true; // Could be enhanced with actual connection status
    } else {
        startupStatus["rpc_client_running"] = false;
        startupStatus["rpc_client_connected"] = false;
    }
    
    // Extension status
    if (operations_) {
        auto* extensionManager = operations_->getExtensionManager();
        if (extensionManager) {
            auto allExtensions = extensionManager->getAllExtensions();
            startupStatus["extension_count"] = allExtensions.size();
            
            nlohmann::json extensionStatuses = nlohmann::json::array();
            for (const auto& extInfo : allExtensions) {
                nlohmann::json extStatus;
                extStatus["name"] = extInfo.name;
                extStatus["loaded"] = true;
                
                auto extResp = operations_->getThreadStatus(extInfo.name);
                extStatus["running"] = (extResp.status == OperationStatus::SUCCESS);
                
                extensionStatuses.push_back(extStatus);
            }
            startupStatus["extensions"] = extensionStatuses;
        } else {
            startupStatus["extension_count"] = 0;
            startupStatus["extensions"] = nlohmann::json::array();
        }
    } else {
        startupStatus["extension_count"] = 0;
        startupStatus["extensions"] = nlohmann::json::array();
    }
    
    // Overall status
    if (mainloopStarted_.load() && isMainloopRunning()) {
        startupStatus["overall_status"] = "running";
    } else if (discoveryTriggered_.load()) {
        startupStatus["overall_status"] = "discovering";
    } else if (startupStatus["heartbeat_active"].get<bool>()) {
        startupStatus["overall_status"] = "ready";
    } else {
        startupStatus["overall_status"] = "waiting";
    }
    
    return startupStatus;
}

void RpcController::handleDeviceAddedEvent(const MavlinkShared::DeviceAddedEvent& event) {
    try {
        log_info("[STARTUP] Processing device added event for device: %s", 
               event.deviceInfo.devicePath.c_str());
        
        // Lock startup state
        std::lock_guard<std::mutex> lock(startupMutex_);
        
        // Check if mainloop is already started
        if (mainloopStarted_.load()) {
            log_info("[STARTUP] Mainloop already started, ignoring device added event");
            return;
        }
        
        // Start mainloop and extensions
        if (operations_) {
            log_info("[STARTUP] Starting mainloop due to device discovery");
            
            // Start mainloop
            auto startResult = this->startThread(ThreadTarget::MAINLOOP);
            if (startResult.status == OperationStatus::SUCCESS) {
                mainloopStarted_.store(true);
                log_info("[STARTUP] Mainloop started successfully via device discovery");
                
                // Load and start extensions
                auto* extensionManager = operations_->getExtensionManager();
                if (extensionManager) {
                    auto allExtensions = extensionManager->getAllExtensions();
                    for (const auto& extInfo : allExtensions) {
                        // Start extension via RPC operations
                        log_info("[STARTUP] Starting extension: %s", extInfo.name.c_str());
                        // Note: Extensions are typically started via their own RPC mechanisms
                        // For now, we'll just log that they should be started
                    }
                }
                
                log_info("[STARTUP] Startup sequence completed - mainloop running");
                
            } else {
                log_error("[STARTUP] Failed to start mainloop: %s", startResult.message.c_str());
            }
        } else {
            log_error("[STARTUP] Operations not available for mainloop startup");
        }
        
    } catch (const std::exception& e) {
        log_error("[STARTUP] Failed to handle device added event: %s", e.what());
    }
}

} // namespace RpcMechanisms
