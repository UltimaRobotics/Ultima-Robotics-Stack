#include "DeviceDiscoveryCronJob.hpp"
#include "../common/log.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <ctime>
#include <cstdlib>

using json = nlohmann::json;

DeviceDiscoveryCronJob::DeviceDiscoveryCronJob(const std::string& configPath, bool verbose)
    : configPath_(configPath)
    , verbose_(verbose)
    , threadManager_(std::make_shared<ThreadMgr::ThreadManager>(10)) {
    
    log_info("[DISCOVERY_CRON] DeviceDiscoveryCronJob initialized with heartbeat-triggered discovery");
}

DeviceDiscoveryCronJob::~DeviceDiscoveryCronJob() {
    stop();
}

bool DeviceDiscoveryCronJob::start() {
    if (running_.load()) {
        return true;
    }

    running_.store(true);
    discoveryInProgress_.store(false);
    firstJobCompleted_.store(false);
    
    log_info("[DISCOVERY_CRON] DeviceDiscoveryCronJob started - waiting for heartbeat triggers");
    
    // Start periodic scheduling thread
    startPeriodicScheduling();
    
    return true;
}

void DeviceDiscoveryCronJob::stop() {
    log_info("[DISCOVERY_CRON] Stopping DeviceDiscoveryCronJob...");
    running_.store(false);
    
    // Stop periodic scheduling
    stopPeriodicScheduling();
    
    // Notify any waiting threads
    responseCondition_.notify_all();
    periodicSchedulingCondition_.notify_all();
    
    log_info("[DISCOVERY_CRON] DeviceDiscoveryCronJob stopped");
}

bool DeviceDiscoveryCronJob::isRunning() const {
    return running_.load();
}

void DeviceDiscoveryCronJob::setDiscoveryInterval(std::chrono::seconds interval) {
    discoveryInterval_ = interval;
}

void DeviceDiscoveryCronJob::triggerImmediateDiscovery() {
    if (!running_.load()) {
        log_warning("[DISCOVERY_CRON] Cannot trigger discovery - job not running");
        return;
    }

    // Start discovery in a separate thread
    threadManager_->createThread([this]() {
        this->performDeviceDiscovery();
    });
}

void DeviceDiscoveryCronJob::handleHeartbeatMessage(const std::string& /*topic*/, const std::string& /*payload*/) {
    if (!running_.load()) {
        return;
    }

    // Mark heartbeat as received
    if (!heartbeatReceived_.exchange(true)) {
        log_info("[DISCOVERY_CRON] First heartbeat received from ur-mavdiscovery - cron job is now active");
    }

    // Ignore heartbeats while discovery is in progress
    if (discoveryInProgress_.load()) {
        log_debug("[DISCOVERY_CRON] Ignoring heartbeat - discovery in progress");
        return;
    }

    try {
        log_info("[DISCOVERY_CRON] Heartbeat received from ur-mavdiscovery, starting device discovery");

        // Start discovery in a separate thread
        threadManager_->createThread([this]() {
            this->performDeviceDiscovery();
        });

    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error handling heartbeat: %s", e.what());
    }
}

bool DeviceDiscoveryCronJob::hasReceivedHeartbeat() const {
    return heartbeatReceived_.load();
}

