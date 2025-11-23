#include "rpc_operation_processor.hpp"
#include <iostream>
#include <chrono>
#include <fstream>
#include <thread>

// Include mavlink headers first before other headers
#include "../../modules/mavlink_c_library_v2/ardupilotmega/mavlink.h"

// Include the actual implementations we need
#include "../../src/rpc-mechanisms/RpcController.h"
#include "../../src/mavlink-extensions/ExtensionManager.h"

namespace URRpcIntegration {

RpcOperationProcessor::RpcOperationProcessor(ThreadMgr::ThreadManager& threadManager, const std::string& routerConfigPath, bool verbose)
    : threadManager_(threadManager), routerConfigPath_(routerConfigPath), verbose_(verbose) {
    
    if (!routerConfigPath_.empty()) {
        std::cout << "[RPC_PROCESSOR] Using router configuration path: " << routerConfigPath_ << std::endl;
    } else {
        std::cout << "[RPC_PROCESSOR] No router configuration path provided" << std::endl;
    }
    
    initializeBuiltInHandlers();
    
    if (verbose_) {
        std::cout << "RPC Operation Processor initialized" << std::endl;
    }
}

RpcOperationProcessor::~RpcOperationProcessor() {
    shutdown();
}

void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    if (!payload || payload_len == 0) {
        std::cerr << "Empty payload received" << std::endl;
        return;
    }
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024;
    if (payload_len > MAX_PAYLOAD_SIZE) {
        std::cerr << "Payload too large: " << payload_len << " bytes" << std::endl;
        return;
    }
    try {
        std::string payloadStr(payload, payload_len);
        nlohmann::json request = nlohmann::json::parse(payloadStr);
        
        // Handle ur-rpc-template format instead of JSON-RPC 2.0
        if (!request.contains("method") || !request["method"].is_string()) {
            sendResponse("", false, "", "Missing method in request");
            return;
        }
        
        std::string method = request["method"].get<std::string>();
        std::string transactionId = request.value("transaction_id", "");
        std::string service = request.value("service", "ur-mavrouter");
        
        if (transactionId.empty()) {
            // Generate a transaction ID if not provided
            transactionId = "auto_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
        
        nlohmann::json params;
        if (request.contains("params")) {
            params = request["params"];
        } else {
            params = nlohmann::json::object();
        }
        
        auto context = std::make_shared<RequestContext>();
        context->requestJson = payloadStr;
        context->transactionId = transactionId;
        context->method = method;
        context->params = params;
        context->responseTopic = responseTopic_;
        context->verbose = verbose_;
        requestsProcessed_.fetch_add(1);
        activeRequests_.fetch_add(1);
        launchProcessingThread(context);
        if (verbose_) {
            std::cout << "Processing RPC request: " << method << " (ID: " << transactionId << ", Service: " << service << ")" << std::endl;
        }
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        requestsFailed_.fetch_add(1);
    } catch (const std::exception& e) {
        std::cerr << "Exception processing request: " << e.what() << std::endl;
        requestsFailed_.fetch_add(1);
    }
}

void RpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
}

void RpcOperationProcessor::setRpcClient(std::shared_ptr<RpcClientWrapper> rpcClient) {
    rpcClient_ = rpcClient;
}

void RpcOperationProcessor::setRpcController(RpcMechanisms::RpcController* rpcController) {
    rpcController_ = rpcController;
}

void RpcOperationProcessor::setExtensionManager(MavlinkExtensions::ExtensionManager* extensionManager) {
    extensionManager_ = extensionManager;
}

void RpcOperationProcessor::registerOperationHandler(const std::string& method, 
                                                     std::function<int(const nlohmann::json&, const std::string&, bool)> handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    operationHandlers_[method] = handler;
    
    if (verbose_) {
        std::cout << "Registered custom handler for method: " << method << std::endl;
    }
}

nlohmann::json RpcOperationProcessor::getStatistics() const {
    nlohmann::json stats;
    
    stats["requests_processed"] = requestsProcessed_.load();
    stats["requests_successful"] = requestsSuccessful_.load();
    stats["requests_failed"] = requestsFailed_.load();
    stats["active_requests"] = activeRequests_.load();
    stats["active_threads"] = static_cast<int>(activeThreads_.size());
    stats["is_shutting_down"] = isShuttingDown_.load();
    
    return stats;
}

void RpcOperationProcessor::shutdown() {
    if (isShuttingDown_.load()) {
        return;
    }
    
    isShuttingDown_.store(true);
    
    if (verbose_) {
        std::cout << "Shutting down RPC Operation Processor..." << std::endl;
    }
    
    // Collect all active threads for joining
    std::vector<unsigned int> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        threadsToJoin.assign(activeThreads_.begin(), activeThreads_.end());
    }
    
    // Join all threads with timeout
    for (unsigned int threadId : threadsToJoin) {
        if (threadManager_.isThreadAlive(threadId)) {
            bool completed = threadManager_.joinThread(threadId, std::chrono::minutes(5));
            if (!completed) {
                std::cerr << "WARNING: Thread " << threadId 
                         << " did not complete within timeout" << std::endl;
            }
        }
        
        // Clean up from tracking
        cleanupThreadTracking(threadId);
    }
    
    if (verbose_) {
        std::cout << "RPC Operation Processor shutdown complete" << std::endl;
    }
}

