#include "rpc_operation_processor.hpp"
#include "qmi_scanner.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <stdexcept>
#include <chrono>

using json = nlohmann::json;

extern "C" {
#include "direct_template.h"
}

RpcOperationProcessor::RpcOperationProcessor(bool verbose)
    : verbose_(verbose)
    , responseTopic_("direct_messaging/ur-qmi-ident/responses")
    , threadManagerInitialized_(false) {
    
    // Initialize thread manager using C API
    if (thread_manager_init(&threadManager_, 100) != 0) {
        throw std::runtime_error("Failed to initialize thread manager");
    }
    threadManagerInitialized_ = true;
    
    if (verbose_) {
        std::cout << "[RPC Processor] Initialized with thread pool size: 100" << std::endl;
    }
}

RpcOperationProcessor::~RpcOperationProcessor() {
    std::cout << "[RPC Processor] Destructor called - setting shutdown flag" << std::endl;
    
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
            if (thread_is_alive(&threadManager_, threadId)) {
                if (verbose_) {
                    std::cout << "[RPC Processor] Waiting for thread " << threadId << " to complete..." << std::endl;
                }
                
                void* result = nullptr;
                thread_join(&threadManager_, threadId, &result);
                
                if (verbose_) {
                    std::cout << "[RPC Processor] Thread " << threadId << " completed successfully" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            if (verbose_) {
                std::cerr << "[RPC Processor] Error joining thread " << threadId << ": " << e.what() << std::endl;
            }
        }
    }
    
    if (threadManagerInitialized_) {
        thread_manager_destroy(&threadManager_);
    }
    
    if (verbose_) {
        std::cout << "[RPC Processor] All threads joined, cleanup complete" << std::endl;
    }
}

void RpcOperationProcessor::setScanner(QMIScanner* scanner) {
    scanner_ = scanner;
    if (verbose_) {
        std::cout << "[RPC Processor] Scanner instance set" << std::endl;
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

        if (verbose_) {
            std::cout << "[RPC Processor] Processing request with ID: " 
                      << transactionId << ", method: " << method << std::endl;
        }

        // Check if we're shutting down
        bool shuttingDown = isShuttingDown_.load();
        if (shuttingDown) {
            std::cerr << "[RPC Processor] Cannot create thread - processor is shutting down" << std::endl;
            sendResponse(transactionId, false, "", "Server is shutting down");
            return;
        }
        
        try {
            // Create context with shared_ptr
            auto context = std::make_shared<RequestContext>(
                requestJson,
                transactionId,
                responseTopic_,
                verbose_,
                &threadManager_,
                &activeThreads_,
                &threadsMutex_,
                scanner_
            );
            
            // Create thread with context using C API
            unsigned int threadId = 0;
            auto threadFunc = [](void* arg) -> void* {
                auto ctx = static_cast<std::shared_ptr<RequestContext>*>(arg);
                RpcOperationProcessor::processOperationThreadStatic(*ctx);
                delete ctx;
                return nullptr;
            };
            
            // Create a copy of context on heap for thread
            auto* contextPtr = new std::shared_ptr<RequestContext>(context);
            
            if (thread_create(&threadManager_, threadFunc, contextPtr, &threadId) != 0) {
                delete contextPtr;
                throw std::runtime_error("Failed to create thread");
            }
            
            // Register thread in activeThreads_
            {
                std::lock_guard<std::mutex> lock(threadsMutex_);
                activeThreads_.insert(threadId);
            }
            
            // Set thread ID in atomic field
            context->threadId.store(threadId);
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
            sendResponse(transactionId, false, "", std::string("Processing failed: ") + e.what());
            return;
        }

    } catch (const std::exception& e) {
        std::cerr << "[RPC Processor] Exception processing request: " << e.what() << std::endl;
    }
}