void DeviceDiscoveryCronJob::handleDiscoveryResponse(const std::string& topic, const std::string& payload) {
    if (!running_.load()) {
        log_debug("[DISCOVERY_CRON] Ignoring response - cron job not running");
        return;
    }

    // Only process responses after heartbeat has been received
    if (!heartbeatReceived_.load()) {
        log_debug("[DISCOVERY_CRON] Ignoring response - no heartbeat received yet");
        return;
    }

    try {
        log_info("[DISCOVERY_CRON] Discovery response received on topic: %s (payload size: %zu)", topic.c_str(), payload.size());
        log_debug("[DISCOVERY_CRON] Response payload: %s", payload.c_str());

        if (payload.empty()) {
            log_warning("[DISCOVERY_CRON] Empty discovery response payload");
            return;
        }

        json response = json::parse(payload);

        // Check if this is a JSON-RPC 2.0 response from ur-mavdiscovery
        if (!response.contains("jsonrpc") || response["jsonrpc"].get<std::string>() != "2.0") {
            log_warning("[DISCOVERY_CRON] Ignoring non-JSON-RPC 2.0 response: %s", response.dump().c_str());
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
                log_warning("[DISCOVERY_CRON] Invalid transaction ID type in response");
                return;
            }
        } else {
            log_warning("[DISCOVERY_CRON] Missing transaction ID in response");
            return;
        }

        log_info("[DISCOVERY_CRON] Processing response with transaction ID: %s", transactionId.c_str());

        // Find the local transaction ID that maps to this RPC transaction ID
        std::string localTransactionId;
        {
            std::lock_guard<std::mutex> lock(responseMutex_);
            for (const auto& pair : pendingResponses_) {
                if (pair.second.contains("_rpc_transaction_id") && 
                    pair.second["_rpc_transaction_id"].get<std::string>() == transactionId &&
                    pair.second.contains("_pending") && pair.second["_pending"].get<bool>()) {
                    localTransactionId = pair.first;
                    break;
                }
            }
        }

        if (localTransactionId.empty()) {
            log_debug("[DISCOVERY_CRON] No matching local transaction ID found for RPC transaction: %s", transactionId.c_str());
            return;
        }

        log_info("[DISCOVERY_CRON] Found local transaction ID: %s for RPC transaction: %s", localTransactionId.c_str(), transactionId.c_str());
        
        // Parse the JSON-RPC 2.0 response structure
        bool success = false;
        json resultData;
        std::string errorMessage;
        
        if (response.contains("result")) {
            resultData = response["result"];
            success = true;
            log_info("[DISCOVERY_CRON] Response contains result data");
        }
        
        if (response.contains("error")) {
            if (response["error"].contains("message")) {
                errorMessage = response["error"]["message"].get<std::string>();
            }
            success = false;
            log_error("[DISCOVERY_CRON] Response contains error: %s", errorMessage.c_str());
        }
        
        // Store the structured response and notify waiting thread
        {
            std::lock_guard<std::mutex> lock(responseMutex_);
            
            // Check if we have a pending entry for this transaction
            if (pendingResponses_.find(localTransactionId) != pendingResponses_.end()) {
                // Merge the response data with the existing pending entry
                pendingResponses_[localTransactionId].update(response);
                // Add convenience fields for easier access
                pendingResponses_[localTransactionId]["_extracted_success"] = success;
                pendingResponses_[localTransactionId]["_extracted_result"] = resultData;
                pendingResponses_[localTransactionId]["_extracted_error"] = errorMessage;
                pendingResponses_[localTransactionId]["_pending"] = false; // Mark as completed
            } else {
                // No pending entry, store the response directly
                pendingResponses_[localTransactionId] = response;
                // Add convenience fields for easier access
                pendingResponses_[localTransactionId]["_extracted_success"] = success;
                pendingResponses_[localTransactionId]["_extracted_result"] = resultData;
                pendingResponses_[localTransactionId]["_extracted_error"] = errorMessage;
                pendingResponses_[localTransactionId]["_pending"] = false;
            }
        }
        responseCondition_.notify_all();
        
        log_info("[DISCOVERY_CRON] Stored discovery response for local transaction: %s (rpc: %s)", localTransactionId.c_str(), transactionId.c_str());
        log_info("[DISCOVERY_CRON] Response success: %s", success ? "true" : "false");
        log_info("[DISCOVERY_CRON] Full response payload: %s", response.dump().c_str());
        if (!errorMessage.empty()) {
            log_info("[DISCOVERY_CRON] Response message: %s", errorMessage.c_str());
        }

    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error in discovery response handler: %s", e.what());
    }
}