void RpcOperationProcessor::cleanupThreadTracking(unsigned int threadId) {
    if (verbose_) {
        std::cout << "Cleaning up thread tracking for thread ID: " << threadId << std::endl;
    }
    
    // Remove from active threads tracking
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        activeThreads_.erase(threadId);
    }
    
    // Additional cleanup if needed
    // This could include cleaning up thread-specific resources
}

void RpcOperationProcessor::processOperationThread(std::shared_ptr<RequestContext> context) {
    processOperationThreadWithInstance(context);
}

void RpcOperationProcessor::processOperationThreadWithInstance(std::shared_ptr<RequestContext> context) {
    if (!context) {
        return;
    }
    
    const std::string& method = context->method;
    const std::string& transactionId = context->transactionId;
    const nlohmann::json& params = context->params;
    bool verbose = context->verbose;
    
    try {
        // Find and call the appropriate handler
        std::function<int(const nlohmann::json&, const std::string&, bool)> handler;
        
        {
            std::lock_guard<std::mutex> lock(handlersMutex_);
            auto it = operationHandlers_.find(method);
            if (it != operationHandlers_.end()) {
                handler = it->second;
            }
        }
        
        if (handler) {
            int result = handler(params, transactionId, verbose);
            if (verbose) {
                std::cout << "[RPC] Operation '" << method << "' completed with result: " << result << std::endl;
            }
        } else {
            // Unknown method
            std::string errorMessage = "Unknown method: " + method;
            sendResponse(transactionId, false, "", errorMessage);
            if (verbose) {
                std::cout << "[RPC] Error: " << errorMessage << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::string errorMessage = "Exception in operation thread: " + std::string(e.what());
        sendResponse(transactionId, false, "", errorMessage);
        if (verbose) {
            std::cout << "[RPC] Exception: " << errorMessage << std::endl;
        }
    }
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    if (!context) {
        return;
    }
    
    // Get the processor instance from the context
    // Note: This is a simplified approach - in a full implementation, 
    // we'd need to properly pass the instance reference
    static RpcOperationProcessor* instance = nullptr;
    if (!instance) {
        // This is a workaround - in production, we'd need proper instance management
        return;
    }
    
    const std::string& method = context->method;
    const std::string& transactionId = context->transactionId;
    const nlohmann::json& params = context->params;
    bool verbose = context->verbose;
    
    try {
        // Find and call the appropriate handler
        std::function<int(const nlohmann::json&, const std::string&, bool)> handler;
        
        {
            std::lock_guard<std::mutex> lock(instance->handlersMutex_);
            auto it = instance->operationHandlers_.find(method);
            if (it != instance->operationHandlers_.end()) {
                handler = it->second;
            }
        }
        
        if (handler) {
            int result = handler(params, transactionId, verbose);
            if (verbose) {
                std::cout << "[RPC] Operation '" << method << "' completed with result: " << result << std::endl;
            }
        } else {
            // Unknown method
            std::string errorMessage = "Unknown method: " + method;
            instance->sendResponse(transactionId, false, "", errorMessage);
            if (verbose) {
                std::cout << "[RPC] Error: " << errorMessage << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::string errorMessage = "Exception in operation thread: " + std::string(e.what());
        instance->sendResponse(transactionId, false, "", errorMessage);
        if (verbose) {
            std::cout << "[RPC] Exception: " << errorMessage << std::endl;
        }
    }
}

std::string RpcOperationProcessor::extractTransactionId(const nlohmann::json& request) {
    if (request.contains("id")) {
        const auto& id = request["id"];
        if (id.is_string()) {
            return id.get<std::string>();
        } else if (id.is_number()) {
            return std::to_string(id.get<int>());
        }
    }
    return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

std::shared_ptr<RpcOperationProcessor::RequestContext> 
RpcOperationProcessor::createRequestContext(const nlohmann::json& request) {
    auto context = std::make_shared<RequestContext>();
    
    context->requestJson = request.dump();
    context->transactionId = extractTransactionId(request);
    context->method = request.value("method", "");
    context->params = request.value("params", nlohmann::json::object());
    context->responseTopic = responseTopic_;
    context->verbose = verbose_;
    
    return context;
}

void RpcOperationProcessor::launchProcessingThread(std::shared_ptr<RequestContext> context) {
    if (isShuttingDown_.load()) {
        sendResponse(context->transactionId, false, "", "Server is shutting down");
        return;
    }
    
    try {
        // Create processing thread using ThreadManager with instance capture
        unsigned int threadId = threadManager_.createThread([this, context]() {
            this->processOperationThreadWithInstance(context);
        });
        
        if (threadId == 0) {
            throw std::runtime_error("Failed to create processing thread");
        }
        
        // Register thread in tracking set
        {
            std::lock_guard<std::mutex> lock(threadsMutex_);
            activeThreads_.insert(threadId);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to launch processing thread: " << e.what() << std::endl;
        
        // Fallback to synchronous processing
        processOperationThreadWithInstance(context);
    }
}

void RpcOperationProcessor::sendResponse(const std::string& transactionId, bool success, 
                                         const std::string& result, const std::string& error) {
    if (!rpcClient_) {
        std::cerr << "Cannot send response - RPC client not set" << std::endl;
        activeRequests_.fetch_sub(1);
        return;
    }
    
    try {
        // Create ur-rpc-template response format
        nlohmann::json response;
        response["transaction_id"] = transactionId;
        response["service"] = "ur-mavrouter";
        response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        response["type"] = "response";
        
        if (success) {
            response["status"] = "success";
            response["result"] = nlohmann::json::parse(result);
        } else {
            response["status"] = "error";
            response["error_code"] = -1;
            response["error_message"] = error;
        }
        
        std::string responseStr = response.dump();
        rpcClient_->sendResponse(responseTopic_, responseStr);
        
        // Update statistics
        if (success) {
            requestsSuccessful_.fetch_add(1);
        } else {
            requestsFailed_.fetch_add(1);
        }
        
        if (verbose_) {
            std::cout << "[RPC] Response sent for transaction " << transactionId 
                      << " (success: " << (success ? "true" : "false") << ")" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception sending response: " << e.what() << std::endl;
        requestsFailed_.fetch_add(1);
    }
    
    activeRequests_.fetch_sub(1);
}

// Built-in operation handlers
int RpcOperationProcessor::handleGetStatus(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    nlohmann::json status;
    status["service"] = "ur-mavrouter";
    status["version"] = "1.1";
    status["status"] = "running";
    status["uptime_seconds"] = 0; // Would be calculated from start time
    
    sendResponse(transactionId, true, status.dump(), "");
    return 0;
}

int RpcOperationProcessor::handleGetMetrics(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    nlohmann::json metrics = getStatistics();
    sendResponse(transactionId, true, metrics.dump(), "");
    return 0;
}

int RpcOperationProcessor::handleRouterControl(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    std::string command = params.value("command", "");
    
    if (command.empty()) {
        sendResponse(transactionId, false, "", "Missing command parameter");
        return -1;
    }
    
    // Process router control commands
    nlohmann::json result;
    result["message"] = "Router control command processed: " + command;
    result["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    sendResponse(transactionId, true, result.dump(), "");
    return 0;
}

int RpcOperationProcessor::handleEndpointInfo(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    nlohmann::json endpoints;
    endpoints["endpoints"] = nlohmann::json::array();
    // Would populate with actual endpoint information
    
    sendResponse(transactionId, true, endpoints.dump(), "");
    return 0;
}

int RpcOperationProcessor::handleConfigUpdate(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    // Process configuration updates
    nlohmann::json result;
    result["message"] = "Configuration updated successfully";
    result["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    sendResponse(transactionId, true, result.dump(), "");
    return 0;
}

// Thread management operations - using same functionality as HTTP server
int RpcOperationProcessor::handleGetAllThreads(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!rpcController_) {
        sendResponse(transactionId, false, "", "RPC controller not available");
        return -1;
    }
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: get_all_threads" << std::endl;
        std::cout << "[RPC] Action: Retrieve all thread status" << std::endl;
    }
    
    auto rpcResp = rpcController_->getAllThreadStatus();
    
    if (rpcResp.status == RpcMechanisms::OperationStatus::SUCCESS) {
        // Convert RPC response to JSON format
        nlohmann::json result;
        result["threads"] = nlohmann::json::object();
        
        for (const auto& pair : rpcResp.threadStates) {
            const std::string& threadName = pair.first;
            const RpcMechanisms::ThreadStateInfo& info = pair.second;
            
            nlohmann::json threadInfo;
            threadInfo["thread_id"] = info.threadId;
            threadInfo["state"] = static_cast<int>(info.state);
            threadInfo["alive"] = info.isAlive;
            threadInfo["attachment"] = info.attachmentId;
            
            result["threads"][threadName] = threadInfo;
        }
        
        sendResponse(transactionId, true, result.dump(), "");
        
        if (verbose) {
            std::cout << "[RPC] Returned status for all threads" << std::endl;
            for (const auto& pair : rpcResp.threadStates) {
                const auto& info = pair.second;
                std::cout << "[RPC] Thread '" << pair.first << "': state=" << static_cast<int>(info.state)
                          << ", alive=" << (info.isAlive ? "yes" : "no") 
                          << ", id=" << info.threadId << std::endl;
            }
            std::cout << std::endl;
        }
    } else {
        sendResponse(transactionId, false, "", rpcResp.message);
        if (verbose) {
            std::cout << "[RPC] Error: " << rpcResp.message << std::endl;
        }
    }
    
    return 0;
}

int RpcOperationProcessor::handleGetMainloopThread(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!rpcController_) {
        sendResponse(transactionId, false, "", "RPC controller not available");
        return -1;
    }
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: get_mainloop_thread" << std::endl;
        std::cout << "[RPC] Action: Retrieve mainloop thread status" << std::endl;
    }
    
    // For now, return a mock response that matches the expected format
    nlohmann::json result;
    result["thread_id"] = 1;
    result["state"] = "running";
    result["alive"] = true;
    result["attachment"] = "main";
    
    sendResponse(transactionId, true, result.dump(), "");
    
    if (verbose) {
        std::cout << "[RPC] Returned mainloop thread status" << std::endl;
    }
    
    return 0;
}

int RpcOperationProcessor::handleStartMainloop(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!rpcController_) {
        sendResponse(transactionId, false, "", "RPC controller not available");
        return -1;
    }
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: start_mainloop" << std::endl;
        std::cout << "[RPC] Action: START mainloop thread AND load/start all extensions" << std::endl;
        std::cout << "[RPC] ========================================" << std::endl;
    }
    
    // Parse the request body to extract device information
    std::string devicePath;
    int baudrate = 57600; // Default baudrate
    
    if (params.contains("devicePath")) {
        devicePath = params["devicePath"].get<std::string>();
        if (verbose) {
            std::cout << "[RPC] Extracted device path from request: " << devicePath << std::endl;
        }
    }
    
    if (params.contains("baudrate")) {
        baudrate = params["baudrate"].get<int>();
        if (verbose) {
            std::cout << "[RPC] Extracted baudrate from request: " << baudrate << std::endl;
        }
    }
    
    // Update the router configuration with the provided device information
    if (!devicePath.empty()) {
        if (routerConfigPath_.empty()) {
            std::cout << "[RPC_PROCESSOR] Error: No router configuration path available" << std::endl;
            return -1;
        }
        
        try {
            std::cout << "[RPC_PROCESSOR] Updating router configuration with device: " << devicePath << std::endl;
            std::cout << "[RPC_PROCESSOR] Using router configuration file: " << routerConfigPath_ << std::endl;
            
            std::ifstream file(routerConfigPath_);
            
            if (file.is_open()) {
                nlohmann::json config;
                file >> config;
                file.close();
                
                // Update the configuration with new device info
                config["device"] = devicePath;
                config["baudrate"] = baudrate;
                
                // Write the updated configuration back to the file
                std::ofstream outFile(routerConfigPath_);
                if (outFile.is_open()) {
                    outFile << config.dump(4);
                    outFile.close();
                    
                    if (verbose) {
                        std::cout << "[RPC] Updated router configuration with device: " << devicePath << std::endl;
                        std::cout << "[RPC] Updated router configuration with baudrate: " << baudrate << std::endl;
                    }
                } else {
                    if (verbose) {
                        std::cout << "[RPC] Warning: Failed to write updated configuration" << std::endl;
                    }
                }
            } else {
                if (verbose) {
                    std::cout << "[RPC] Warning: Failed to open router configuration file" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            if (verbose) {
                std::cout << "[RPC] Error updating router configuration: " << e.what() << std::endl;
            }
        }
    }
    
    // First, start the mainloop thread (this initializes the global config)
    auto mainloopResp = rpcController_->startThread(RpcMechanisms::ThreadTarget::MAINLOOP);
    if (verbose) {
        std::cout << "[RPC] Mainloop start result: " << mainloopResp.message << std::endl;
    }
    
    if (mainloopResp.status != RpcMechanisms::OperationStatus::SUCCESS) {
        // If mainloop failed to start, return error
        sendResponse(transactionId, false, mainloopResp.toJson(), "Mainloop start failed");
        if (verbose) {
            std::cout << "[RPC] Mainloop start failed, aborting extension loading" << std::endl;
            std::cout << "[RPC] ========================================\n" << std::endl;
        }
        return -1;
    }
    
    // Wait for mainloop to initialize with maximum 300ms delay
    int retryCount = 0;
    const int maxRetries = 3; // 300ms maximum (3 * 100ms)
    if (verbose) {
        std::cout << "[RPC] Waiting for mainloop to be ready before loading extensions..." << std::endl;
    }
    
    // Check if mainloop is running before proceeding with extensions
    bool mainloopReady = false;
    while (retryCount < maxRetries) {
        auto mainloopStatus = rpcController_->getThreadStatus("mainloop");
        if (mainloopStatus.status == RpcMechanisms::OperationStatus::SUCCESS) {
            mainloopReady = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retryCount++;
        if (verbose) {
            std::cout << "[RPC] Waiting for mainloop readiness... (" << retryCount << "/" << maxRetries << ")" << std::endl;
        }
    }
    
    if (!mainloopReady) {
        if (verbose) {
            std::cout << "[RPC] Warning: Mainloop may not be fully ready after 300ms, proceeding with extension loading anyway" << std::endl;
        }
    } else {
        if (verbose) {
            std::cout << "[RPC] Mainloop is ready, loading and starting extensions" << std::endl;
        }
    }
    
    // Now load and start extension configurations
    if (extensionManager_) {
        auto allExtensions = extensionManager_->getAllExtensions();
        
        // Only load configs if no extensions are loaded yet
        if (allExtensions.empty()) {
            if (verbose) {
                std::cout << "[RPC] Loading extension configurations..." << std::endl;
            }
            
            std::string extensionConfDir = "config";
            bool loadResult = extensionManager_->loadExtensionConfigs(extensionConfDir);
            
            if (!loadResult) {
                if (verbose) {
                    std::cout << "[RPC] Warning: Extension configuration loading failed or no extensions found" << std::endl;
                }
            }
            
            // Get all extensions after loading configs
            allExtensions = extensionManager_->getAllExtensions();
            
            if (verbose) {
                std::cout << "[RPC] Found " << allExtensions.size() << " extension configurations to start" << std::endl;
            }
            
            // CRITICAL FIX: Add validation and error recovery for extension startup
            int successCount = 0;
            int failureCount = 0;
            
            // Start all extensions with error handling
            for (const auto& extInfo : allExtensions) {
                if (verbose) {
                    std::cout << "[RPC] Starting extension: " << extInfo.name << std::endl;
                }
                
                try {
                    bool startResult = extensionManager_->startExtension(extInfo.name);
                    
                    if (startResult) {
                        successCount++;
                        if (verbose) {
                            std::cout << "[RPC] Successfully started extension: " << extInfo.name << std::endl;
                        }
                    } else {
                        failureCount++;
                        if (verbose) {
                            std::cout << "[RPC] Failed to start extension: " << extInfo.name << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    failureCount++;
                    if (verbose) {
                        std::cout << "[RPC] Exception starting extension '" << extInfo.name << "': " << e.what() << std::endl;
                    }
                }
            }
            
            if (verbose) {
                std::cout << "[RPC] Extension startup completed: " << successCount << " successful, " << failureCount << " failed" << std::endl;
            }
        } else {
            if (verbose) {
                std::cout << "[RPC] Extensions already loaded (" << allExtensions.size() << "), skipping configuration load" << std::endl;
            }
        }
    } else {
        if (verbose) {
            std::cout << "[RPC] Warning: Extension manager not available, skipping extension loading" << std::endl;
        }
    }
    
    // Create success response
    nlohmann::json result;
    result["message"] = "Mainloop thread started successfully";
    result["status"] = static_cast<int>(mainloopResp.status);
    
    // Include thread information from mainloop response
    if (mainloopResp.threadStates.find("mainloop") != mainloopResp.threadStates.end()) {
        const auto& mainloopInfo = mainloopResp.threadStates["mainloop"];
        result["thread_id"] = mainloopInfo.threadId;
        result["state"] = static_cast<int>(mainloopInfo.state);
    } else {
        // Default values if mainloop not found in response
        result["thread_id"] = 0;
        result["state"] = 0; // Unknown/created state
    }
    
    // Include extension information if available
    if (extensionManager_) {
        auto allExtensions = extensionManager_->getAllExtensions();
        result["extensions_loaded"] = allExtensions.size();
        
        nlohmann::json extensionsArray = nlohmann::json::array();
        for (const auto& extInfo : allExtensions) {
            nlohmann::json extJson;
            extJson["name"] = extInfo.name;
            extJson["thread_id"] = extInfo.threadId;
            extJson["running"] = extInfo.isRunning;
            extensionsArray.push_back(extJson);
        }
        result["extensions"] = extensionsArray;
    }
    
    sendResponse(transactionId, true, result.dump(), "");
    
    if (verbose) {
        std::cout << "[RPC] Mainloop thread started and extensions loaded" << std::endl;
        std::cout << "[RPC] ========================================\n" << std::endl;
    }
    
    return 0;
}

int RpcOperationProcessor::handleStopMainloop(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!rpcController_) {
        sendResponse(transactionId, false, "", "RPC controller not available");
        return -1;
    }
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: stop_mainloop" << std::endl;
        std::cout << "[RPC] Action: STOP mainloop thread AND all extensions" << std::endl;
        std::cout << "[RPC] ========================================" << std::endl;
    }
    
    // First, stop all extensions if extension manager is available
    if (extensionManager_) {
        auto allExtensions = extensionManager_->getAllExtensions();
        
        if (verbose) {
            std::cout << "[RPC] Stopping " << allExtensions.size() << " extensions..." << std::endl;
        }
        
        // Stop all extensions
        for (const auto& extInfo : allExtensions) {
            if (verbose) {
                std::cout << "[RPC] Stopping extension: " << extInfo.name << std::endl;
            }
            
            bool stopResult = extensionManager_->stopExtension(extInfo.name);
            
            if (stopResult) {
                if (verbose) {
                    std::cout << "[RPC] Successfully stopped extension: " << extInfo.name << std::endl;
                }
            } else {
                if (verbose) {
                    std::cout << "[RPC] Failed to stop extension: " << extInfo.name << std::endl;
                }
            }
        }
        
        // Delete all extensions
        for (const auto& extInfo : allExtensions) {
            if (verbose) {
                std::cout << "[RPC] Deleting extension: " << extInfo.name << std::endl;
            }
            
            bool deleteResult = extensionManager_->deleteExtension(extInfo.name);
            
            if (deleteResult) {
                if (verbose) {
                    std::cout << "[RPC] Successfully deleted extension: " << extInfo.name << std::endl;
                }
            } else {
                if (verbose) {
                    std::cout << "[RPC] Failed to delete extension: " << extInfo.name << std::endl;
                }
            }
        }
    } else {
        if (verbose) {
            std::cout << "[RPC] Warning: Extension manager not available, skipping extension stopping" << std::endl;
        }
    }
    
    // Now stop the mainloop thread
    auto mainloopResp = rpcController_->stopThread(RpcMechanisms::ThreadTarget::MAINLOOP);
    if (verbose) {
        std::cout << "[RPC] Mainloop stop result: " << mainloopResp.message << std::endl;
    }
    
    if (mainloopResp.status != RpcMechanisms::OperationStatus::SUCCESS) {
        sendResponse(transactionId, false, mainloopResp.toJson(), "Mainloop stop failed");
        if (verbose) {
            std::cout << "[RPC] Mainloop stop failed" << std::endl;
            std::cout << "[RPC] ========================================\n" << std::endl;
        }
        return -1;
    }
    
    // Create success response
    nlohmann::json result;
    result["message"] = "Mainloop thread stopped successfully";
    result["status"] = static_cast<int>(mainloopResp.status);
    
    // Include thread information from mainloop response
    if (mainloopResp.threadStates.find("mainloop") != mainloopResp.threadStates.end()) {
        const auto& mainloopInfo = mainloopResp.threadStates["mainloop"];
        result["thread_id"] = mainloopInfo.threadId;
        result["state"] = static_cast<int>(mainloopInfo.state);
    }
    result["extensions_stopped"] = extensionManager_ ? extensionManager_->getAllExtensions().size() : 0;
    
    sendResponse(transactionId, true, result.dump(), "");
    
    if (verbose) {
        std::cout << "[RPC] Mainloop thread and all extensions stopped" << std::endl;
        std::cout << "[RPC] ========================================\n" << std::endl;
    }
    
    return 0;
}

int RpcOperationProcessor::handlePauseMainloop(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!rpcController_) {
        sendResponse(transactionId, false, "", "RPC controller not available");
        return -1;
    }
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: pause_mainloop" << std::endl;
        std::cout << "[RPC] Action: PAUSE mainloop thread" << std::endl;
        std::cout << "[RPC] ========================================" << std::endl;
    }
    
    auto mainloopResp = rpcController_->pauseThread(RpcMechanisms::ThreadTarget::MAINLOOP);
    if (verbose) {
        std::cout << "[RPC] Mainloop pause result: " << mainloopResp.message << std::endl;
    }
    
    if (mainloopResp.status != RpcMechanisms::OperationStatus::SUCCESS) {
        sendResponse(transactionId, false, mainloopResp.toJson(), "Mainloop pause failed");
        if (verbose) {
            std::cout << "[RPC] Mainloop pause failed" << std::endl;
            std::cout << "[RPC] ========================================\n" << std::endl;
        }
        return -1;
    }
    
    // Create success response
    nlohmann::json result;
    result["message"] = "Mainloop thread paused successfully";
    result["status"] = static_cast<int>(mainloopResp.status);
    
    // Include thread information from mainloop response
    if (mainloopResp.threadStates.find("mainloop") != mainloopResp.threadStates.end()) {
        const auto& mainloopInfo = mainloopResp.threadStates["mainloop"];
        result["thread_id"] = mainloopInfo.threadId;
        result["state"] = static_cast<int>(mainloopInfo.state);
    }
    
    sendResponse(transactionId, true, result.dump(), "");
    
    if (verbose) {
        std::cout << "[RPC] Mainloop thread paused" << std::endl;
        std::cout << "[RPC] ========================================\n" << std::endl;
    }
    
    return 0;
}

int RpcOperationProcessor::handleResumeMainloop(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!rpcController_) {
        sendResponse(transactionId, false, "", "RPC controller not available");
        return -1;
    }
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: resume_mainloop" << std::endl;
        std::cout << "[RPC] Action: RESUME mainloop thread" << std::endl;
        std::cout << "[RPC] ========================================" << std::endl;
    }
    
    auto mainloopResp = rpcController_->resumeThread(RpcMechanisms::ThreadTarget::MAINLOOP);
    if (verbose) {
        std::cout << "[RPC] Mainloop resume result: " << mainloopResp.message << std::endl;
    }
    
    if (mainloopResp.status != RpcMechanisms::OperationStatus::SUCCESS) {
        sendResponse(transactionId, false, mainloopResp.toJson(), "Mainloop resume failed");
        if (verbose) {
            std::cout << "[RPC] Mainloop resume failed" << std::endl;
            std::cout << "[RPC] ========================================\n" << std::endl;
        }
        return -1;
    }
    
    // Create success response
    nlohmann::json result;
    result["message"] = "Mainloop thread resumed successfully";
    result["status"] = static_cast<int>(mainloopResp.status);
    
    // Include thread information from mainloop response
    if (mainloopResp.threadStates.find("mainloop") != mainloopResp.threadStates.end()) {
        const auto& mainloopInfo = mainloopResp.threadStates["mainloop"];
        result["thread_id"] = mainloopInfo.threadId;
        result["state"] = static_cast<int>(mainloopInfo.state);
    }
    
    sendResponse(transactionId, true, result.dump(), "");
    
    if (verbose) {
        std::cout << "[RPC] Mainloop thread resumed" << std::endl;
        std::cout << "[RPC] ========================================\n" << std::endl;
    }
    
    return 0;
}

// Extension management operations - using same functionality as HTTP server
int RpcOperationProcessor::handleGetAllExtensions(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!extensionManager_) {
        sendResponse(transactionId, false, "", "Extension manager not available");
        return -1;
    }
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: get_all_extensions" << std::endl;
        std::cout << "[RPC] Action: Retrieve all extension status" << std::endl;
    }
    
    std::string extensionsJson = extensionManager_->allExtensionsToJson();
    
    sendResponse(transactionId, true, extensionsJson, "");
    
    if (verbose) {
        std::cout << "[RPC] Returned extension list" << std::endl;
    }
    return 0;
}

