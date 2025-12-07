#include "rpc_operation_processor.hpp"
#include "MavlinkCollectorThread.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <stdexcept>
#include <chrono>
#include <atomic>
#include <map>
#include <mutex>
#include <condition_variable>

using json = nlohmann::json;

// Global MAVLink collector thread ID for RPC operations
static unsigned int g_collectorThreadId = 0;
static unsigned int g_publisherThreadId = 0;

// External global variables
extern std::atomic<bool> g_collector_running;

// Device discovery state
static std::atomic<bool> g_discoveryInProgress{false};
static std::atomic<bool> g_startupDiscoveryCompleted{false};
static std::map<std::string, json> g_pendingDiscoveryResponses;
static std::mutex g_discoveryMutex;
static std::condition_variable g_discoveryCondition;

extern "C" {
#include "../../ur-rpc-template/extensions/direct_template.h"
}

RpcOperationProcessor::RpcOperationProcessor(const PackageConfig& config, bool verbose)
    : config_(std::make_shared<const PackageConfig>(config))  // Create immutable shared pointer
    , verbose_(verbose)
    , responseTopic_("direct_messaging/ur-mavcollector/responses") {
    // Initialize thread manager with larger pool to handle concurrent requests
    threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(100);
    
    std::cout << "[RPC Processor] Constructor called - this=" << this 
              << ", isShuttingDown=" << isShuttingDown_.load() << std::endl;
    
    if (verbose_) {
        std::cout << "[RPC Processor] Initialized with thread pool size: 100" << std::endl;
        std::cout << "[RPC Processor] PackageConfig stored as immutable shared_ptr to prevent corruption" << std::endl;
        std::cout << "[RPC Processor] Device discovery state initialized" << std::endl;
    }
    
    // Trigger startup device discovery
    triggerStartupDeviceDiscovery();
}

RpcOperationProcessor::~RpcOperationProcessor() {
    std::cout << "[RPC Processor] Destructor called - this=" << this 
              << ", setting shutdown flag" << std::endl;
    
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
    if (verbose_) {
        std::cout << "[RPC Processor] Shutting down, waiting for active threads..." << std::endl;
    }
    
    // Join all active threads before cleanup
    std::vector<unsigned int> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        threadsToJoin.assign(activeThreads_.begin(), activeThreads_.end());
        if (verbose_ && !threadsToJoin.empty()) {
            std::cout << "[RPC Processor] Waiting for " << threadsToJoin.size() << " active threads to complete" << std::endl;
        }
    }
    
    // Join threads outside the lock to avoid deadlock
    for (unsigned int threadId : threadsToJoin) {
        try {
            if (threadManager_ && threadManager_->isThreadAlive(threadId)) {
                if (verbose_) {
                    std::cout << "[RPC Processor] Waiting for thread " << threadId << " to complete..." << std::endl;
                }
                
                bool completed = threadManager_->joinThread(threadId, std::chrono::minutes(5));
                
                if (!completed) {
                    std::cerr << "[RPC Processor] WARNING: Thread " << threadId 
                              << " did not complete after 5 minutes - potential deadlock" << std::endl;
                } else if (verbose_) {
                    std::cout << "[RPC Processor] Thread " << threadId << " completed successfully" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            if (verbose_) {
                std::cerr << "[RPC Processor] Error joining thread " << threadId << ": " << e.what() << std::endl;
            }
        }
    }
    
    if (verbose_) {
        std::cout << "[RPC Processor] All threads joined, thread manager will clean up automatically" << std::endl;
    }
}

void RpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
}