void DeviceDiscoveryCronJob::performDeviceDiscovery() {
    if (!running_.load()) {
        return;
    }

    // Set discovery in progress flag
    discoveryInProgress_.store(true);
    
    try {
        log_info("[DISCOVERY_CRON] Starting device discovery process");
        
        // Send device discovery request
        std::string localTransactionId = "device_discovery_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand());
        std::string rpcTransactionId = sendDiscoveryRequestWithLocalId(localTransactionId);
        if (rpcTransactionId.empty()) {
            log_error("[DISCOVERY_CRON] Failed to send device discovery request");
            discoveryInProgress_.store(false);
            return;
        }
        
        // Wait for response with 1 second timeout using local transaction ID
        if (!waitForResponse(localTransactionId, std::chrono::milliseconds(1000))) {
            log_error("[DISCOVERY_CRON] Timeout waiting for device discovery response (local transaction: %s, rpc transaction: %s)", 
                     localTransactionId.c_str(), rpcTransactionId.c_str());
            discoveryInProgress_.store(false);
            return;
        }
        
        // Process the response
        {
            std::lock_guard<std::mutex> lock(responseMutex_);
            
            if (pendingResponses_.find(localTransactionId) != pendingResponses_.end()) {
                const auto& response = pendingResponses_[localTransactionId];
                
                log_info("[DISCOVERY_CRON] Processing response for local transaction: %s", localTransactionId.c_str());
                log_info("[DISCOVERY_CRON] Response has _extracted_success: %s", 
                         response.contains("_extracted_success") ? "true" : "false");
                
                if (response.contains("_extracted_success")) {
                    log_info("[DISCOVERY_CRON] _extracted_success value: %s", 
                             response["_extracted_success"].get<bool>() ? "true" : "false");
                }
                
                if (response.contains("_extracted_success") && response["_extracted_success"].get<bool>()) {
                    json resultData = response["_extracted_result"];
                    log_info("[DISCOVERY_CRON] Processing successful discovery response");
                    processDiscoveryResponse(resultData);
                } else {
                    std::string errorMsg = response.value("_extracted_error", "Unknown error");
                    log_error("[DISCOVERY_CRON] Device discovery request failed (local transaction: %s): %s", 
                             localTransactionId.c_str(), errorMsg.c_str());
                    log_info("[DISCOVERY_CRON] Full response that caused error: %s", response.dump().c_str());
                }
            } else {
                log_error("[DISCOVERY_CRON] No response found for local transaction: %s", localTransactionId.c_str());
            }
        }
        
        log_info("[DISCOVERY_CRON] Device discovery completed successfully");
        
        // Mark first job as completed
        if (!firstJobCompleted_.exchange(true)) {
            log_info("[DISCOVERY_CRON] First device discovery job completed successfully");
            log_info("[DISCOVERY_CRON] Switching to periodic scheduling (every 5 minutes)");
        }
        
    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error during device discovery: %s", e.what());
    }
    
    // Clear discovery flag and exit
    discoveryInProgress_.store(false);
    
    // Clear pending responses
    {
        std::lock_guard<std::mutex> lock(responseMutex_);
        pendingResponses_.clear();
    }
    
    log_info("[DISCOVERY_CRON] Device discovery thread exiting");
}

std::string DeviceDiscoveryCronJob::sendDiscoveryRequestWithLocalId(const std::string& localTransactionId) {
    try {
        log_info("[DISCOVERY_CRON] Sending device discovery request with local ID: %s", localTransactionId.c_str());
        
        // Create JSON-RPC 2.0 request
        json request = json::object();
        request["jsonrpc"] = "2.0";
        request["id"] = localTransactionId;
        request["method"] = "device-list";
        
        // Parameters
        json params = json::object();
        params["include_unverified"] = false;
        params["include_usb_info"] = true;
        params["timeout_seconds"] = 1; // 1 second timeout as requested
        request["params"] = params;

        std::string requestStr = request.dump();
        
        // Use RPC callback to send request
        if (rpcRequestCallback_) {
            std::string returnedTransactionId = rpcRequestCallback_("ur-mavdiscovery", "device-list", requestStr);
            
            if (!returnedTransactionId.empty()) {
                log_info("[DISCOVERY_CRON] Sent device discovery request - local: %s, rpc: %s", 
                        localTransactionId.c_str(), returnedTransactionId.c_str());
                
                // Store mapping between local ID and RPC ID for response matching
                {
                    std::lock_guard<std::mutex> lock(responseMutex_);
                    pendingResponses_[localTransactionId]["_rpc_transaction_id"] = returnedTransactionId;
                    pendingResponses_[localTransactionId]["_pending"] = true;
                }
                
                return returnedTransactionId;
            } else {
                log_error("[DISCOVERY_CRON] Failed to send device discovery request via RPC callback");
                return "";
            }
        } else {
            log_error("[DISCOVERY_CRON] RPC request callback not set");
            return "";
        }

    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error sending device discovery request: %s", e.what());
        return "";
    }
}