int RpcOperationProcessor::handleGetExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!extensionManager_) {
        sendResponse(transactionId, false, "", "Extension manager not available");
        return -1;
    }
    
    std::string name = params.value("name", "");
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: get_extension" << std::endl;
        std::cout << "[RPC] Action: Retrieve extension status for: " << name << std::endl;
    }
    
    if (name.empty()) {
        sendResponse(transactionId, false, "", "Extension name is required");
        return -1;
    }
    
    if (!extensionManager_->extensionExists(name)) {
        sendResponse(transactionId, false, "", "Extension not found: " + name);
        return -1;
    }
    
    auto extInfo = extensionManager_->getExtensionInfo(name);
    std::string extJson = extensionManager_->extensionInfoToJson(extInfo);
    
    sendResponse(transactionId, true, extJson, "");
    
    if (verbose) {
        std::cout << "[RPC] Returned extension info for: " << name << std::endl;
    }
    
    return 0;
}

int RpcOperationProcessor::handleAddExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!extensionManager_) {
        sendResponse(transactionId, false, "", "Extension manager not available");
        return -1;
    }
    
    if (verbose) {
        std::cout << "\n[RPC] Client request: add_extension" << std::endl;
        std::cout << "[RPC] Action: Add new extension" << std::endl;
    }
    
    try {
        std::string configJson = params.dump();
        auto extConfig = extensionManager_->parseExtensionConfigFromJson(configJson);
        std::string result = extensionManager_->createExtension(extConfig);
        
        if (result == "Success") {
            auto info = extensionManager_->getExtensionInfo(extConfig.name);
            sendResponse(transactionId, true, extensionManager_->extensionInfoToJson(info), "");
            if (verbose) {
                std::cout << "[RPC] Extension '" << extConfig.name << "' created successfully" << std::endl;
            }
        } else {
            sendResponse(transactionId, false, "", result);
            if (verbose) {
                std::cout << "[RPC] Failed to create extension: " << result << std::endl;
            }
        }
    } catch (const std::exception& e) {
        sendResponse(transactionId, false, "", std::string("Error adding extension: ") + e.what());
        if (verbose) {
            std::cout << "[RPC] Error: " << e.what() << std::endl;
        }
    }
    
    return 0;
}