void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    if (!payload || payload_len == 0) {
        std::cerr << "[RPC Processor] Empty payload received" << std::endl;
        return;
    }

    // Validate payload size (max 1MB to prevent memory issues)
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        std::cerr << "[RPC Processor] Payload too large: " << payload_len 
                  << " bytes (max: " << MAX_PAYLOAD_SIZE << " bytes)" << std::endl;
        return;
    }

    if (verbose_) {
        std::cout << "[RPC Processor] Processing request - payload size: " << payload_len << " bytes" << std::endl;
    }

    try {
        // Parse JSON using nlohmann json
        json root;
        try {
            root = json::parse(payload, payload + payload_len);
        } catch (const json::parse_error& e) {
            std::cerr << "[RPC Processor] JSON parse error: " << e.what() << std::endl;
            std::cerr << "[RPC Processor]   - Exception id: " << e.id << std::endl;
            std::cerr << "[RPC Processor]   - Byte position: " << e.byte << std::endl;
            return;
        }

        if (verbose_) {
            std::cout << "[RPC Processor] JSON parsed successfully" << std::endl;
        }

        // Extract method and transaction_id
        if (!root.contains("jsonrpc") || !root["jsonrpc"].is_string() || root["jsonrpc"].get<std::string>() != "2.0") {
             std::cerr << "[RPC Processor] Invalid or missing JSON-RPC version" << std::endl;
             return;
        }

        // Extract transaction_id (use "id" for JSON-RPC 2.0)
        std::string transactionId;
        if (root.contains("id")) {
            if (root["id"].is_string()) {
                transactionId = root["id"].get<std::string>();
            } else if (root["id"].is_number()) {
                transactionId = std::to_string(root["id"].get<int>());
            } else {
                transactionId = "unknown";
            }
        } else {
            transactionId = "unknown";
        }

        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponse(transactionId, false, "", "Missing method in request");
            return;
        }

        std::string method = root["method"].get<std::string>();

        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponse(transactionId, false, "", "Missing or invalid params in request");
            return;
        }

        // Serialize to string
        std::string requestJson;
        try {
            requestJson = root.dump();
        } catch (const std::exception& e) {
            std::cerr << "[RPC Processor] Failed to serialize JSON to string: " << e.what() << std::endl;
            return;
        }

        size_t jsonStrLen = requestJson.size();
        if (verbose_) {
            std::cout << "[RPC Processor] JSON string length: " << jsonStrLen << " bytes" << std::endl;
        }

        // Validate JSON string size
        const size_t MAX_JSON_SIZE = 512 * 1024; // 512KB
        if (jsonStrLen > MAX_JSON_SIZE) {
            std::cerr << "[RPC Processor] JSON string too large: " << jsonStrLen 
                      << " bytes (max: " << MAX_JSON_SIZE << " bytes)" << std::endl;
            return;
        }

        if (verbose_) {
            std::cout << "[RPC Processor] Processing request with ID: " 
                      << transactionId << ", method: " << method << std::endl;
        }

        // Validate threadManager_ is valid before creating context
        if (!threadManager_) {
            std::cerr << "[RPC Processor] ThreadManager is null, cannot create thread" << std::endl;
            sendResponse(transactionId, false, "", "ThreadManager is not available");
            return;
        }

        // Capture threadManager_ in a local shared_ptr to keep it alive
        auto threadMgr = threadManager_;
        
        // Create context with shared_ptr
        auto context = std::make_shared<RequestContext>(
            requestJson,
            transactionId,
            responseTopic_,
            config_,
            verbose_,
            threadMgr,
            &activeThreads_,
            &threadsMutex_
        );
        
        // Check if we're shutting down - don't create new threads
        bool shuttingDown = isShuttingDown_.load();
        if (verbose_) {
            std::cout << "[RPC Processor] Shutdown flag state: " << (shuttingDown ? "true" : "false") << std::endl;
        }
        if (shuttingDown) {
            std::cerr << "[RPC Processor] Cannot create thread - processor is shutting down" << std::endl;
            sendResponse(transactionId, false, "", "Server is shutting down");
            return;
        }
        
        try {
            // Create thread with context
            unsigned int threadId = threadMgr->createThread([context]() {
                RpcOperationProcessor::processOperationThreadStatic(context);
            });
            
            // Register thread in activeThreads_ FIRST
            {
                std::lock_guard<std::mutex> lock(threadsMutex_);
                activeThreads_.insert(threadId);
            }
            
            // Set thread ID in atomic field
            context->threadId.store(threadId);
            
            // Publish threadId via promise LAST
            context->threadIdPromise->set_value(threadId);

            if (verbose_) {
                std::cout << "[RPC Processor] Thread " << threadId << " created for transaction: " << transactionId << std::endl;
            }
            
            // Periodically cleanup completed threads (every 10th request)
            static std::atomic<int> requestCount{0};
            if (++requestCount % 10 == 0) {
                cleanupCompletedThreads();
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[RPC Processor] Failed to create thread for transaction " << transactionId 
                      << ": " << e.what() << std::endl;
            
            // Fallback: Process synchronously if thread creation fails
            try {
                context->threadIdPromise->set_value(0);
                RpcOperationProcessor::processOperationThreadStatic(context);
                if (verbose_) {
                    std::cout << "[RPC Processor] Synchronous processing completed for transaction " << transactionId << std::endl;
                }
            } catch (const std::exception& syncError) {
                std::cerr << "[RPC Processor] Synchronous processing also failed: " << syncError.what() << std::endl;
                sendResponse(transactionId, false, "", std::string("Processing failed: ") + syncError.what());
            }
            return;
        }

    } catch (const std::bad_alloc& e) {
        std::cerr << "[RPC Processor] CRITICAL: std::bad_alloc caught: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Exception processing request: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    const std::string& requestJson = context->requestJson;
    const std::string& transactionId = context->transactionId;
    const std::string& responseTopic = context->responseTopic;
    std::shared_ptr<const PackageConfig> config = context->config;
    bool verbose = context->verbose;
    
    // Wait for threadId to be published
    unsigned int threadId = context->threadIdFuture.get();
    
    // Extract cleanup info
    std::set<unsigned int>* activeThreads = context->activeThreads;
    std::mutex* threadsMutex = context->threadsMutex;
    
    if (verbose) {
        std::cerr << "[RPC Thread " << threadId << "/" << transactionId << "] Thread started, parsing JSON (size: " << requestJson.size() << " bytes)" << std::endl;
    }

    try {
        // Parse request again in thread context
        json root = json::parse(requestJson);
        if (verbose) {
            std::cerr << "[RPC Thread " << transactionId << "] JSON parsed successfully in thread" << std::endl;
        }

        // Extract method
        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponseStatic(transactionId, false, "", "Missing method in request", responseTopic);
            return;
        }
        std::string method = root["method"].get<std::string>();

        // Extract params
        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponseStatic(transactionId, false, "", "Missing or invalid params in request", responseTopic);
            return;
        }
        json paramsObj = root["params"];

        // Process MAVLink-specific operations
        json result;
        bool success = true;
        std::string errorMsg = "";

        if (method == "mavlink_device_added") {
            // Handle MAVLink device added event
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Processing mavlink_device_added" << std::endl;
            }
            
            // Extract device information from params
            DeviceInfo deviceInfo;
            deviceInfo.isValid = true;
            
            if (paramsObj.contains("devicePath")) {
                deviceInfo.devicePath = paramsObj["devicePath"].get<std::string>();
            }
            if (paramsObj.contains("baudrate")) {
                deviceInfo.baudrate = paramsObj["baudrate"].get<int>();
            }
            if (paramsObj.contains("systemId")) {
                deviceInfo.systemId = paramsObj["systemId"].get<int>();
            }
            if (paramsObj.contains("componentId")) {
                deviceInfo.componentId = paramsObj["componentId"].get<int>();
            }
            if (paramsObj.contains("mavlinkVersion")) {
                deviceInfo.mavlinkVersion = paramsObj["mavlinkVersion"].get<int>();
            }
            if (paramsObj.contains("state")) {
                deviceInfo.state = paramsObj["state"].get<std::string>();
            }
            if (paramsObj.contains("timestamp")) {
                deviceInfo.timestamp = paramsObj["timestamp"].get<std::string>();
            }
            
            // Extract USB info if available
            if (paramsObj.contains("usbInfo")) {
                auto usbInfo = paramsObj["usbInfo"];
                if (usbInfo.contains("autopilotType")) {
                    deviceInfo.autopilotType = usbInfo["autopilotType"].get<std::string>();
                }
                if (usbInfo.contains("boardClass")) {
                    deviceInfo.boardClass = usbInfo["boardClass"].get<std::string>();
                }
                if (usbInfo.contains("boardName")) {
                    deviceInfo.boardName = usbInfo["boardName"].get<std::string>();
                }
                if (usbInfo.contains("deviceName")) {
                    deviceInfo.deviceName = usbInfo["deviceName"].get<std::string>();
                }
                if (usbInfo.contains("manufacturer")) {
                    deviceInfo.manufacturer = usbInfo["manufacturer"].get<std::string>();
                }
                if (usbInfo.contains("serialNumber")) {
                    deviceInfo.serialNumber = usbInfo["serialNumber"].get<std::string>();
                }
                if (usbInfo.contains("vendorId")) {
                    deviceInfo.vendorId = usbInfo["vendorId"].get<std::string>();
                }
                if (usbInfo.contains("productId")) {
                    deviceInfo.productId = usbInfo["productId"].get<std::string>();
                }
            }
            
            // Store device info in vehicle
            vehicle.setDeviceInfo(deviceInfo);
            
            // Start MAVLink collector if not already running
            if (g_collectorThreadId > 0) {
                result["status"] = "already_running";
                result["thread_id"] = g_collectorThreadId;
                result["message"] = "MAVLink device added but collector was already running";
                result["device_info"] = {
                    {"devicePath", deviceInfo.devicePath},
                    {"systemId", deviceInfo.systemId},
                    {"componentId", deviceInfo.componentId}
                };
            } else {
                // Start collector using global running flag
                g_collectorThreadId = startMavlinkCollector(*config, &g_collector_running);
                
                if (g_collectorThreadId > 0) {
                    result["status"] = "started";
                    result["thread_id"] = g_collectorThreadId;
                    result["message"] = "MAVLink device added and collector started successfully";
                    result["device_info"] = {
                        {"devicePath", deviceInfo.devicePath},
                        {"systemId", deviceInfo.systemId},
                        {"componentId", deviceInfo.componentId}
                    };
                } else {
                    success = false;
                    errorMsg = "Failed to start MAVLink collector after device added";
                }
            }
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Device added operation completed" << std::endl;
            }
        } else if (method == "mavlink_device_removed") {
            // Handle MAVLink device removed event
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Processing mavlink_device_removed" << std::endl;
            }
            
            // Extract device path for logging
            std::string devicePath;
            if (paramsObj.contains("devicePath")) {
                devicePath = paramsObj["devicePath"].get<std::string>();
            }
            
            // Stop MAVLink collector if running
            if (g_collectorThreadId > 0) {
                stopMavlinkCollector(g_collectorThreadId);
                
                // Clear device data flags to prevent publishing stale data
                clearDeviceData();
                
                // Publish "no device" messages to all topics
                publishNoDeviceMessages();
                
                result["status"] = "stopped";
                result["thread_id"] = g_collectorThreadId;
                result["message"] = "MAVLink device removed and collector stopped successfully";
                result["devicePath"] = devicePath;
                g_collectorThreadId = 0;
                
                // Clear vehicle data
                vehicle = Vehicle(); // Reset vehicle to clear all data
            } else {
                result["status"] = "not_running";
                result["message"] = "MAVLink device removed but collector was not running";
                result["devicePath"] = devicePath;
            }
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Device removed operation completed" << std::endl;
            }
        } else if (method == "get_vehicle_info") {
            // Get vehicle information operation
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Processing get_vehicle_info" << std::endl;
            }
            
            // Return complete vehicle information as JSON
            std::string vehicleJson = vehicle.getVehicleInfoJson();
            result = json::parse(vehicleJson);
            
            // Add collector status
            if (g_collectorThreadId > 0) {
                result["collector_status"] = "running";
                result["collector_thread_id"] = g_collectorThreadId;
            } else {
                result["collector_status"] = "stopped";
                result["collector_thread_id"] = 0;
            }
            
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Get vehicle info operation completed" << std::endl;
            }
        } else {
            success = false;
            errorMsg = "Unknown operation: " + method;
        }

        // Send response
        if (success) {
            sendResponseStatic(transactionId, true, result.dump(), "", responseTopic);
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Operation completed successfully, response sent" << std::endl;
            }
        } else {
            sendResponseStatic(transactionId, false, "", errorMsg, responseTopic);
            if (verbose) {
                std::cerr << "[RPC Thread " << transactionId << "] Operation failed: " << errorMsg << std::endl;
            }
        }

    } catch (const std::bad_alloc& e) {
        std::cerr << "[RPC Thread " << transactionId << "] CRITICAL: std::bad_alloc: " << e.what() << std::endl;
        sendResponseStatic(transactionId, false, "", "Server error - out of memory", responseTopic);
    } catch (const std::exception& e) {
        std::cerr << "[RPC Thread " << transactionId << "] Exception: " << e.what() << std::endl;
        sendResponseStatic(transactionId, false, "", std::string("Exception: ") + e.what(), responseTopic);
    }

    // Clean up this thread from tracking before exiting
    if (activeThreads && threadsMutex) {
        std::lock_guard<std::mutex> lock(*threadsMutex);
        activeThreads->erase(threadId);
        if (verbose) {
            std::cerr << "[RPC Thread " << threadId << "/" << transactionId << "] Removed from active threads, remaining: " << activeThreads->size() << std::endl;
        }
    }
    
    if (verbose) {
        std::cerr << "[RPC Thread " << threadId << "/" << transactionId << "] Thread execution completed" << std::endl;
    }
}