std::string DeviceDiscoveryCronJob::sendDiscoveryRequest() {
    try {
        log_info("[DISCOVERY_CRON] Sending device discovery request...");

        // Generate unique transaction ID for local tracking
        std::string localTransactionId = "device_discovery_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand());
        
        // Create JSON-RPC 2.0 request
        json request = json::object();
        request["jsonrpc"] = "2.0";
        request["id"] = localTransactionId;
        request["method"] = "device-list";
        
        // Parameters
        json params = json::object();
        params["include_unverified"] = false;
        params["include_usb_info"] = true;
        params["timeout_seconds"] = 1; // 1 second timeout as requested
        request["params"] = params;

        std::string requestStr = request.dump();
        
        // Use RPC callback to send request
        if (rpcRequestCallback_) {
            std::string returnedTransactionId = rpcRequestCallback_("ur-mavdiscovery", "device-list", requestStr);
            
            if (!returnedTransactionId.empty()) {
                log_info("[DISCOVERY_CRON] Sent device discovery request with transaction: %s (local tracking: %s)", 
                        returnedTransactionId.c_str(), localTransactionId.c_str());
                
                // Store mapping between local ID and RPC ID for response matching
                {
                    std::lock_guard<std::mutex> lock(responseMutex_);
                    pendingResponses_[localTransactionId]["_rpc_transaction_id"] = returnedTransactionId;
                    pendingResponses_[localTransactionId]["_pending"] = true;
                }
                
                return returnedTransactionId;
            } else {
                log_error("[DISCOVERY_CRON] Failed to send device discovery request via RPC callback");
                return "";
            }
        } else {
            log_error("[DISCOVERY_CRON] RPC request callback not set");
            return "";
        }

    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error sending device discovery request: %s", e.what());
        return "";
    }
}

void DeviceDiscoveryCronJob::setRpcRequestCallback(std::function<std::string(const std::string&, const std::string&, const std::string&)> callback) {
    rpcRequestCallback_ = callback;
    log_info("[DISCOVERY_CRON] RPC request callback set");
}

bool DeviceDiscoveryCronJob::waitForResponse(const std::string& transactionId, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(responseMutex_);
    
    // Wait for response with timeout
    auto deadline = std::chrono::steady_clock::now() + timeout;
    
    if (responseCondition_.wait_until(lock, deadline, [&]() {
        return (pendingResponses_.find(transactionId) != pendingResponses_.end() && 
                !pendingResponses_[transactionId].value("_pending", true)) || !running_.load();
    })) {
        // Response received and processed or job stopped
        if (!running_.load()) {
            log_info("[DISCOVERY_CRON] Job stopped while waiting for response");
            return false;
        }
        
        log_info("[DISCOVERY_CRON] Response received for transaction: %s", transactionId.c_str());
        return true;
    } else {
        // Timeout occurred
        log_error("[DISCOVERY_CRON] Timeout waiting for response to transaction: %s", transactionId.c_str());
        return false;
    }
}

void DeviceDiscoveryCronJob::processDiscoveryResponse(const nlohmann::json& responseData) {
    try {
        log_info("[DISCOVERY_CRON] Processing device discovery response");
        
        if (!responseData.contains("devices") || !responseData["devices"].is_array()) {
            log_error("[DISCOVERY_CRON] Invalid response format - missing devices array");
            return;
        }
        
        auto devicesJson = responseData["devices"];
        std::vector<MavlinkShared::DeviceInfo> devices;
        
        for (const auto& deviceJson : devicesJson) {
            try {
                MavlinkShared::DeviceInfo deviceInfo;
                deviceInfo.devicePath = deviceJson.value("devicePath", "");
                deviceInfo.state = MavlinkShared::DeviceState::VERIFIED;
                deviceInfo.baudrate = deviceJson.value("baudrate", 57600);
                deviceInfo.sysid = deviceJson.value("systemId", 1);
                deviceInfo.compid = deviceJson.value("componentId", 1);
                
                // Validate required device path
                if (deviceInfo.devicePath.empty()) {
                    log_warning("[DISCOVERY_CRON] Skipping device with empty path");
                    continue;
                }
                
                devices.push_back(deviceInfo);
                log_info("[DISCOVERY_CRON] Discovered device: %s (sysid:%d, compid:%d)", 
                        deviceInfo.devicePath.c_str(), deviceInfo.sysid, deviceInfo.compid);
                
            } catch (const std::exception& e) {
                log_error("[DISCOVERY_CRON] Failed to process device entry: %s", e.what());
            }
        }
        
        if (!devices.empty()) {
            log_info("[DISCOVERY_CRON] Found %zu devices, processing...", devices.size());
            processDiscoveredDevices(devices);
        } else {
            log_info("[DISCOVERY_CRON] No devices found in discovery response");
        }
        
    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error processing discovery response: %s", e.what());
    }
}