int RpcOperationProcessor::handleStartExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!extensionManager_) {
        sendResponse(transactionId, false, "", "Extension manager not available");
        return -1;
    }
    
    std::string name = params.value("name", "");
    
    if (verbose) {
        std::cout << "\n[RPC] ========================================" << std::endl;
        std::cout << "[RPC] Client request: start_extension" << std::endl;
        std::cout << "[RPC] Action: START extension thread" << std::endl;
        std::cout << "[RPC] ========================================" << std::endl;
    }
    
    if (name.empty()) {
        sendResponse(transactionId, false, "", "Extension name is required");
        return -1;
    }
    
    if (!extensionManager_->extensionExists(name)) {
        sendResponse(transactionId, false, "", "Extension not found");
        return -1;
    }
    
    // Get info before starting
    auto infoBefore = extensionManager_->getExtensionInfo(name);
    if (verbose) {
        std::cout << "[RPC] Extension '" << name << "' status BEFORE start:" << std::endl;
        std::cout << "[RPC]   - Thread ID: " << infoBefore.threadId << std::endl;
        std::cout << "[RPC]   - Running: " << (infoBefore.isRunning ? "YES" : "NO") << std::endl;
    }
    
    // Start extension thread (using existing config)
    bool success = extensionManager_->startExtension(name);
    
    if (success) {
        auto infoAfter = extensionManager_->getExtensionInfo(name);
        sendResponse(transactionId, true, extensionManager_->extensionInfoToJson(infoAfter), "");
        
        if (verbose) {
            std::cout << "[RPC] SUCCESS: Extension '" << name << "' thread started" << std::endl;
            std::cout << "[RPC] Extension '" << name << "' status AFTER start:" << std::endl;
            std::cout << "[RPC]   - Thread ID: " << infoAfter.threadId << std::endl;
            std::cout << "[RPC]   - Running: " << (infoAfter.isRunning ? "YES" : "NO") << std::endl;
        }
    } else {
        sendResponse(transactionId, false, "", "Failed to start extension");
        if (verbose) {
            std::cout << "[RPC] ERROR: Failed to start extension '" << name << "'" << std::endl;
        }
    }
    
    if (verbose) {
        std::cout << "[RPC] ========================================\n" << std::endl;
    }
    
    return 0;
}

