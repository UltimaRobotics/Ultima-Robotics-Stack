#include "RpcOperationProcessor.hpp"
#include "Logger.hpp"
#include "DeviceStateDB.hpp"
#include "DeviceInfo.hpp"
#include <chrono>

RpcOperationProcessor::RpcOperationProcessor(const DeviceConfig& config, RpcClient& rpcClient, bool verbose)
    : config_(config), verbose_(verbose), responseTopic_("direct_messaging/ur-mavdiscovery/responses"), rpcClient_(&rpcClient) {
    // Initialize ThreadManager with appropriate pool size
    threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(50);
    LOG_INFO("RpcOperationProcessor initialized with thread pool size: 50");
    
    // Note: RPC client is now managed by DeviceManager and passed to us by reference
}

RpcOperationProcessor::~RpcOperationProcessor() {
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
    // Note: RPC client is now managed by DeviceManager, so we don't stop it here
    
    // Collect all active threads for joining
    std::vector<unsigned int> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        threadsToJoin.assign(activeThreads_.begin(), activeThreads_.end());
    }
    
    // Join all threads with timeout
    for (unsigned int threadId : threadsToJoin) {
        if (threadManager_->isThreadAlive(threadId)) {
            bool completed = threadManager_->joinThread(threadId, std::chrono::minutes(5));
            if (!completed) {
                LOG_WARNING("Thread " + std::to_string(threadId) + " did not complete after 5 minutes");
            }
        }
    }
    
    LOG_INFO("RpcOperationProcessor shutdown completed");
}

void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    // Input validation
    if (!payload || payload_len == 0) {
        LOG_ERROR("Empty payload received in RPC request");
        return;
    }

    // Size validation (prevent memory exhaustion)
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        LOG_ERROR("Payload too large: " + std::to_string(payload_len) + " bytes");
        return;
    }

    try {
        // JSON parsing
        json root = json::parse(payload, payload + payload_len);

        // JSON-RPC 2.0 validation
        if (!root.contains("jsonrpc") || root["jsonrpc"].get<std::string>() != "2.0") {
            LOG_ERROR("Invalid or missing JSON-RPC version");
            return;
        }

        // Extract transaction ID
        std::string transactionId = extractTransactionId(root);
        if (transactionId.empty()) {
            LOG_ERROR("Missing or invalid transaction ID in request");
            return;
        }

        // Extract method
        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponse(transactionId, false, "", "Missing method in request");
            return;
        }
        std::string method = root["method"].get<std::string>();

        // Extract parameters
        json params;
        if (root.contains("params")) {
            if (!root["params"].is_object()) {
                sendResponse(transactionId, false, "", "Invalid params in request - must be object");
                return;
            }
            params = root["params"];
        } else {
            params = json::object();
        }

        // Create processing context
        auto context = std::make_shared<RequestContext>();
        context->requestJson = std::string(payload, payload_len);
        context->transactionId = transactionId;
        context->responseTopic = responseTopic_;
        context->config = std::make_shared<DeviceConfig>(config_);
        context->verbose = verbose_;
        context->processor = this; // Add pointer to this processor instance
        context->threadIdPromise = std::make_shared<std::promise<unsigned int>>();
        context->threadIdFuture = context->threadIdPromise->get_future();

        // Check shutdown state
        bool shuttingDown = isShuttingDown_.load();
        if (shuttingDown) {
            sendResponse(transactionId, false, "", "Server is shutting down");
            return;
        }

        try {
            // Create processing thread
            unsigned int threadId = threadManager_->createThread([context]() {
                RpcOperationProcessor::processOperationThreadStatic(context);
            });
            
            // Register thread in tracking set
            {
                std::lock_guard<std::mutex> lock(threadsMutex_);
                activeThreads_.insert(threadId);
            }
            
            // Set thread ID for worker access
            context->threadIdPromise->set_value(threadId);

            if (verbose_) {
                LOG_INFO("Created worker thread " + std::to_string(threadId) + " for method: " + method);
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create processing thread: " + std::string(e.what()));
            
            // Fallback to synchronous processing
            context->threadIdPromise->set_value(0);
            RpcOperationProcessor::processOperationThreadStatic(context);
        }

    } catch (const json::parse_error& e) {
        LOG_ERROR("JSON parse error in RPC request: " + std::string(e.what()));
    } catch (const std::exception& e) {
        LOG_ERROR("Exception processing RPC request: " + std::string(e.what()));
    }
}

void RpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
}