void DeviceDiscoveryCronJob::startPeriodicScheduling() {
    try {
        if (periodicSchedulingRunning_.exchange(true)) {
            log_info("[DISCOVERY_CRON] Periodic scheduling already running");
            return;
        }
        
        log_info("[DISCOVERY_CRON] Starting periodic scheduling thread");
        periodicSchedulingThread_ = std::thread(&DeviceDiscoveryCronJob::periodicSchedulingThread, this);
        
    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error starting periodic scheduling: %s", e.what());
        periodicSchedulingRunning_.store(false);
    }
}

void DeviceDiscoveryCronJob::stopPeriodicScheduling() {
    try {
        if (!periodicSchedulingRunning_.exchange(false)) {
            return; // Already stopped
        }
        
        log_info("[DISCOVERY_CRON] Stopping periodic scheduling thread");
        periodicSchedulingCondition_.notify_all();
        
        if (periodicSchedulingThread_.joinable()) {
            periodicSchedulingThread_.join();
        }
        
        log_info("[DISCOVERY_CRON] Periodic scheduling thread stopped");
        
    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error stopping periodic scheduling: %s", e.what());
    }
}

void DeviceDiscoveryCronJob::periodicSchedulingThread() {
    log_info("[DISCOVERY_CRON] Periodic scheduling thread started");
    
    while (periodicSchedulingRunning_.load() && running_.load()) {
        // Wait for first job to complete or timeout
        std::unique_lock<std::mutex> lock(periodicSchedulingMutex_);
        
        if (firstJobCompleted_.load()) {
            // First job completed, run every 5 minutes
            if (periodicSchedulingCondition_.wait_for(lock, std::chrono::minutes(PERIODIC_INTERVAL_MINUTES), [this]() {
                return !periodicSchedulingRunning_.load() || !running_.load();
            })) {
                break; // Stop requested
            }
            
            if (periodicSchedulingRunning_.load() && running_.load()) {
                log_info("[DISCOVERY_CRON] Periodic device discovery triggered");
                
                // Run device discovery in separate thread
                threadManager_->createThread([this]() {
                    this->performDeviceDiscovery();
                });
            }
        } else {
            // Wait for first job to complete
            if (periodicSchedulingCondition_.wait_for(lock, std::chrono::seconds(30), [this]() {
                return !periodicSchedulingRunning_.load() || !running_.load() || firstJobCompleted_.load();
            })) {
                break; // Stop requested
            }
        }
    }
    
    log_info("[DISCOVERY_CRON] Periodic scheduling thread exiting");
}

void DeviceDiscoveryCronJob::processDiscoveredDevices(const std::vector<MavlinkShared::DeviceInfo>& devices) {
    try {
        // Store discovered devices
        {
            std::lock_guard<std::mutex> lock(devicesMutex_);
            discoveredDevices_ = devices;
        }
        
        // Trigger device added events for each device
        for (const auto& device : devices) {
            MavlinkShared::DeviceAddedEvent deviceEvent(device);
            // This would be handled by the main application
            log_info("[DISCOVERY_CRON] Triggering device added event for: %s", device.devicePath.c_str());
        }
        
    } catch (const std::exception& e) {
        log_error("[DISCOVERY_CRON] Error processing discovered devices: %s", e.what());
    }
}

std::vector<MavlinkShared::DeviceInfo> DeviceDiscoveryCronJob::getDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    return discoveredDevices_;
}
