#include "RpcControllerNew.hpp"
#include "RpcClientInterfaceNew.hpp"
#include "../common/log.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include "../mavlink-extensions/ExtensionManager.h"
#include "../../ur-mavdiscovery-shared/include/MavlinkDeviceStructs.hpp"
#include "../../ur-mavdiscovery-shared/include/MavlinkEventSerializer.hpp"

namespace RpcMechanisms {

RpcController::RpcController(ThreadMgr::ThreadManager& threadManager) 
    : operations_(std::make_unique<RpcOperations>(threadManager)) {
    log_info("RpcController initialized with separated RPC client and operations");
}

RpcController::~RpcController() {
    stopRpcClient();
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
        log_error("Cannot setup RPC message handlers - RPC client not available");
        return;
    }
    
    // Set message handler for RPC client
    rpcClient_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
        this->handleRpcMessage(topic, payload);
    });
    
    log_info("RPC message handlers configured successfully");
}

void RpcController::handleRpcMessage(const std::string& topic, const std::string& payload) {
    try {
        log_debug("Handling RPC message from topic: %s", topic.c_str());
        
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
            log_info("\n[RPC] ========================================");
            log_info("[RPC] Received mavlink_device_added request");
            log_info("[RPC] Action: START mainloop thread AND load/start all extensions");
            log_info("[RPC] ========================================");
            
            // Extract device info from params
            std::string devicePath = params.value("devicePath", "unknown");
            std::string deviceState = params.value("state", "UNKNOWN");
            int baudrate = params.value("baudrate", 0);
            log_info("[RPC] Device added: %s (State: %s, Baudrate: %d)", devicePath.c_str(), deviceState.c_str(), baudrate);
            
            // First, start the mainloop thread (this initializes the global config)
            auto mainloopResp = this->startThread(ThreadTarget::MAINLOOP);
            log_info("[RPC] Mainloop start result: %s", mainloopResp.message.c_str());
            
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
                        log_info("[RPC] Loading extension configurations...");
                        
                        std::string extensionConfDir = "config";
                        extensionManager->loadExtensionConfigs(extensionConfDir);
                        
                        // Refresh the extensions list
                        allExtensions = extensionManager->getAllExtensions();
                        log_info("[RPC] Loaded and started %zu extensions from config", allExtensions.size());
                    } else {
                        // Extensions already exist, ensure they're started
                        log_info("[RPC] Extensions already loaded (%zu found)", allExtensions.size());
                        
                        // Start all existing extensions
                        for (const auto& extInfo : allExtensions) {
                            auto extResp = operations_->executeOperationOnThread(extInfo.name, ThreadOperation::START);
                            if (extResp.status != OperationStatus::SUCCESS) {
                                log_warning("[RPC] Failed to start extension %s: %s", extInfo.name.c_str(), extResp.message.c_str());
                            }
                        }
                    }
                    response["result"] = {{"status", "success"}, {"message", "Device added: mainloop and extensions started"}};
                } else {
                    log_warning("[RPC] No extension manager available, only mainloop started");
                    response["result"] = {{"status", "partial"}, {"message", "Device added: mainloop started (no extensions)"}};
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

} // namespace RpcMechanisms