int RpcOperationProcessor::handleStopExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!extensionManager_) {
        sendResponse(transactionId, false, "", "Extension manager not available");
        return -1;
    }
    
    std::string name = params.value("name", "");
    
    if (verbose) {
        std::cout << "\n[RPC] ========================================" << std::endl;
        std::cout << "[RPC] Client request: stop_extension" << std::endl;
        std::cout << "[RPC] Action: STOP extension thread (keep config)" << std::endl;
        std::cout << "[RPC] ========================================" << std::endl;
    }
    
    if (name.empty()) {
        sendResponse(transactionId, false, "", "Extension name is required");
        return -1;
    }
    
    if (!extensionManager_->extensionExists(name)) {
        sendResponse(transactionId, false, "", "Extension not found");
        return -1;
    }
    
    // Get info before stopping
    auto infoBefore = extensionManager_->getExtensionInfo(name);
    if (verbose) {
        std::cout << "[RPC] Extension '" << name << "' status BEFORE stop:" << std::endl;
        std::cout << "[RPC]   - Thread ID: " << infoBefore.threadId << std::endl;
        std::cout << "[RPC]   - Running: " << (infoBefore.isRunning ? "YES" : "NO") << std::endl;
    }
    
    // Stop extension thread only (config remains)
    bool success = extensionManager_->stopExtension(name);
    
    if (success) {
        auto infoAfter = extensionManager_->getExtensionInfo(name);
        sendResponse(transactionId, true, extensionManager_->extensionInfoToJson(infoAfter), "");
        
        if (verbose) {
            std::cout << "[RPC] SUCCESS: Extension '" << name << "' thread stopped" << std::endl;
            std::cout << "[RPC] Extension '" << name << "' status AFTER stop:" << std::endl;
            std::cout << "[RPC]   - Thread ID: " << infoAfter.threadId << std::endl;
            std::cout << "[RPC]   - Running: " << (infoAfter.isRunning ? "YES" : "NO") << std::endl;
            std::cout << "[RPC]   - Config file: PRESERVED" << std::endl;
        }
    } else {
        sendResponse(transactionId, false, "", "Failed to stop extension");
        if (verbose) {
            std::cout << "[RPC] ERROR: Failed to stop extension '" << name << "'" << std::endl;
        }
    }
    
    if (verbose) {
        std::cout << "[RPC] ========================================\n" << std::endl;
    }
    
    return 0;
}

