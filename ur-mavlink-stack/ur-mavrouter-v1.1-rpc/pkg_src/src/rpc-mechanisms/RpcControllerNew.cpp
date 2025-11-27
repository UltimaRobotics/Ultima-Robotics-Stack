#include "RpcControllerNew.hpp"
#include "DeviceDiscoveryCronJob.hpp"
#include "../common/log.h"
#include "../common/json_config.h"
#include "../mainloop.h"
#include <thread>
#include <chrono>
#include <fstream>
#include "../mavlink-extensions/ExtensionManager.h"
#include "../../ur-mavdiscovery-shared/include/MavlinkDeviceStructs.hpp"
#include "modules/nholmann/json.hpp"

namespace RpcMechanisms {

RpcController::RpcController(ThreadMgr::ThreadManager& threadManager, const std::string& routerConfigPath) 
    : operations_(std::make_unique<RpcOperations>(threadManager, routerConfigPath)),
      routerConfigPath_(routerConfigPath),
      startupTime_(std::chrono::system_clock::now()) {
    log_info("RpcController initialized with separated RPC client and operations");
    
    if (!routerConfigPath_.empty()) {
        log_info("RpcController: Using router configuration path: %s", routerConfigPath_.c_str());
    } else {
        log_warning("RpcController: No router configuration path provided");
    }
    
    // Initialize device discovery cron job
    discoveryCronJob_ = std::make_unique<DeviceDiscoveryCronJob>("", false);
    
    // Set RPC request callback for the cron job
    discoveryCronJob_->setRpcRequestCallback([this](const std::string& service, const std::string& method, const std::string& params) -> std::string {
        if (rpcClient_) {
            return rpcClient_->sendRpcRequest(service, method, params);
        }
        return "";
    });
    
    // Set mainloop startup callback for the cron job
    discoveryCronJob_->setMainloopStartupCallback([this](const MavlinkShared::DeviceInfo& deviceInfo) -> std::string {
        log_info("[STARTUP] ========================================");
        log_info("[STARTUP] Startup trigger: Device discovered - starting services");
        log_info("[STARTUP] Device: %s (baudrate: %d, sysid:%d, compid:%d)", 
                deviceInfo.devicePath.c_str(), deviceInfo.baudrate, deviceInfo.sysid, deviceInfo.compid);
        log_info("[STARTUP] ========================================");
        
        // Update router configuration with discovered device info
        if (updateRouterConfigWithDevice(deviceInfo)) {
            log_info("[STARTUP] Router configuration updated successfully");
        } else {
            log_warning("[STARTUP] Failed to update router configuration, using existing config");
        }
        
        // Start mainloop using the same logic as handleDeviceAddedEvent
        if (operations_) {
            log_info("[STARTUP] Starting mainloop due to device discovery");
            
            // Start mainloop
            auto startResult = this->startThread(ThreadTarget::MAINLOOP);
            if (startResult.status == OperationStatus::SUCCESS) {
                mainloopStarted_.store(true);
                log_info("[STARTUP] Mainloop started successfully via device discovery");
                
                // Wait for mainloop to enter event loop before starting extensions
                log_info("[STARTUP] Waiting for mainloop to enter event loop before loading extensions...");
                bool mainloopReady = Mainloop::wait_for_event_loop(5000); // 5 second timeout
                
                if (mainloopReady) {
                    log_info("[STARTUP] Mainloop is in event loop, loading and starting extensions");
                    
                    // Load and start extension configurations
                    auto* extensionManager = operations_->getExtensionManager();
                    if (extensionManager) {
                        auto allExtensions = extensionManager->getAllExtensions();
                        
                        // Only load configs if no extensions are loaded yet
                        if (allExtensions.empty()) {
                            log_info("[STARTUP] Loading extension configurations...");
                            std::string extensionConfDir = "config";
                            bool loadResult = extensionManager->loadExtensionConfigs(extensionConfDir);
                            log_info("[STARTUP] Extension config loading result: %s", loadResult ? "SUCCESS" : "FAILED");
                            
                            // Refresh the extensions list
                            allExtensions = extensionManager->getAllExtensions();
                            log_info("[STARTUP] Found %zu extensions after loading configs", allExtensions.size());
                        } else {
                            log_info("[STARTUP] Extensions already loaded (%zu found), ensuring they are started", allExtensions.size());
                        }
                        
                        // Start all extensions using ExtensionManager directly
                        for (const auto& extInfo : allExtensions) {
                            log_info("[STARTUP] Starting extension: %s", extInfo.name.c_str());
                            bool started = extensionManager->startExtension(extInfo.name);
                            if (!started) {
                                log_warning("[STARTUP] Failed to start extension %s", extInfo.name.c_str());
                            } else {
                                log_info("[STARTUP] Successfully started extension: %s", extInfo.name.c_str());
                            }
                        }
                        
                        log_info("[STARTUP] Startup sequence completed - mainloop and extensions running");
                        return "Mainloop and extensions started successfully";
                    } else {
                        log_warning("[STARTUP] Extension manager not available, only mainloop started");
                        return "Mainloop started (no extensions)";
                    }
                } else {
                    log_error("[STARTUP] Mainloop failed to enter event loop within 5 seconds - extensions not started");
                    return "Mainloop started but failed to enter event loop";
                }
                
            } else {
                log_error("[STARTUP] Failed to start mainloop: %s", startResult.message.c_str());
                return "";
            }
        } else {
            log_error("[STARTUP] Operations not available for mainloop startup");
            return "";
        }
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
        log_debug("[RPC_CONTROLLER] Message payload: %s", payload.c_str());
        
        // Handle heartbeat messages from ur-mavdiscovery (startup mechanism trigger)
        if (topic == "clients/ur-mavdiscovery/heartbeat") {
            // Route to DeviceDiscoveryCronJob
            if (discoveryCronJob_) {
                discoveryCronJob_->handleHeartbeatMessage(topic, payload);
            } else {
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
            
        } else if (method == "runtime-info") {
            // Get comprehensive runtime information including thread types and status
            nlohmann::json runtimeInfo = getRuntimeInfo();
            response["result"] = runtimeInfo;
            
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
            
            // Create device info structure and update configuration
            MavlinkShared::DeviceInfo deviceInfo;
            deviceInfo.devicePath = devicePath;
            deviceInfo.state = MavlinkShared::DeviceState::VERIFIED;
            deviceInfo.baudrate = baudrate;
            deviceInfo.sysid = params.value("systemId", 1);
            deviceInfo.compid = params.value("componentId", 1);
            
            // Update router configuration with discovered device info (preserve existing baudrate)
            if (updateRouterConfigWithDevice(deviceInfo)) {
                if (isStartupTrigger) {
                    log_info("[STARTUP] Router configuration updated successfully");
                } else {
                    log_info("[RPC] Router configuration updated successfully");
                }
                
                // CRITICAL FIX: Recreate UART endpoints in main router's mainloop
                // This ensures the flight_controller endpoint is recreated after device reconnect
                try {
                    log_info("[RPC] Recreating UART endpoints in main router after device reconnect");
                    
                    // Reload configuration from file to get updated UART settings
                    JsonConfig json_config;
                    std::string configFilePath = routerConfigPath_; // This is already the full path to the JSON file
                    int ret = json_config.parse(configFilePath);
                    if (ret >= 0) {
                        Configuration config;
                        ret = json_config.extract_configuration(config);
                        if (ret >= 0) {
                            log_info("[RPC] Configuration reloaded: %zu UART, %zu UDP, %zu TCP endpoints",
                                    config.uart_configs.size(), config.udp_configs.size(), config.tcp_configs.size());
                            
                            // Get the main router's mainloop instance and recreate endpoints
                            auto& mainloop = Mainloop::get_instance();
                            
                            // Check if TCP server needs to be recreated
                            if (config.tcp_port != 0) {
                                log_info("[RPC] Ensuring TCP server is available on port %lu", config.tcp_port);
                                // The add_endpoints() will recreate the TCP server if needed
                            }
                            
                            // Add the updated UART endpoints
                            if (mainloop.add_endpoints(config)) {
                                log_info("[RPC] Successfully recreated UART endpoints in main router");
                                if (!config.uart_configs.empty()) {
                                    for (const auto& uartConfig : config.uart_configs) {
                                        log_info("[RPC] UART endpoint recreated: %s on %s (baudrate: %d)",
                                               uartConfig.name.c_str(), uartConfig.device.c_str(), 
                                               uartConfig.baudrates.empty() ? 0 : uartConfig.baudrates[0]);
                                    }
                                }
                            } else {
                                log_error("[RPC] Failed to recreate UART endpoints in main router");
                            }
                        } else {
                            log_error("[RPC] Failed to extract configuration from JSON (error code: %d)", ret);
                        }
                    } else {
                        log_error("[RPC] Failed to parse JSON configuration file (error code: %d)", ret);
                    }
                } catch (const std::exception& e) {
                    log_error("[RPC] Exception while recreating UART endpoints: %s", e.what());
                }
                
            } else {
                if (isStartupTrigger) {
                    log_warning("[STARTUP] Failed to update router configuration, using existing config");
                } else {
                    log_warning("[RPC] Failed to update router configuration, using existing config");
                }
            }
            
            // Check if mainloop is already running
            if (isMainloopRunning()) {
                log_info("[STARTUP] Mainloop already running, loading and starting extensions");
                
                // Load and start extension configurations
                auto* extensionManager = operations_->getExtensionManager();
                if (extensionManager) {
                    auto allExtensions = extensionManager->getAllExtensions();
                    
                    // Only load configs if no extensions are loaded yet
                    if (allExtensions.empty()) {
                        log_info("[STARTUP] Loading extension configurations...");
                        std::string extensionConfDir = "config";
                        bool loadResult = extensionManager->loadExtensionConfigs(extensionConfDir);
                        log_info("[STARTUP] Extension config loading result: %s", loadResult ? "SUCCESS" : "FAILED");
                        
                        // Refresh the extensions list
                        allExtensions = extensionManager->getAllExtensions();
                        log_info("[STARTUP] Found %zu extensions after loading configs", allExtensions.size());
                    } else {
                        log_info("[STARTUP] Extensions already loaded (%zu found), ensuring they are started", allExtensions.size());
                    }
                    
                    // Start all extensions using ExtensionManager directly
                    for (const auto& extInfo : allExtensions) {
                        log_info("[STARTUP] Starting extension: %s", extInfo.name.c_str());
                        bool started = extensionManager->startExtension(extInfo.name);
                        if (!started) {
                            log_warning("[STARTUP] Failed to start extension %s", extInfo.name.c_str());
                        } else {
                            log_info("[STARTUP] Successfully started extension: %s", extInfo.name.c_str());
                        }
                    }
                    
                    response["result"] = {{"status", "success"}, {"message", "Device added: mainloop running, extensions started"}};
                } else {
                    response["result"] = {{"status", "partial"}, {"message", "Device added: mainloop running, no extensions available"}};
                }
                
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
                    // Wait for mainloop to enter event loop with enhanced logging
                    if (isStartupTrigger) {
                        log_info("[STARTUP] Waiting for mainloop to enter event loop before loading extensions...");
                    } else {
                        log_info("[RPC] Waiting for mainloop to enter event loop before loading extensions...");
                    }
                    
                    // Use the new event loop wait mechanism
                    bool mainloopReady = Mainloop::wait_for_event_loop(5000); // 5 second timeout
                    
                    if (mainloopReady) {
                        if (isStartupTrigger) {
                            log_info("[STARTUP] Mainloop is in event loop, loading and starting extensions");
                        } else {
                            log_info("[RPC] Mainloop is in event loop, loading and starting extensions");
                        }
                        
                        // Load and start extension configurations
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
                                bool loadResult = extensionManager->loadExtensionConfigs(extensionConfDir);
                                if (isStartupTrigger) {
                                    log_info("[STARTUP] Extension config loading result: %s", loadResult ? "SUCCESS" : "FAILED");
                                } else {
                                    log_info("[RPC] Extension config loading result: %s", loadResult ? "SUCCESS" : "FAILED");
                                }
                                
                                // Refresh the extensions list
                                allExtensions = extensionManager->getAllExtensions();
                            }
                            
                            if (isStartupTrigger) {
                                log_info("[STARTUP] Found %zu extensions after loading configs", allExtensions.size());
                            } else {
                                log_info("[RPC] Found %zu extensions after loading configs", allExtensions.size());
                            }
                            
                            // Start all extensions
                            for (const auto& extInfo : allExtensions) {
                                if (isStartupTrigger) {
                                    log_info("[STARTUP] Starting extension: %s", extInfo.name.c_str());
                                } else {
                                    log_info("[RPC] Starting extension: %s", extInfo.name.c_str());
                                }
                                bool success = extensionManager->startExtension(extInfo.name);
                                if (isStartupTrigger) {
                                    log_info("[STARTUP] Successfully started extension: %s", extInfo.name.c_str());
                                } else {
                                    log_info("[RPC] Successfully started extension: %s", extInfo.name.c_str());
                                }
                            }
                            
                            if (isStartupTrigger) {
                                log_info("[STARTUP] Startup sequence completed - mainloop and extensions running");
                            } else {
                                log_info("[RPC] Startup sequence completed - mainloop and extensions running");
                            }
                        }
                    } else {
                        // Mainloop didn't enter event loop within timeout
                        if (isStartupTrigger) {
                            log_error("[STARTUP] Mainloop failed to enter event loop within 5 seconds - extensions not started");
                        } else {
                            log_error("[RPC] Mainloop failed to enter event loop within 5 seconds - extensions not started");
                        }
                        response["error"] = {{"code", -32500}, {"message", "Mainloop failed to enter event loop within timeout"}};
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
            
            // First stop all extensions using ExtensionManager directly
            auto* extensionManager = operations_->getExtensionManager();
            if (extensionManager) {
                auto allExtensions = extensionManager->getAllExtensions();
                log_info("[RPC] Stopping %zu extensions...", allExtensions.size());
                
                for (const auto& extInfo : allExtensions) {
                    bool stopped = extensionManager->stopExtension(extInfo.name);
                    if (!stopped) {
                        log_warning("[RPC] Failed to stop extension %s", extInfo.name.c_str());
                    } else {
                        log_info("[RPC] Successfully stopped extension: %s", extInfo.name.c_str());
                    }
                }
            } else {
                log_warning("[RPC] No extension manager available");
            }
            
            // Then stop the mainloop thread FIRST to ensure event loop is not running
            auto mainloopResp = this->stopThread(ThreadTarget::MAINLOOP);
            log_info("[RPC] Mainloop stop result: %s", mainloopResp.message.c_str());
            
            // CRITICAL FIX: Clear ALL endpoints AFTER stopping mainloop to avoid race conditions
            // This ensures clean state without concurrent access issues
            try {
                log_info("[RPC] Force closing TCP server and clearing all endpoints for clean disconnect");
                auto& mainloop = Mainloop::get_instance();
                mainloop.force_close_tcp_server();
                log_info("[RPC] TCP server force closed successfully");
                
                log_info("[RPC] Clearing ALL endpoints from main router after mainloop stopped");
                mainloop.clear_endpoints();
                log_info("[RPC] All endpoints cleared successfully");
            } catch (const std::exception& e) {
                log_error("[RPC] Exception while cleaning up endpoints: %s", e.what());
            }
            
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
    extensionManager_ = extensionManager; // Store reference for runtime info
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

bool RpcController::isMainloopRunning() const
{
    if (!operations_) {
        return false;
    }
    
    // Check both thread state AND event loop readiness
    auto status = operations_->getThreadStatus("mainloop");
    bool threadRunning = status.status == OperationStatus::SUCCESS;
    bool inEventLoop = Mainloop::is_in_event_loop();
    
    // Enhanced detection: If event loop is ready, consider mainloop as running
    // even if thread registration is temporarily lost during device reconnect
    if (inEventLoop) {
        log_debug("RpcController::isMainloopRunning() - Event loop is ready, considering mainloop as running");
        return true;
    }
    
    bool isRunning = threadRunning && inEventLoop;
    log_debug("RpcController::isMainloopRunning() - Thread running: %s, Event loop: %s, Overall: %s",
             threadRunning ? "yes" : "no", inEventLoop ? "yes" : "no", isRunning ? "yes" : "no");
    
    return isRunning;
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
        
        // Check if mainloop is already running (prevents duplicate startup)
        if (isMainloopRunning()) {
            log_info("[STARTUP] Mainloop already running, marking as started and ensuring extensions");
            mainloopStarted_.store(true);
            
            // Ensure extensions are started using ExtensionManager directly
            if (operations_) {
                auto* extensionManager = operations_->getExtensionManager();
                if (extensionManager) {
                    auto allExtensions = extensionManager->getAllExtensions();
                    
                    // Only load configs if no extensions are loaded yet
                    if (allExtensions.empty()) {
                        log_info("[STARTUP] Loading extension configurations...");
                        std::string extensionConfDir = "config";
                        bool loadResult = extensionManager->loadExtensionConfigs(extensionConfDir);
                        log_info("[STARTUP] Extension config loading result: %s", loadResult ? "SUCCESS" : "FAILED");
                        
                        // Refresh the extensions list
                        allExtensions = extensionManager->getAllExtensions();
                        log_info("[STARTUP] Found %zu extensions after loading configs", allExtensions.size());
                    } else {
                        log_info("[STARTUP] Extensions already loaded (%zu found), ensuring they are started", allExtensions.size());
                    }
                    
                    // Start all extensions using ExtensionManager directly
                    for (const auto& extInfo : allExtensions) {
                        log_info("[STARTUP] Starting extension: %s", extInfo.name.c_str());
                        bool started = extensionManager->startExtension(extInfo.name);
                        if (!started) {
                            log_warning("[STARTUP] Failed to start extension %s", extInfo.name.c_str());
                        } else {
                            log_info("[STARTUP] Successfully started extension: %s", extInfo.name.c_str());
                        }
                    }
                } else {
                    log_warning("[STARTUP] Extension manager not available, only mainloop started");
                }
            }
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
                
                // Wait for mainloop to enter event loop before starting extensions
                log_info("[STARTUP] Waiting for mainloop to enter event loop before loading extensions...");
                bool mainloopReady = Mainloop::wait_for_event_loop(5000); // 5 second timeout
                
                if (mainloopReady) {
                    log_info("[STARTUP] Mainloop is in event loop, loading and starting extensions");
                    
                    // Load and start extension configurations
                    auto* extensionManager = operations_->getExtensionManager();
                    if (extensionManager) {
                        auto allExtensions = extensionManager->getAllExtensions();
                        
                        // Only load configs if no extensions are loaded yet
                        if (allExtensions.empty()) {
                            log_info("[STARTUP] Loading extension configurations...");
                            std::string extensionConfDir = "config";
                            bool loadResult = extensionManager->loadExtensionConfigs(extensionConfDir);
                            log_info("[STARTUP] Extension config loading result: %s", loadResult ? "SUCCESS" : "FAILED");
                            
                            // Refresh the extensions list
                            allExtensions = extensionManager->getAllExtensions();
                            log_info("[STARTUP] Found %zu extensions after loading configs", allExtensions.size());
                        } else {
                            log_info("[STARTUP] Extensions already loaded (%zu found), ensuring they are started", allExtensions.size());
                        }
                        
                        // Start all extensions using ExtensionManager directly
                        for (const auto& extInfo : allExtensions) {
                            log_info("[STARTUP] Starting extension: %s", extInfo.name.c_str());
                            bool started = extensionManager->startExtension(extInfo.name);
                            if (!started) {
                                log_warning("[STARTUP] Failed to start extension %s", extInfo.name.c_str());
                            } else {
                                log_info("[STARTUP] Successfully started extension: %s", extInfo.name.c_str());
                            }
                        }
                        
                        log_info("[STARTUP] Startup sequence completed - mainloop and extensions running");
                    } else {
                        log_warning("[STARTUP] Extension manager not available, only mainloop started");
                    }
                } else {
                    log_error("[STARTUP] Mainloop failed to enter event loop within 5 seconds - extensions not started");
                }
                
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

nlohmann::json RpcController::getRuntimeInfo() {
    nlohmann::json runtimeInfo;
    
    try {
        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        runtimeInfo["timestamp"] = std::ctime(&time_t);
        runtimeInfo["uptime_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(
            now - startupTime_).count();
        
        // Get all thread status
        RpcResponse allThreadsResponse = operations_->getAllThreadStatus();
        runtimeInfo["total_threads"] = 0;
        runtimeInfo["running_threads"] = 0;
        runtimeInfo["threads"] = nlohmann::json::array();
        
        if (allThreadsResponse.status == OperationStatus::SUCCESS && 
            allThreadsResponse.threadStates.size() > 0) {
            
            for (const auto& pair : allThreadsResponse.threadStates) {
                const std::string& threadName = pair.first;
                const ThreadStateInfo& threadInfo = pair.second;
                
                nlohmann::json threadJson;
                threadJson["name"] = threadName;
                threadJson["thread_id"] = threadInfo.threadId;
                threadJson["attachment_id"] = threadInfo.attachmentId;
                threadJson["state"] = threadInfo.state;
                threadJson["state_name"] = threadStateToString(static_cast<int>(threadInfo.state));
                threadJson["is_alive"] = threadInfo.isAlive;
                
                // Determine thread nature/type
                threadJson["nature"] = determineThreadNature(threadName);
                threadJson["type"] = determineThreadType(threadName);
                
                // Add additional metadata based on thread type
                if (threadName == "mainloop") {
                    threadJson["description"] = "Main event loop for MAVLink message processing";
                    threadJson["critical"] = true;
                } else if (threadName.find("extension") != std::string::npos || 
                          threadName.find("udp") != std::string::npos || 
                          threadName.find("tcp") != std::string::npos) {
                    threadJson["description"] = "Extension thread for protocol handling";
                    threadJson["critical"] = false;
                    
                    // Try to get extension-specific info if available
                    if (extensionManager_) {
                        auto extInfo = extensionManager_->getExtensionInfo(threadName);
                        if (!extInfo.name.empty()) {
                            threadJson["extension_info"] = {
                                {"config_file", extInfo.config.name},
                                {"type", static_cast<int>(extInfo.config.type)},
                                {"address", extInfo.config.address},
                                {"port", extInfo.config.port},
                                {"extension_point", extInfo.config.assigned_extension_point}
                            };
                        }
                    }
                } else if (threadName.find("http") != std::string::npos) {
                    threadJson["description"] = "HTTP server thread for REST API";
                    threadJson["critical"] = false;
                } else if (threadName.find("stats") != std::string::npos) {
                    threadJson["description"] = "Statistics collection thread";
                    threadJson["critical"] = false;
                } else {
                    threadJson["description"] = "System thread";
                    threadJson["critical"] = false;
                }
                
                runtimeInfo["threads"].push_back(threadJson);
                
                // Update counters
                runtimeInfo["total_threads"] = runtimeInfo["total_threads"].get<int>() + 1;
                if (threadInfo.isAlive) {
                    runtimeInfo["running_threads"] = runtimeInfo["running_threads"].get<int>() + 1;
                }
            }
        }
        
        // Add system information
        runtimeInfo["system"] = {
            {"rpc_initialized", rpcInitialized_.load()},
            {"discovery_triggered", discoveryTriggered_.load()},
            {"mainloop_started", mainloopStarted_.load()},
            {"client_id", rpcClientId_},
            {"config_path", rpcConfigPath_}
        };
        
        // Add extension information if available
        if (extensionManager_) {
            auto allExtensions = extensionManager_->getAllExtensions();
            runtimeInfo["extensions"] = nlohmann::json::array();
            
            for (const auto& extInfo : allExtensions) {
                nlohmann::json extJson;
                extJson["name"] = extInfo.name;
                extJson["type"] = static_cast<int>(extInfo.config.type);
                extJson["config_file"] = extInfo.config.name;
                extJson["address"] = extInfo.config.address;
                extJson["port"] = extInfo.config.port;
                extJson["extension_point"] = extInfo.config.assigned_extension_point;
                extJson["loaded"] = true; // If it's in the list, it's loaded
                extJson["thread_id"] = extInfo.threadId;
                extJson["is_running"] = extInfo.isRunning;
                
                // Check if corresponding thread is running
                bool threadRunning = false;
                for (const auto& threadJson : runtimeInfo["threads"]) {
                    if (threadJson["name"] == extInfo.name && threadJson["is_alive"]) {
                        threadRunning = true;
                        break;
                    }
                }
                extJson["thread_running"] = threadRunning;
                
                runtimeInfo["extensions"].push_back(extJson);
            }
            
            runtimeInfo["total_extensions"] = allExtensions.size();
        } else {
            runtimeInfo["extensions"] = nlohmann::json::array();
            runtimeInfo["total_extensions"] = 0;
        }
        
        // Add device discovery status
        runtimeInfo["device_discovery"] = {
            {"last_heartbeat", lastHeartbeatTime_.time_since_epoch().count()},
            {"heartbeat_timeout_seconds", HEARTBEAT_TIMEOUT.count()},
            {"heartbeat_active", 
             (std::chrono::steady_clock::now() - lastHeartbeatTime_) < HEARTBEAT_TIMEOUT}
        };
        
        runtimeInfo["status"] = "success";
        runtimeInfo["message"] = "Runtime information retrieved successfully";
        
    } catch (const std::exception& e) {
        runtimeInfo["status"] = "error";
        runtimeInfo["message"] = std::string("Failed to get runtime info: ") + e.what();
        log_error("Failed to get runtime info: %s", e.what());
    }
    
    return runtimeInfo;
}

std::string RpcController::determineThreadNature(const std::string& threadName) {
    if (threadName == "mainloop") {
        return "core";
    } else if (threadName.find("extension") != std::string::npos || 
               threadName.find("udp") != std::string::npos || 
               threadName.find("tcp") != std::string::npos ||
               threadName.find("_ext_") != std::string::npos) {  // Extension naming pattern
        return "extension";
    } else if (threadName.find("http") != std::string::npos) {
        return "service";
    } else if (threadName.find("stats") != std::string::npos) {
        return "monitoring";
    } else {
        return "system";
    }
}

std::string RpcController::determineThreadType(const std::string& threadName) {
    if (threadName == "mainloop") {
        return "mainloop";
    } else if (threadName.find("udp") != std::string::npos) {
        return "udp_extension";
    } else if (threadName.find("tcp") != std::string::npos) {
        return "tcp_extension";
    } else if (threadName.find("internal") != std::string::npos) {
        return "internal_extension";
    } else if (threadName.find("_ext_") != std::string::npos) {
        return "extension";
    } else if (threadName.find("http") != std::string::npos) {
        return "http_server";
    } else if (threadName.find("stats") != std::string::npos) {
        return "statistics";
    } else {
        return "unknown";
    }
}

std::string RpcController::threadStateToString(int state) {
    switch (state) {
        case 0: return "Stopped";
        case 1: return "Running";
        case 2: return "Paused";
        case 3: return "Error";
        case 4: return "Starting";
        default: return "Unknown";
    }
}

bool RpcController::updateRouterConfigWithDevice(const MavlinkShared::DeviceInfo& deviceInfo) {
    try {
        if (routerConfigPath_.empty()) {
            log_error("[CONFIG] No router configuration path available");
            return false;
        }
        
        log_info("[CONFIG] Updating router configuration with device: %s", deviceInfo.devicePath.c_str());
        log_info("[CONFIG] Using router configuration file: %s", routerConfigPath_.c_str());
        
        std::ifstream configFile(routerConfigPath_);
        if (!configFile.is_open()) {
            log_error("[CONFIG] Failed to open router configuration file: %s", routerConfigPath_.c_str());
            return false;
        }
        
        nlohmann::json configJson;
        configFile >> configJson;
        configFile.close();
        
        // Update or add the UART endpoint for the discovered device
        bool deviceFound = false;
        bool deviceUpdated = false;
        
        if (configJson.contains("uart_endpoints") && configJson["uart_endpoints"].is_array()) {
            log_info("[CONFIG] Checking existing UART endpoints...");
            
            for (auto& endpoint : configJson["uart_endpoints"]) {
                if (endpoint.contains("name") && endpoint["name"] == "flight_controller") {
                    log_info("[CONFIG] Found flight_controller endpoint, updating device path from %s to %s", 
                            endpoint.value("device", "unknown").c_str(), deviceInfo.devicePath.c_str());
                    
                    // CRITICAL: Preserve existing baudrate configuration, only update device path
                    endpoint["device"] = deviceInfo.devicePath;
                    deviceFound = true;
                    deviceUpdated = true;
                    break;
                }
            }
            
            if (!deviceFound) {
                log_warning("[CONFIG] flight_controller endpoint not found, adding new endpoint");
                // Add new UART endpoint with discovered device path and default baudrate
                nlohmann::json newEndpoint;
                newEndpoint["name"] = "flight_controller";
                newEndpoint["device"] = deviceInfo.devicePath;
                newEndpoint["baud"] = nlohmann::json::array({57600}); // Default baudrate
                newEndpoint["flow_control"] = false;
                
                configJson["uart_endpoints"].push_back(newEndpoint);
                deviceUpdated = true;
            }
        } else {
            log_info("[CONFIG] No uart_endpoints array found, creating new one");
            // Create uart_endpoints array with the discovered device
            nlohmann::json newEndpoint;
            newEndpoint["name"] = "flight_controller";
            newEndpoint["device"] = deviceInfo.devicePath;
            newEndpoint["baud"] = nlohmann::json::array({57600}); // Default baudrate
            newEndpoint["flow_control"] = false;
            
            configJson["uart_endpoints"] = nlohmann::json::array({newEndpoint});
            deviceUpdated = true;
        }
        
        if (deviceUpdated) {
            // Write updated config back to file
            std::ofstream outFile(routerConfigPath_);
            if (outFile.is_open()) {
                outFile << configJson.dump(2);
                outFile.close();
                log_info("[CONFIG] Successfully updated router configuration file");
                
                // Log the updated configuration for verification
                log_info("[CONFIG] Updated UART endpoints:");
                if (configJson.contains("uart_endpoints")) {
                    for (const auto& endpoint : configJson["uart_endpoints"]) {
                        std::string device = endpoint.value("device", "unknown");
                        std::string name = endpoint.value("name", "unnamed");
                        auto baudArray = endpoint.value("baud", nlohmann::json::array());
                        std::string baudStr = baudArray.empty() ? "unknown" : std::to_string(baudArray[0].get<int>());
                        log_info("[CONFIG]   - %s: %s (baudrate: %s)", name.c_str(), device.c_str(), baudStr.c_str());
                    }
                }
                
                return true;
            } else {
                log_error("[CONFIG] Failed to write updated configuration to file: %s", routerConfigPath_.c_str());
                return false;
            }
        } else {
            log_info("[CONFIG] No configuration updates needed");
            return true;
        }
        
    } catch (const std::exception& e) {
        log_error("[CONFIG] Error updating router configuration: %s", e.what());
        return false;
    }
}

} // namespace RpcMechanisms