void RpcOperationProcessor::processOperationThread(std::shared_ptr<RequestContext> context) {
    // Wait for thread ID synchronization
    unsigned int threadId = context->threadIdFuture.get();
    
    // Extract context data safely
    const std::string& requestJson = context->requestJson;
    const std::string& transactionId = context->transactionId;
    std::shared_ptr<const DeviceConfig> config = context->config;
    bool verbose = context->verbose;
    
    try {
        // Parse request in thread context
        json root = json::parse(requestJson);
        
        // Extract method and parameters
        std::string method = root["method"].get<std::string>();
        json paramsObj = root.contains("params") ? root["params"] : json::object();
        
        if (verbose) {
            LOG_INFO("Processing RPC method: " + method + " (thread " + std::to_string(threadId) + ")");
        }
        
        // Execute operation based on method
        int exitCode = -1;
        std::string result;
        
        if (method == "device-list") {
            exitCode = handleDeviceList(paramsObj, result);
        } else if (method == "device_info") {
            exitCode = handleDeviceInfo(paramsObj, result);
        } else if (method == "device_verify") {
            exitCode = handleDeviceVerify(paramsObj, result);
        } else if (method == "device_status") {
            exitCode = handleDeviceStatus(paramsObj, result);
        } else if (method == "system_info") {
            exitCode = handleSystemInfo(paramsObj, result);
        } else {
            sendResponseStatic(transactionId, false, "", "Unknown method: " + method, context->responseTopic, context->processor->rpcClient_);
            return;
        }
        
        // Send response based on execution result
        if (exitCode == 0) {
            sendResponseStatic(transactionId, true, result, "", context->responseTopic, context->processor->rpcClient_);
            if (verbose) {
                LOG_INFO("RPC method " + method + " completed successfully");
            }
        } else {
            sendResponseStatic(transactionId, false, "", "Operation failed with exit code: " + std::to_string(exitCode), context->responseTopic, context->processor->rpcClient_);
            if (verbose) {
                LOG_WARNING("RPC method " + method + " failed with exit code: " + std::to_string(exitCode));
            }
        }
        
    } catch (const std::exception& e) {
        sendResponseStatic(transactionId, false, "", 
                          std::string("Exception: ") + e.what(), 
                          context->responseTopic, context->processor->rpcClient_);
        LOG_ERROR("Exception in RPC operation thread: " + std::string(e.what()));
    }

    // Cleanup thread from tracking
    if (threadId != 0) {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        activeThreads_.erase(threadId);
        if (verbose) {
            LOG_INFO("Thread " + std::to_string(threadId) + " completed and cleaned up");
        }
    }
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    // Use the processor instance from context instead of creating a new one
    if (context->processor) {
        context->processor->processOperationThread(context);
    }
}

void RpcOperationProcessor::sendResponse(const std::string& transactionId, bool success, 
                                         const std::string& result, const std::string& error) {
    sendResponseStatic(transactionId, success, result, error, responseTopic_, rpcClient_);
}