void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    const std::string& requestJson = context->requestJson;
    const std::string& transactionId = context->transactionId;
    const std::string& responseTopic = context->responseTopic;
    bool verbose = context->verbose;
    QMIScanner* scanner = context->scanner;
    
    // Wait for threadId to be published
    unsigned int threadId = context->threadIdFuture.get();
    
    // Extract cleanup info
    std::set<unsigned int>* activeThreads = context->activeThreads;
    std::mutex* threadsMutex = context->threadsMutex;
    
    if (verbose) {
        std::cerr << "[RPC Thread " << threadId << "/" << transactionId << "] Thread started, processing request" << std::endl;
    }

    try {
        // Parse request again in thread context
        json root = json::parse(requestJson);
        
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

        std::string result;
        bool success = true;
        
        // Process QMI-specific operations
        if (method == "get_devices") {
            if (scanner) {
                std::vector<QMIDevice> devices = scanner->getCurrentDevices();
                json devicesJson = json::array();
                for (const auto& device : devices) {
                    json deviceJson;
                    deviceJson["device_path"] = device.device_path;
                    deviceJson["imei"] = device.imei;
                    deviceJson["model"] = device.model;
                    deviceJson["manufacturer"] = device.manufacturer;
                    deviceJson["firmware_version"] = device.firmware_version;
                    deviceJson["is_available"] = device.is_available;
                    deviceJson["action"] = device.action;
                    
                    // Convert SIMStatus to JSON
                    json simStatusJson;
                    simStatusJson["card_state"] = device.sim_status.card_state;
                    simStatusJson["upin_state"] = device.sim_status.upin_state;
                    simStatusJson["upin_retries"] = device.sim_status.upin_retries;
                    simStatusJson["upuk_retries"] = device.sim_status.upuk_retries;
                    simStatusJson["application_type"] = device.sim_status.application_type;
                    simStatusJson["application_state"] = device.sim_status.application_state;
                    simStatusJson["application_id"] = device.sim_status.application_id;
                    simStatusJson["personalization_state"] = device.sim_status.personalization_state;
                    simStatusJson["upin_replaces_pin1"] = device.sim_status.upin_replaces_pin1;
                    simStatusJson["pin1_state"] = device.sim_status.pin1_state;
                    simStatusJson["pin1_retries"] = device.sim_status.pin1_retries;
                    simStatusJson["puk1_retries"] = device.sim_status.puk1_retries;
                    simStatusJson["pin2_state"] = device.sim_status.pin2_state;
                    simStatusJson["pin2_retries"] = device.sim_status.pin2_retries;
                    simStatusJson["puk2_retries"] = device.sim_status.puk2_retries;
                    deviceJson["sim_status"] = simStatusJson;
                    
                    devicesJson.push_back(deviceJson);
                }
                result = devicesJson.dump();
            } else {
                success = false;
                result = "Scanner not available";
            }
        } else if (method == "list_devices") {
            // Enhanced list_devices method that works with all scanner modes
            if (scanner) {
                json devicesJson = json::array();
                
                try {
                    // Get current scanner mode to determine which device list to return
                    // Try to get devices from all available modes with proper initialization checks
                    
                    // Try to get advanced device profiles first (most comprehensive)
                    try {
                        std::vector<AdvancedDeviceProfile> advancedProfiles = scanner->getCurrentAdvancedProfiles();
                        if (!advancedProfiles.empty()) {
                            for (const auto& profile : advancedProfiles) {
                                json deviceJson;
                                
                                // Basic profile data with validation
                                deviceJson["path"] = profile.basic.path.empty() ? "unknown" : profile.basic.path;
                                deviceJson["imei"] = profile.basic.imei.empty() ? "unknown" : profile.basic.imei;
                                deviceJson["model"] = profile.basic.model.empty() ? "unknown" : profile.basic.model;
                                deviceJson["firmware"] = profile.basic.firmware.empty() ? "unknown" : profile.basic.firmware;
                                deviceJson["bands"] = profile.basic.bands;
                                deviceJson["sim_present"] = profile.basic.sim_present;
                                deviceJson["pin_locked"] = profile.basic.pin_locked;
                                deviceJson["gps_supported"] = profile.basic.gps_supported;
                                deviceJson["max_carriers"] = profile.basic.max_carriers;
                                
                                // Advanced profile data with validation
                                deviceJson["manufacturer"] = profile.manufacturer.empty() ? "unknown" : profile.manufacturer;
                                deviceJson["msisdn"] = profile.msisdn.empty() ? "unknown" : profile.msisdn;
                                deviceJson["power_state"] = profile.power_state.empty() ? "unknown" : profile.power_state;
                                deviceJson["hardware_revision"] = profile.hardware_revision.empty() ? "unknown" : profile.hardware_revision;
                                deviceJson["operating_mode"] = profile.operating_mode.empty() ? "unknown" : profile.operating_mode;
                                deviceJson["prl_version"] = profile.prl_version.empty() ? "unknown" : profile.prl_version;
                                deviceJson["activation_state"] = profile.activation_state.empty() ? "unknown" : profile.activation_state;
                                deviceJson["user_lock_state"] = profile.user_lock_state.empty() ? "unknown" : profile.user_lock_state;
                                deviceJson["band_capabilities"] = profile.band_capabilities.empty() ? "unknown" : profile.band_capabilities;
                                deviceJson["factory_sku"] = profile.factory_sku.empty() ? "unknown" : profile.factory_sku;
                                deviceJson["software_version"] = profile.software_version.empty() ? "unknown" : profile.software_version;
                                deviceJson["iccid"] = profile.iccid.empty() ? "unknown" : profile.iccid;
                                deviceJson["imsi"] = profile.imsi.empty() ? "unknown" : profile.imsi;
                                deviceJson["uim_state"] = profile.uim_state.empty() ? "unknown" : profile.uim_state;
                                deviceJson["pin_status"] = profile.pin_status.empty() ? "unknown" : profile.pin_status;
                                deviceJson["time"] = profile.time.empty() ? "unknown" : profile.time;
                                deviceJson["stored_images"] = profile.stored_images;
                                deviceJson["firmware_preference"] = profile.firmware_preference.empty() ? "unknown" : profile.firmware_preference;
                                deviceJson["boot_image_download_mode"] = profile.boot_image_download_mode.empty() ? "unknown" : profile.boot_image_download_mode;
                                deviceJson["usb_composition"] = profile.usb_composition.empty() ? "unknown" : profile.usb_composition;
                                deviceJson["mac_address_wlan"] = profile.mac_address_wlan.empty() ? "unknown" : profile.mac_address_wlan;
                                deviceJson["mac_address_bt"] = profile.mac_address_bt.empty() ? "unknown" : profile.mac_address_bt;
                                
                                deviceJson["profile_type"] = "advanced";
                                devicesJson.push_back(deviceJson);
                            }
                        } else {
                            // Fall back to basic device profiles if advanced is empty
                            std::vector<DeviceProfile> basicProfiles = scanner->getCurrentProfiles();
                            for (const auto& profile : basicProfiles) {
                                json deviceJson;
                                deviceJson["path"] = profile.path.empty() ? "unknown" : profile.path;
                                deviceJson["imei"] = profile.imei.empty() ? "unknown" : profile.imei;
                                deviceJson["model"] = profile.model.empty() ? "unknown" : profile.model;
                                deviceJson["firmware"] = profile.firmware.empty() ? "unknown" : profile.firmware;
                                deviceJson["bands"] = profile.bands;
                                deviceJson["sim_present"] = profile.sim_present;
                                deviceJson["pin_locked"] = profile.pin_locked;
                                deviceJson["gps_supported"] = profile.gps_supported;
                                deviceJson["max_carriers"] = profile.max_carriers;
                                deviceJson["profile_type"] = "basic";
                                devicesJson.push_back(deviceJson);
                            }
                        }
                    } catch (const std::exception& e) {
                        // If advanced profiles fail, try basic profiles
                        try {
                            std::vector<DeviceProfile> basicProfiles = scanner->getCurrentProfiles();
                            for (const auto& profile : basicProfiles) {
                                json deviceJson;
                                deviceJson["path"] = profile.path.empty() ? "unknown" : profile.path;
                                deviceJson["imei"] = profile.imei.empty() ? "unknown" : profile.imei;
                                deviceJson["model"] = profile.model.empty() ? "unknown" : profile.model;
                                deviceJson["firmware"] = profile.firmware.empty() ? "unknown" : profile.firmware;
                                deviceJson["bands"] = profile.bands;
                                deviceJson["sim_present"] = profile.sim_present;
                                deviceJson["pin_locked"] = profile.pin_locked;
                                deviceJson["gps_supported"] = profile.gps_supported;
                                deviceJson["max_carriers"] = profile.max_carriers;
                                deviceJson["profile_type"] = "basic";
                                devicesJson.push_back(deviceJson);
                            }
                        } catch (const std::exception& e2) {
                            success = false;
                            result = "Failed to retrieve device list: " + std::string(e2.what());
                        }
                    }
                    
                    if (success) {
                        // Add metadata about the scan
                        json response;
                        response["devices"] = devicesJson;
                        response["count"] = devicesJson.size();
                        response["timestamp"] = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                        result = response.dump();
                    }
                    
                } catch (const std::exception& e) {
                    success = false;
                    result = "Exception in list_devices: " + std::string(e.what());
                }
            } else {
                success = false;
                result = "Scanner not available";
            }
        } else if (method == "get_device_count") {
            if (scanner) {
                std::vector<QMIDevice> devices = scanner->getCurrentDevices();
                result = std::to_string(devices.size());
            } else {
                success = false;
                result = "Scanner not available";
            }
        } else if (method == "scan_devices") {
            if (scanner) {
                // Trigger a rescan
                scanner->startMonitoring();
                result = "Device scan initiated";
            } else {
                success = false;
                result = "Scanner not available";
            }
        } else if (method == "get_device_details") {
            if (scanner && paramsObj.contains("device_path")) {
                std::string devicePath = paramsObj["device_path"];
                std::vector<QMIDevice> devices = scanner->getCurrentDevices();
                for (const auto& device : devices) {
                    if (device.device_path == devicePath) {
                        json deviceJson;
                        deviceJson["device_path"] = device.device_path;
                        deviceJson["imei"] = device.imei;
                        deviceJson["model"] = device.model;
                        deviceJson["manufacturer"] = device.manufacturer;
                        deviceJson["firmware_version"] = device.firmware_version;
                        deviceJson["is_available"] = device.is_available;
                        deviceJson["action"] = device.action;
                        
                        // Convert SIMStatus to JSON
                        json simStatusJson;
                        simStatusJson["card_state"] = device.sim_status.card_state;
                        simStatusJson["upin_state"] = device.sim_status.upin_state;
                        simStatusJson["upin_retries"] = device.sim_status.upin_retries;
                        simStatusJson["upuk_retries"] = device.sim_status.upuk_retries;
                        simStatusJson["application_type"] = device.sim_status.application_type;
                        simStatusJson["application_state"] = device.sim_status.application_state;
                        simStatusJson["application_id"] = device.sim_status.application_id;
                        simStatusJson["pin1_state"] = device.sim_status.pin1_state;
                        simStatusJson["pin1_retries"] = device.sim_status.pin1_retries;
                        simStatusJson["puk1_retries"] = device.sim_status.puk1_retries;
                        simStatusJson["pin2_state"] = device.sim_status.pin2_state;
                        simStatusJson["pin2_retries"] = device.sim_status.puk2_retries;
                        simStatusJson["puk2_retries"] = device.sim_status.puk2_retries;
                        deviceJson["sim_status"] = simStatusJson;
                        
                        result = deviceJson.dump();
                        break;
                    }
                }
                if (result.empty()) {
                    success = false;
                    result = "Device not found: " + devicePath;
                }
            } else {
                success = false;
                result = scanner ? "Missing device_path parameter" : "Scanner not available";
            }
        } else {
            success = false;
            result = "Unknown operation: " + method;
        }

        // Send response
        sendResponseStatic(transactionId, success, result, "", responseTopic);
        
        if (verbose) {
            std::cerr << "[RPC Thread " << transactionId << "] Operation completed, response sent" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[RPC Thread " << transactionId << "] Exception in thread: " << e.what() << std::endl;
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
            // If result is JSON string, parse it and include as structured data
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
            json errorObj;
            errorObj["code"] = -1;
            errorObj["message"] = error;
            response["error"] = errorObj;
        }

        std::string responseJson = response.dump();

        // Publish response
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
    if (!threadManagerInitialized_) {
        return;
    }
    
    std::vector<unsigned int> threadsToClean;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        // Find completed threads
        for (unsigned int threadId : activeThreads_) {
            if (!thread_is_alive(&threadManager_, threadId)) {
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