void RpcOperationProcessor::sendResponseStatic(const std::string& transactionId, bool success,
                                                 const std::string& result, const std::string& error,
                                                 const std::string& responseTopic) {
    try {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transactionId;

        if (success) {
            // Add result as object or string
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

        // Convert to string and publish
        std::string responseJson = response.dump();
        direct_client_publish_raw_message(responseTopic.c_str(), 
                                         responseJson.c_str(), 
                                         responseJson.size());

    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Failed to send response: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::sendResponse(const std::string& transactionId, bool success,
                                          const std::string& result, const std::string& error) {
    if (verbose_) {
        std::cout << "[RPC Processor] Sending response for transaction: " 
                  << transactionId << std::endl;
    }
    sendResponseStatic(transactionId, success, result, error, responseTopic_);
}

void RpcOperationProcessor::cleanupCompletedThreads() {
    if (!threadManager_) {
        return;
    }
    
    std::vector<unsigned int> threadsToClean;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        // Find completed threads
        for (unsigned int threadId : activeThreads_) {
            if (!threadManager_->isThreadAlive(threadId)) {
                threadsToClean.push_back(threadId);
            }
        }
        
        // Remove completed threads from tracking
        for (unsigned int threadId : threadsToClean) {
            activeThreads_.erase(threadId);
        }
        
        if (verbose_ && !threadsToClean.empty()) {
            std::cout << "[RPC Processor] Cleaned up " << threadsToClean.size() 
                      << " completed threads, remaining active: " << activeThreads_.size() << std::endl;
        }
    }
}

void RpcOperationProcessor::triggerStartupDeviceDiscovery() {
    if (verbose_) {
        std::cout << "[RPC Processor] Triggering startup device discovery..." << std::endl;
    }
    
    // Start device discovery in a separate thread
    threadManager_->createThread([this]() {
        performStartupDeviceDiscovery();
    });
}

void RpcOperationProcessor::performStartupDeviceDiscovery() {
    if (g_startupDiscoveryCompleted.load()) {
        if (verbose_) {
            std::cout << "[RPC Processor] Startup discovery already completed" << std::endl;
        }
        return;
    }
    
    if (g_discoveryInProgress.exchange(true)) {
        if (verbose_) {
            std::cout << "[RPC Processor] Discovery already in progress" << std::endl;
        }
        return;
    }
    
    try {
        if (verbose_) {
            std::cout << "[RPC Processor] Starting device discovery process..." << std::endl;
        }
        
        // Wait a moment for RPC client to be ready
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Send device discovery request
        std::string transactionId = sendDeviceListRequest();
        if (transactionId.empty()) {
            std::cerr << "[RPC Processor] Failed to send device discovery request" << std::endl;
            g_discoveryInProgress.store(false);
            return;
        }
        
        // Wait for response with 5 second timeout
        if (!waitForDiscoveryResponse(transactionId, std::chrono::seconds(5))) {
            std::cerr << "[RPC Processor] Timeout waiting for device discovery response" << std::endl;
            g_discoveryInProgress.store(false);
            return;
        }
        
        // Process the response
        {
            std::lock_guard<std::mutex> lock(g_discoveryMutex);
            
            if (g_pendingDiscoveryResponses.find(transactionId) != g_pendingDiscoveryResponses.end()) {
                const auto& response = g_pendingDiscoveryResponses[transactionId];
                
                if (response.contains("success") && response["success"].get<bool>()) {
                    json resultData = response["result"];
                    processDiscoveryResponse(resultData);
                    
                    if (verbose_) {
                        std::cout << "[RPC Processor] Startup device discovery completed successfully" << std::endl;
                    }
                } else {
                    std::string errorMsg = response.value("error", "Unknown error");
                    std::cerr << "[RPC Processor] Device discovery request failed: " << errorMsg << std::endl;
                }
            }
        }
        
        g_startupDiscoveryCompleted.store(true);
        
    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Error during device discovery: " << e.what() << std::endl;
    }
    
    g_discoveryInProgress.store(false);
    
    // Clean up pending responses
    {
        std::lock_guard<std::mutex> lock(g_discoveryMutex);
        g_pendingDiscoveryResponses.clear();
    }
    
    if (verbose_) {
        std::cout << "[RPC Processor] Device discovery thread exiting" << std::endl;
    }
}

std::string RpcOperationProcessor::sendDeviceListRequest() {
    try {
        if (verbose_) {
            std::cout << "[RPC Processor] Sending device list request..." << std::endl;
        }
        
        // Generate unique transaction ID
        std::string transactionId = "device_discovery_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        // Create JSON-RPC 2.0 request
        json request = json::object();
        request["jsonrpc"] = "2.0";
        request["id"] = transactionId;
        request["method"] = "device-list";
        
        // Parameters
        json params = json::object();
        params["include_unverified"] = false;
        params["include_usb_info"] = true;
        params["timeout_seconds"] = 3;
        request["params"] = params;
        
        std::string requestStr = request.dump();
        
        // Send request via direct client publish
        direct_client_publish_raw_message("direct_messaging/ur-mavdiscovery/requests", 
                                         requestStr.c_str(), 
                                         requestStr.size());
        
        if (verbose_) {
            std::cout << "[RPC Processor] Sent device discovery request with transaction: " << transactionId << std::endl;
        }
        
        return transactionId;
        
    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Error sending device discovery request: " << e.what() << std::endl;
        return "";
    }
}

bool RpcOperationProcessor::waitForDiscoveryResponse(const std::string& transactionId, std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(g_discoveryMutex);
    
    auto deadline = std::chrono::steady_clock::now() + timeout;
    
    return g_discoveryCondition.wait_until(lock, deadline, [&]() {
        return g_pendingDiscoveryResponses.find(transactionId) != g_pendingDiscoveryResponses.end() || 
               isShuttingDown_.load();
    });
}

void RpcOperationProcessor::processDiscoveryResponse(const nlohmann::json& responseData) {
    try {
        if (verbose_) {
            std::cout << "[RPC Processor] Processing device discovery response" << std::endl;
        }
        
        if (!responseData.contains("devices") || !responseData["devices"].is_array()) {
            std::cerr << "[RPC Processor] Invalid response format - missing devices array" << std::endl;
            return;
        }
        
        auto devicesJson = responseData["devices"];
        
        if (devicesJson.empty()) {
            if (verbose_) {
                std::cout << "[RPC Processor] No devices found in discovery response" << std::endl;
            }
            return;
        }
        
        if (verbose_) {
            std::cout << "[RPC Processor] Found " << devicesJson.size() << " devices, processing..." << std::endl;
        }
        
        // Process first device (simplified - could be extended for multiple devices)
        for (const auto& deviceJson : devicesJson) {
            processDiscoveredDevice(deviceJson);
            break; // Process only first device for now
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Error processing discovery response: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::processDiscoveredDevice(const nlohmann::json& deviceJson) {
    try {
        if (verbose_) {
            std::cout << "[RPC Processor] Processing discovered device..." << std::endl;
        }
        
        // Extract device information
        std::string devicePath = deviceJson.value("devicePath", "");
        int baudrate = deviceJson.value("baudrate", 57600);
        int systemId = deviceJson.value("systemId", 1);
        int componentId = deviceJson.value("componentId", 1);
        
        if (devicePath.empty()) {
            std::cerr << "[RPC Processor] Skipping device with empty path" << std::endl;
            return;
        }
        
        if (verbose_) {
            std::cout << "[RPC Processor] Found device: " << devicePath 
                      << " (sysid:" << systemId << ", compid:" << componentId << ", baud:" << baudrate << ")" << std::endl;
        }
        
        // Create device info for vehicle
        DeviceInfo deviceInfo;
        deviceInfo.devicePath = devicePath;
        deviceInfo.baudrate = baudrate;
        deviceInfo.systemId = systemId;
        deviceInfo.componentId = componentId;
        deviceInfo.isValid = true;
        
        // Extract USB info if available
        if (deviceJson.contains("usbInfo")) {
            auto usbInfo = deviceJson["usbInfo"];
            deviceInfo.autopilotType = usbInfo.value("autopilotType", "");
            deviceInfo.boardClass = usbInfo.value("boardClass", "");
            deviceInfo.boardName = usbInfo.value("boardName", "");
            deviceInfo.deviceName = usbInfo.value("deviceName", "");
            deviceInfo.manufacturer = usbInfo.value("manufacturer", "");
            deviceInfo.serialNumber = usbInfo.value("serialNumber", "");
            deviceInfo.vendorId = usbInfo.value("vendorId", "");
            deviceInfo.productId = usbInfo.value("productId", "");
        }
        
        // Store device info in vehicle
        vehicle.setDeviceInfo(deviceInfo);
        
        // Start MAVLink collector
        if (g_collectorThreadId == 0) {
            g_collectorThreadId = startMavlinkCollector(*config_, &g_collector_running);
            
            if (g_collectorThreadId > 0) {
                if (verbose_) {
                    std::cout << "[RPC Processor] Started MAVLink collector for device: " << devicePath << std::endl;
                }
            } else {
                std::cerr << "[RPC Processor] Failed to start MAVLink collector for device: " << devicePath << std::endl;
            }
        } else {
            if (verbose_) {
                std::cout << "[RPC Processor] MAVLink collector already running for device: " << devicePath << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Error processing discovered device: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::handleDiscoveryResponse(const std::string& topic, const std::string& payload) {
    if (verbose_) {
        std::cout << "[RPC Processor] Discovery response received on topic: " << topic << std::endl;
    }
    
    // Validate that this is a discovery response from ur-mavdiscovery
    if (topic.find("direct_messaging/ur-mavdiscovery/responses") == std::string::npos) {
        if (verbose_) {
            std::cout << "[RPC Processor] Ignoring non-discovery response: " << topic << std::endl;
        }
        return;
    }
    
    try {
        if (payload.empty()) {
            std::cerr << "[RPC Processor] Empty discovery response payload" << std::endl;
            return;
        }
        
        json response = json::parse(payload);
        
        // Check if this is a JSON-RPC 2.0 response
        if (!response.contains("jsonrpc") || response["jsonrpc"].get<std::string>() != "2.0") {
            if (verbose_) {
                std::cout << "[RPC Processor] Ignoring non-JSON-RPC 2.0 response" << std::endl;
            }
            return;
        }
        
        // Extract transaction ID
        std::string transactionId;
        if (response.contains("id")) {
            if (response["id"].is_string()) {
                transactionId = response["id"].get<std::string>();
            } else if (response["id"].is_number()) {
                transactionId = std::to_string(response["id"].get<int>());
            } else {
                std::cerr << "[RPC Processor] Invalid transaction ID type in response" << std::endl;
                return;
            }
        } else {
            std::cerr << "[RPC Processor] Missing transaction ID in response" << std::endl;
            return;
        }
        
        // Check if this is a discovery response
        if (transactionId.find("device_discovery_") != 0) {
            if (verbose_) {
                std::cout << "[RPC Processor] Ignoring non-discovery response: " << transactionId << std::endl;
            }
            return;
        }
        
        // Parse the response
        bool success = false;
        json resultData;
        std::string errorMessage;
        
        if (response.contains("result")) {
            resultData = response["result"];
            success = true;
        }
        
        if (response.contains("error")) {
            if (response["error"].contains("message")) {
                errorMessage = response["error"]["message"].get<std::string>();
            }
            success = false;
        }
        
        // Store the response and notify waiting thread
        {
            std::lock_guard<std::mutex> lock(g_discoveryMutex);
            g_pendingDiscoveryResponses[transactionId] = {
                {"success", success},
                {"result", resultData},
                {"error", errorMessage}
            };
        }
        g_discoveryCondition.notify_all();
        
        if (verbose_) {
            std::cout << "[RPC Processor] Stored discovery response for transaction: " << transactionId 
                      << " (success: " << (success ? "true" : "false") << ")" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Error handling discovery response: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::shutdown() {
    if (verbose_) {
        std::cout << "[RPC Processor] Shutdown initiated..." << std::endl;
    }
    
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
    // Notify all waiting threads to wake up and check shutdown flag
    g_discoveryCondition.notify_all();
    
    // Stop collector if running
    if (g_collectorThreadId > 0) {
        if (verbose_) {
            std::cout << "[RPC Processor] Stopping MAVLink collector thread..." << std::endl;
        }
        stopMavlinkCollector(g_collectorThreadId);
        g_collectorThreadId = 0;
    }
    
    // Stop publisher if running
    if (g_publisherThreadId > 0) {
        if (verbose_) {
            std::cout << "[RPC Processor] Stopping device data publisher thread..." << std::endl;
        }
        stopDeviceDataPublisher();
        g_publisherThreadId = 0;
    }
    
    if (verbose_) {
        std::cout << "[RPC Processor] Shutdown completed" << std::endl;
    }
}