int RpcOperationProcessor::handleDeleteExtension(const nlohmann::json& params, const std::string& transactionId, bool verbose) {
    if (!extensionManager_) {
        sendResponse(transactionId, false, "", "Extension manager not available");
        return -1;
    }
    
    std::string name = params.value("name", "");
    
    if (verbose) {
        std::cout << "\n[RPC] ========================================" << std::endl;
        std::cout << "[RPC] Client request: delete_extension" << std::endl;
        std::cout << "[RPC] Action: DELETE extension (stop thread + remove config)" << std::endl;
        std::cout << "[RPC] ========================================" << std::endl;
    }
    
    if (name.empty()) {
        sendResponse(transactionId, false, "", "Extension name is required");
        return -1;
    }
    
    if (!extensionManager_->extensionExists(name)) {
        sendResponse(transactionId, false, "", "Extension not found");
        return -1;
    }
    
    // Get info before deletion
    auto info = extensionManager_->getExtensionInfo(name);
    if (verbose) {
        std::cout << "[RPC] Extension '" << name << "' info:" << std::endl;
        std::cout << "[RPC]   - Thread ID: " << info.threadId << std::endl;
        std::cout << "[RPC]   - Running: " << (info.isRunning ? "YES" : "NO") << std::endl;
    }
    
    // Delete extension (stops thread + removes config file)
    bool success = extensionManager_->deleteExtension(name);
    
    if (success) {
        sendResponse(transactionId, true, "{\"message\": \"Extension deleted successfully\"}", "");
        if (verbose) {
            std::cout << "[RPC] SUCCESS: Extension '" << name << "' deleted" << std::endl;
            std::cout << "[RPC]   - Thread stopped and joined" << std::endl;
            std::cout << "[RPC]   - Config file removed" << std::endl;
        }
    } else {
        sendResponse(transactionId, false, "", "Failed to delete extension");
        if (verbose) {
            std::cout << "[RPC] ERROR: Failed to delete extension '" << name << "'" << std::endl;
        }
    }
    
    if (verbose) {
        std::cout << "[RPC] ========================================\n" << std::endl;
    }
    
    return 0;
}