void RpcOperationProcessor::sendResponseStatic(const std::string& transactionId, bool success,
                                               const std::string& result, const std::string& error,
                                               const std::string& responseTopic, RpcClient* rpcClient) {
    try {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transactionId;

        if (success) {
            // Handle JSON result parsing
            if (!result.empty() && result[0] == '{') {
                try {
                    json parsedResult = json::parse(result);
                    response["result"] = parsedResult;
                } catch (const json::parse_error&) {
                    response["result"] = result;
                }
            } else if (!result.empty()) {
                response["result"] = result;
            } else {
                response["result"] = "Operation completed successfully";
            }
        } else {
            // Error response format
            json errorObj;
            errorObj["code"] = -1;
            errorObj["message"] = error;
            response["error"] = errorObj;
        }

        // Publish response using provided RPC client
        std::string responseJson = response.dump();
        
        if (rpcClient) {
            rpcClient->sendResponse(responseTopic, responseJson);
            LOG_INFO("RPC Response sent to " + responseTopic + ": " + responseJson);
        } else {
            LOG_ERROR("RPC client is null, cannot send response");
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send RPC response: " + std::string(e.what()));
    }
}

std::string RpcOperationProcessor::extractTransactionId(const json& request) {
    if (request.contains("id")) {
        if (request["id"].is_string()) {
            return request["id"].get<std::string>();
        } else if (request["id"].is_number()) {
            return std::to_string(request["id"].get<int>());
        }
    }
    return "";
}

json RpcOperationProcessor::buildErrorResponse(const std::string& transactionId, const std::string& error) {
    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = transactionId;
    response["error"]["code"] = -1;
    response["error"]["message"] = error;
    return response;
}

json RpcOperationProcessor::buildSuccessResponse(const std::string& transactionId, const std::string& result) {
    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = transactionId;
    
    if (!result.empty() && result[0] == '{') {
        try {
            response["result"] = json::parse(result);
        } catch (const json::parse_error&) {
            response["result"] = result;
        }
    } else {
        response["result"] = result;
    }
    
    return response;
}

// Operation handlers
int RpcOperationProcessor::handleDeviceList(const json& params, std::string& result) {
    (void)params; // Suppress unused parameter warning
    try {
        json devices = json::array();
        
        // Get verified devices from DeviceStateDB
        auto& deviceDB = DeviceStateDB::getInstance();
        auto allDevices = deviceDB.getAllDevices();
        
        for (const auto& deviceInfo : allDevices) {
            // Only include verified devices
            if (deviceInfo->state == DeviceState::VERIFIED) {
                json deviceJson;
                deviceJson["devicePath"] = deviceInfo->devicePath;
                deviceJson["baudrate"] = deviceInfo->baudrate;
                deviceJson["systemId"] = deviceInfo->sysid;
                deviceJson["componentId"] = deviceInfo->compid;
                deviceJson["mavlinkVersion"] = deviceInfo->mavlinkVersion;
                deviceJson["timestamp"] = deviceInfo->timestamp;
                
                // Add USB info if available
                if (!deviceInfo->usbInfo.deviceName.empty()) {
                    deviceJson["deviceName"] = deviceInfo->usbInfo.deviceName;
                }
                if (!deviceInfo->usbInfo.manufacturer.empty()) {
                    deviceJson["manufacturer"] = deviceInfo->usbInfo.manufacturer;
                }
                if (!deviceInfo->usbInfo.serialNumber.empty()) {
                    deviceJson["serialNumber"] = deviceInfo->usbInfo.serialNumber;
                }
                if (!deviceInfo->usbInfo.vendorId.empty()) {
                    deviceJson["vendorId"] = deviceInfo->usbInfo.vendorId;
                }
                if (!deviceInfo->usbInfo.productId.empty()) {
                    deviceJson["productId"] = deviceInfo->usbInfo.productId;
                }
                if (!deviceInfo->usbInfo.boardClass.empty()) {
                    deviceJson["boardClass"] = deviceInfo->usbInfo.boardClass;
                }
                if (!deviceInfo->usbInfo.boardName.empty()) {
                    deviceJson["boardName"] = deviceInfo->usbInfo.boardName;
                }
                if (!deviceInfo->usbInfo.autopilotType.empty()) {
                    deviceJson["autopilotType"] = deviceInfo->usbInfo.autopilotType;
                }
                
                devices.push_back(deviceJson);
            }
        }
        
        json response;
        response["devices"] = devices;
        response["count"] = devices.size();
        
        result = response.dump();
        return 0;
        
    } catch (const std::exception& e) {
        result = "Error getting device list: " + std::string(e.what());
        return -1;
    }
}

int RpcOperationProcessor::handleDeviceInfo(const json& params, std::string& result) {
    try {
        if (!params.contains("device_path")) {
            result = "Missing device_path parameter";
            return -1;
        }
        
        std::string devicePath = params["device_path"].get<std::string>();
        
        // TODO: Integrate with actual DeviceManager to get device info
        json deviceInfo;
        deviceInfo["path"] = devicePath;
        deviceInfo["status"] = "connected";
        deviceInfo["baudrate"] = 115200;
        deviceInfo["system_id"] = 1;
        deviceInfo["component_id"] = 1;
        
        result = deviceInfo.dump();
        return 0;
        
    } catch (const std::exception& e) {
        result = "Error getting device info: " + std::string(e.what());
        return -1;
    }
}

int RpcOperationProcessor::handleDeviceVerify(const json& params, std::string& result) {
    try {
        if (!params.contains("device_path")) {
            result = "Missing device_path parameter";
            return -1;
        }
        
        std::string devicePath = params["device_path"].get<std::string>();
        
        // TODO: Integrate with actual DeviceVerifier
        json verificationResult;
        verificationResult["device_path"] = devicePath;
        verificationResult["verified"] = true;
        verificationResult["timestamp"] = std::time(nullptr);
        
        result = verificationResult.dump();
        return 0;
        
    } catch (const std::exception& e) {
        result = "Error verifying device: " + std::string(e.what());
        return -1;
    }
}

int RpcOperationProcessor::handleDeviceStatus(const json& params, std::string& result) {
    try {
        if (!params.contains("device_path")) {
            result = "Missing device_path parameter";
            return -1;
        }
        
        std::string devicePath = params["device_path"].get<std::string>();
        
        // TODO: Integrate with actual DeviceMonitor
        json statusResult;
        statusResult["device_path"] = devicePath;
        statusResult["status"] = "online";
        statusResult["last_seen"] = std::time(nullptr);
        statusResult["packets_received"] = 1024;
        statusResult["packets_sent"] = 512;
        
        result = statusResult.dump();
        return 0;
        
    } catch (const std::exception& e) {
        result = "Error getting device status: " + std::string(e.what());
        return -1;
    }
}

int RpcOperationProcessor::handleSystemInfo(const json& params, std::string& result) {
    (void)params; // Suppress unused parameter warning
    try {
        json systemInfo;
        systemInfo["service"] = "ur-mavdiscovery-v1.1";
        systemInfo["version"] = "1.1.0";
        systemInfo["uptime"] = 3600; // seconds
        systemInfo["active_devices"] = 2;
        systemInfo["rpc_methods_supported"] = {
            "device_list",
            "device_info", 
            "device_verify",
            "device_status",
            "system_info"
        };
        
        result = systemInfo.dump();
        return 0;
        
    } catch (const std::exception& e) {
        result = "Error getting system info: " + std::string(e.what());
        return -1;
    }
}