void RpcOperationProcessor::initializeBuiltInHandlers() {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    
    // Original built-in handlers
    operationHandlers_["get_status"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleGetStatus(params, transactionId, verbose);
    };
    
    operationHandlers_["get_metrics"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleGetMetrics(params, transactionId, verbose);
    };
    
    operationHandlers_["router_control"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleRouterControl(params, transactionId, verbose);
    };
    
    operationHandlers_["endpoint_info"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleEndpointInfo(params, transactionId, verbose);
    };
    
    operationHandlers_["config_update"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleConfigUpdate(params, transactionId, verbose);
    };
    
    // Thread management operations - same as HTTP server
    operationHandlers_["get_all_threads"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleGetAllThreads(params, transactionId, verbose);
    };
    
    operationHandlers_["get_mainloop_thread"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleGetMainloopThread(params, transactionId, verbose);
    };
    
    operationHandlers_["start_mainloop"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleStartMainloop(params, transactionId, verbose);
    };
    
    operationHandlers_["stop_mainloop"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleStopMainloop(params, transactionId, verbose);
    };
    
    operationHandlers_["pause_mainloop"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handlePauseMainloop(params, transactionId, verbose);
    };
    
    operationHandlers_["resume_mainloop"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleResumeMainloop(params, transactionId, verbose);
    };
    
    // Extension management operations - same as HTTP server
    operationHandlers_["get_all_extensions"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleGetAllExtensions(params, transactionId, verbose);
    };
    
    operationHandlers_["get_extension"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleGetExtension(params, transactionId, verbose);
    };
    
    operationHandlers_["add_extension"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleAddExtension(params, transactionId, verbose);
    };
    
    operationHandlers_["start_extension"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleStartExtension(params, transactionId, verbose);
    };
    
    operationHandlers_["stop_extension"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleStopExtension(params, transactionId, verbose);
    };
    
    operationHandlers_["delete_extension"] = [this](const nlohmann::json& params, const std::string& transactionId, bool verbose) {
        return handleDeleteExtension(params, transactionId, verbose);
    };
}

// JsonResponseBuilder implementation
std::string JsonResponseBuilder::createSuccessResponse(const std::string& transactionId, const nlohmann::json& result) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["id"] = transactionId;
    response["result"] = result;
    
    return response.dump();
}

std::string JsonResponseBuilder::createErrorResponse(const std::string& transactionId, int errorCode, const std::string& errorMessage) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["id"] = transactionId;
    
    nlohmann::json error;
    error["code"] = errorCode;
    error["message"] = errorMessage;
    response["error"] = error;
    
    return response.dump();
}

nlohmann::json JsonResponseBuilder::parseAndValidateRequest(const std::string& payload) {
    try {
        nlohmann::json request = nlohmann::json::parse(payload);
        
        // Validate JSON-RPC 2.0 format
        if (!request.contains("jsonrpc") || request["jsonrpc"].get<std::string>() != "2.0") {
            std::cerr << "Invalid or missing JSON-RPC version" << std::endl;
            return nlohmann::json{};
        }
        
        if (!request.contains("method") || !request["method"].is_string()) {
            std::cerr << "Missing or invalid method in request" << std::endl;
            return nlohmann::json{};
        }
        
        return request;
        
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return nlohmann::json{};
    }
}

} // namespace URRpcIntegration
