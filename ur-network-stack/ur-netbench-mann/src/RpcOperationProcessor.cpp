#include "../include/RpcOperationProcessor.hpp"
#include "../include/ConfigManager.hpp"
#include "../include/RpcClient.hpp"
#include "../include/NetbenchOperationHandler.hpp"
#include "../include/OperationWorker.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <set>

// ============================================================================
// StatusBroadcaster Implementation
// ============================================================================

StatusBroadcaster::StatusBroadcaster(std::shared_ptr<RpcClient> rpc_client)
    : rpc_client_(rpc_client) {
}

void StatusBroadcaster::publishStatusUpdate(const std::string& transaction_id,
                                           const std::string& status,
                                           const json& details) {
    if (!broadcasting_enabled_.load()) {
        return;
    }
    
    publishThrottledStatusUpdate(transaction_id, status, details);
}

void StatusBroadcaster::publishThrottledStatusUpdate(const std::string& transaction_id,
                                                    const std::string& status,
                                                    const json& details) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto last_update = last_update_time_[transaction_id];
    
    // Always publish status changes
    bool is_status_change = false;
    auto last_status = last_published_status_[transaction_id];
    if (last_status != status) {
        is_status_change = true;
    }
    
    // Check if enough time has passed or if status changed
    if (is_status_change || 
        (now - last_update) >= min_update_interval_) {
        
        try {
            json status_message;
            status_message["transaction_id"] = transaction_id;
            status_message["status"] = status;
            status_message["timestamp"] = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());
            
            if (!details.empty()) {
                status_message["details"] = details;
            }
            
            // Add thread context information if available
            status_message["operation"] = "unknown"; // Default
            
            // Publish to shared bus
            std::string message_str = status_message.dump();
            if (rpc_client_) {
                rpc_client_->publishMessage(status_topic_, message_str);
            }
            
            std::cout << "[StatusBroadcaster] Published status update: " << status 
                      << " for transaction: " << transaction_id << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[StatusBroadcaster] Failed to publish status update: " << e.what() << std::endl;
        }
        
        last_update_time_[transaction_id] = now;
        last_published_status_[transaction_id] = status;
    }
}

void StatusBroadcaster::enableBroadcasting(bool enabled) {
    broadcasting_enabled_.store(enabled);
}

bool StatusBroadcaster::isBroadcastingEnabled() const {
    return broadcasting_enabled_.load();
}

// ============================================================================
// RpcResponseHandler Implementation
// ============================================================================

RpcResponseHandler::RpcResponseHandler(std::shared_ptr<RpcClient> rpc_client)
    : rpc_client_(rpc_client) {
}

void RpcResponseHandler::sendSuccessResponse(const std::string& transaction_id,
                                            const std::string& message,
                                            const json& additional_data) {
    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = transaction_id;
    response["result"] = json::object();
    response["result"]["success"] = true;
    response["result"]["message"] = message;
    response["result"]["timestamp"] = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    
    // Add additional data if provided
    for (auto& [key, value] : additional_data.items()) {
        response["result"][key] = value;
    }
    
    publishResponse(response);
}

void RpcResponseHandler::sendErrorResponse(const std::string& transaction_id,
                                         int error_code,
                                         const std::string& error_message,
                                         const json& error_data) {
    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = transaction_id;
    response["error"] = json::object();
    response["error"]["code"] = error_code;
    response["error"]["message"] = error_message;
    response["error"]["timestamp"] = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    
    // Add error data if provided
    if (!error_data.empty()) {
        response["error"]["data"] = error_data;
    }
    
    publishResponse(response);
}

void RpcResponseHandler::publishResponse(const json& response) {
    try {
        std::string response_str = response.dump();
        if (rpc_client_) {
            rpc_client_->sendResponse(response_topic_, response_str);
        }
        
        std::cout << "[RpcResponseHandler] Sent response: " << response_str << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[RpcResponseHandler] Failed to send response: " << e.what() << std::endl;
    }
}

// ============================================================================
// RpcThreadManager Implementation
// ============================================================================

RpcThreadManager::RpcThreadManager(ThreadManager* thread_manager)
    : thread_manager_(thread_manager) {
}

void RpcThreadManager::registerThread(std::shared_ptr<ThreadTrackingContext> context) {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    active_threads_[context->transaction_id] = context;
    std::cout << "[RpcThreadManager] Registered thread for transaction: " 
              << context->transaction_id << std::endl;
}

void RpcThreadManager::updateThreadStatus(const std::string& transaction_id, 
                                         ThreadTrackingContext::ThreadStatus status) {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    auto it = active_threads_.find(transaction_id);
    if (it != active_threads_.end()) {
        it->second->status.store(status);
    }
}

void RpcThreadManager::updateThreadProgress(const std::string& transaction_id, 
                                           const json& progress_data) {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    auto it = active_threads_.find(transaction_id);
    if (it != active_threads_.end()) {
        it->second->progress_data = progress_data;
    }
}

std::shared_ptr<ThreadTrackingContext> RpcThreadManager::getThreadContext(const std::string& transaction_id) {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    auto it = active_threads_.find(transaction_id);
    return (it != active_threads_.end()) ? it->second : nullptr;
}

void RpcThreadManager::cleanupThread(const std::string& transaction_id) {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    auto it = active_threads_.find(transaction_id);
    if (it != active_threads_.end()) {
        std::cout << "[RpcThreadManager] Cleaning up thread for transaction: " 
                  << transaction_id << std::endl;
        active_threads_.erase(it);
    }
}

std::vector<std::shared_ptr<ThreadTrackingContext>> RpcThreadManager::getActiveThreads() {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    std::vector<std::shared_ptr<ThreadTrackingContext>> threads;
    for (const auto& [id, context] : active_threads_) {
        threads.push_back(context);
    }
    return threads;
}

void RpcThreadManager::stopThread(const std::string& transaction_id) {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    auto it = active_threads_.find(transaction_id);
    if (it != active_threads_.end() && thread_manager_) {
        thread_manager_->stopThread(it->second->thread_id);
        it->second->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
    }
}

void RpcThreadManager::shutdown() {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    for (auto& [id, context] : active_threads_) {
        if (thread_manager_) {
            thread_manager_->stopThread(context->thread_id);
        }
        context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
    }
    active_threads_.clear();
}

// ============================================================================
// RpcConfigurationBuilder Implementation
// ============================================================================

json RpcConfigurationBuilder::buildPackageConfig(const std::string& method, const json& rpc_params) {
    json package_config;
    
    // Set the operation
    package_config["operation"] = method;
    
    switch (getOperationType(method)) {
        case OperationType::DNS:
            return buildDnsConfig(package_config, rpc_params);
        case OperationType::PING:
            return buildPingConfig(package_config, rpc_params);
        case OperationType::TRACEROUTE:
            return buildTracerouteConfig(package_config, rpc_params);
        case OperationType::IPERF:
            return buildIperfConfig(package_config, rpc_params);
        case OperationType::SERVERS_STATUS:
            return buildServersStatusConfig(package_config, rpc_params);
        default:
            throw std::invalid_argument("Unknown operation: " + method);
    }
}

RpcConfigurationBuilder::OperationType RpcConfigurationBuilder::getOperationType(const std::string& method) {
    if (method == "dns") return OperationType::DNS;
    if (method == "ping") return OperationType::PING;
    if (method == "traceroute") return OperationType::TRACEROUTE;
    if (method == "iperf") return OperationType::IPERF;
    if (method == "servers-status") return OperationType::SERVERS_STATUS;
    throw std::invalid_argument("Unknown method: " + method);
}

json RpcConfigurationBuilder::buildDnsConfig(json package_config, const json& rpc_params) {
    validateRequiredParams(rpc_params, {"hostname"}, "DNS test");
    
    json test_config = rpc_params;
    
    // Set DNS-specific defaults
    if (!test_config.contains("query_type")) test_config["query_type"] = "A";
    if (!test_config.contains("timeout_ms")) test_config["timeout_ms"] = 5000;
    if (!test_config.contains("use_tcp")) test_config["use_tcp"] = false;
    if (!test_config.contains("nameserver")) test_config["nameserver"] = "8.8.8.8";
    
    package_config["test_config"] = test_config;
    
    if (rpc_params.contains("output_file")) {
        package_config["output_file"] = rpc_params["output_file"];
    }
    
    return package_config;
}

json RpcConfigurationBuilder::buildPingConfig(json package_config, const json& rpc_params) {
    validateRequiredParams(rpc_params, {"destination"}, "Ping test");
    
    json test_config = rpc_params;
    
    // Set ping-specific defaults
    if (!test_config.contains("count")) test_config["count"] = 4;
    if (!test_config.contains("interval_ms")) test_config["interval_ms"] = 1000;
    if (!test_config.contains("timeout_ms")) test_config["timeout_ms"] = 5000;
    if (!test_config.contains("packet_size")) test_config["packet_size"] = 56;
    if (!test_config.contains("ttl")) test_config["ttl"] = 64;
    if (!test_config.contains("resolve_hostname")) test_config["resolve_hostname"] = true;
    
    package_config["test_config"] = test_config;
    
    if (rpc_params.contains("output_file")) {
        package_config["output_file"] = rpc_params["output_file"];
    }
    
    return package_config;
}

json RpcConfigurationBuilder::buildTracerouteConfig(json package_config, const json& rpc_params) {
    validateRequiredParams(rpc_params, {"target"}, "Traceroute test");
    
    json test_config = rpc_params;
    
    // Set traceroute-specific defaults
    if (!test_config.contains("max_hops")) test_config["max_hops"] = 30;
    if (!test_config.contains("timeout_ms")) test_config["timeout_ms"] = 3000;
    if (!test_config.contains("queries_per_hop")) test_config["queries_per_hop"] = 3;
    if (!test_config.contains("packet_size")) test_config["packet_size"] = 60;
    if (!test_config.contains("port")) test_config["port"] = 33434;
    if (!test_config.contains("resolve_hostnames")) test_config["resolve_hostnames"] = true;
    
    package_config["test_config"] = test_config;
    
    if (rpc_params.contains("output_file")) {
        package_config["output_file"] = rpc_params["output_file"];
    }
    
    return package_config;
}

json RpcConfigurationBuilder::buildIperfConfig(json package_config, const json& rpc_params) {
    validateRequiredParams(rpc_params, {"target"}, "Iperf test");
    
    json test_config = rpc_params;
    
    // Set iperf-specific defaults
    if (!test_config.contains("port")) test_config["port"] = 5201;
    if (!test_config.contains("duration")) test_config["duration"] = 10;
    if (!test_config.contains("protocol")) test_config["protocol"] = "tcp";
    if (!test_config.contains("parallel")) test_config["parallel"] = 1;
    if (!test_config.contains("realtime")) test_config["realtime"] = true;
    
    package_config["test_config"] = test_config;
    
    if (rpc_params.contains("output_file")) {
        package_config["output_file"] = rpc_params["output_file"];
    }
    
    if (rpc_params.contains("servers_list_path")) {
        package_config["servers_list_path"] = rpc_params["servers_list_path"];
    }
    
    return package_config;
}

json RpcConfigurationBuilder::buildServersStatusConfig(json package_config, const json& rpc_params) {
    validateRequiredParams(rpc_params, {"servers_list_path"}, "Servers status");
    
    // Direct mapping for servers-status operation
    for (auto& [key, value] : rpc_params.items()) {
        package_config[key] = value;
    }
    
    // Set defaults
    if (!package_config.contains("output_dir")) {
        package_config["output_dir"] = "runtime-data/server-status";
    }
    
    return package_config;
}

void RpcConfigurationBuilder::validateRequiredParams(const json& params, 
                                                   const std::vector<std::string>& required,
                                                   const std::string& operation_name) {
    for (const auto& param : required) {
        if (!params.contains(param)) {
            throw std::invalid_argument(operation_name + " requires parameter: " + param);
        }
    }
}

// ============================================================================
// RpcOperationProcessor Implementation
// ============================================================================

RpcOperationProcessor::RpcOperationProcessor(const ConfigManager& configManager, bool verbose)
    : configManager_(configManager), verbose_(verbose) {
    
    // Initialize internal thread manager
    internalThreadManager_ = std::make_shared<ThreadManager>(50);
    
    // Initialize enhanced components
    thread_manager_ = std::make_unique<RpcThreadManager>(internalThreadManager_.get());
    
    logInfo("RpcOperationProcessor initialized with enhanced architecture");
}

RpcOperationProcessor::~RpcOperationProcessor() {
    shutdown();
    logInfo("RpcOperationProcessor destroyed");
}

void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    if (shutdown_requested_.load()) {
        if (response_handler_) {
            response_handler_->sendErrorResponse("unknown", -32003, 
                "Server is shutting down");
        }
        return;
    }
    
    // Input validation
    if (!payload || payload_len == 0) {
        logError("Empty payload received");
        return;
    }
    
    // Size validation
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        logError("Payload too large: " + std::to_string(payload_len) + " bytes");
        return;
    }
    
    try {
        // Parse JSON-RPC request
        json request = json::parse(payload, payload + payload_len);
        
        // Validate JSON-RPC structure
        if (!validateJsonRpcRequest(request)) {
            if (response_handler_) {
                response_handler_->sendErrorResponse("unknown", -32600, 
                    "Invalid JSON-RPC request");
            }
            return;
        }
        
        std::string method = request["method"];
        std::string transaction_id = extractTransactionId(request);
        json params = request["params"];
        
        // Validate operation method
        if (!isValidOperation(method)) {
            if (response_handler_) {
                response_handler_->sendErrorResponse(transaction_id, -32601, 
                    "Method not found: " + method);
            }
            return;
        }
        
        // Build and validate configuration
        json package_config = RpcConfigurationBuilder::buildPackageConfig(method, params);
        
        // Process operation
        processValidatedOperation(method, transaction_id, package_config);
        
    } catch (const json::parse_error& e) {
        if (response_handler_) {
            response_handler_->sendErrorResponse("unknown", -32700, 
                "Parse error: " + std::string(e.what()));
        }
        logError("JSON parse error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        if (response_handler_) {
            response_handler_->sendErrorResponse("unknown", -32603, 
                "Internal error: " + std::string(e.what()));
        }
        logError("Internal error: " + std::string(e.what()));
    }
}

void RpcOperationProcessor::processValidatedOperation(const std::string& method,
                                                     const std::string& transaction_id,
                                                     const json& package_config) {
    try {
        // Create temporary config file
        std::string temp_config_file = createTempConfigFile(package_config);
        
        // Create config manager
        auto config_manager = std::make_shared<ConfigManager>();
        if (!config_manager->loadPackageConfig(temp_config_file)) {
            if (response_handler_) {
                response_handler_->sendErrorResponse(transaction_id, -32000, 
                    "Failed to load configuration");
            }
            return;
        }
        
        // Create thread tracking context
        auto context = std::make_shared<ThreadTrackingContext>();
        context->transaction_id = transaction_id;
        context->method = method;
        context->config_manager = config_manager;
        context->config_file = temp_config_file;
        context->start_time = std::chrono::system_clock::now();
        
        // Create and register thread
        unsigned int thread_id = internalThreadManager_->createThread(
            [this, context]() {
                this->executeOperationThread(context);
            }
        );
        
        context->thread_id = thread_id;
        thread_manager_->registerThread(context);
        
        // Send immediate success response
        if (response_handler_) {
            response_handler_->sendSuccessResponse(transaction_id, 
                "Test thread launched successfully", {
                    {"thread_id", thread_id},
                    {"operation", method},
                    {"status", "running"}
                });
        }
        
        // Publish initial status update
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(transaction_id, "running", {
                {"thread_id", thread_id},
                {"operation", method},
                {"start_time", std::chrono::system_clock::to_time_t(context->start_time)}
            });
        }
        
        logInfo("Launched operation thread for " + method + " with transaction ID: " + transaction_id);
        
    } catch (const std::exception& e) {
        logError("Failed to launch test: " + std::string(e.what()));
        if (response_handler_) {
            response_handler_->sendErrorResponse(transaction_id, -32603, 
                "Failed to launch test: " + std::string(e.what()));
        }
    }
}

void RpcOperationProcessor::executeOperationThread(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        // Update status to running
        context->status.store(ThreadTrackingContext::ThreadStatus::RUNNING);
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"message", "Test execution started"}
            });
        }
        
        // Route to appropriate operation handler
        if (context->method == "dns") {
            executeDnsOperation(context);
        } else if (context->method == "ping") {
            executePingOperation(context);
        } else if (context->method == "traceroute") {
            executeTracerouteOperation(context);
        } else if (context->method == "iperf") {
            executeIperfOperation(context);
        } else if (context->method == "servers-status") {
            executeServersStatusOperation(context);
        } else {
            throw std::runtime_error("Unknown operation: " + context->method);
        }
        
        // Update final status
        if (context->status.load() == ThreadTrackingContext::ThreadStatus::RUNNING) {
            context->status.store(ThreadTrackingContext::ThreadStatus::FINISHED);
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "finished", {
                    {"phase", "completed"},
                    {"message", "Test completed successfully"}
                });
            }
        }
        
    } catch (const std::exception& e) {
        context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
        context->error_message = e.what();
        
        // Publish failure status
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "failed", {
                {"phase", "error"},
                {"message", "Test failed: " + std::string(e.what())}
            });
        }
        
        logError("Operation failed for transaction " + context->transaction_id + ": " + e.what());
    }
    
    // Cleanup
    if (thread_manager_) {
        thread_manager_->cleanupThread(context->transaction_id);
    }
}

void RpcOperationProcessor::executeDnsOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "initialization"},
                {"message", "Starting DNS test"}
            });
        }
        
        // Extract configuration
        json test_config = context->config_manager->getTestConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Create DNS test worker thread
        unsigned int dns_thread_id = internalThreadManager_->createThread(
            [](ThreadManager& tm, const std::string& config_file) {
                operation_worker(tm, config_file);
            },
            std::ref(*internalThreadManager_),
            context->config_file
        );
        
        context->worker_thread_id = dns_thread_id;
        
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"worker_thread_id", dns_thread_id},
                {"message", "DNS test worker started"}
            });
        }
        
        // Wait for completion with timeout
        bool completed = internalThreadManager_->joinThread(dns_thread_id, std::chrono::minutes(5));
        
        if (completed) {
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "finished", {
                    {"phase", "completed"},
                    {"message", "DNS test completed successfully"}
                });
            }
        } else {
            context->status.store(ThreadTrackingContext::ThreadStatus::TIMEOUT);
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "failed", {
                    {"phase", "timeout"},
                    {"message", "DNS test timed out"}
                });
            }
            
            // Force stop the thread
            internalThreadManager_->stopThread(dns_thread_id);
        }
        
    } catch (const std::exception& e) {
        context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
        throw;
    }
}

void RpcOperationProcessor::executePingOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "initialization"},
                {"message", "Starting ping test"}
            });
        }
        
        json test_config = context->config_manager->getTestConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Create ping worker thread
        unsigned int ping_thread_id = internalThreadManager_->createThread(
            [](ThreadManager& tm, const std::string& config_file) {
                operation_worker(tm, config_file);
            },
            std::ref(*internalThreadManager_),
            context->config_file
        );
        
        context->worker_thread_id = ping_thread_id;
        
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"worker_thread_id", ping_thread_id},
                {"message", "Ping test worker started"}
            });
        }
        
        // Monitor ping progress
        while (internalThreadManager_->isThreadAlive(ping_thread_id)) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Could publish intermediate statistics here
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                    {"phase", "execution"},
                    {"message", "Ping test in progress"}
                });
            }
        }
        
        // Final status update
        bool completed = internalThreadManager_->joinThread(ping_thread_id, std::chrono::seconds(1));
        
        if (completed) {
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "finished", {
                    {"phase", "completed"},
                    {"message", "Ping test completed successfully"}
                });
            }
        } else {
            context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "failed", {
                    {"phase", "error"},
                    {"message", "Ping test failed to complete properly"}
                });
            }
        }
        
    } catch (const std::exception& e) {
        context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
        throw;
    }
}

void RpcOperationProcessor::executeTracerouteOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "initialization"},
                {"message", "Starting traceroute test"}
            });
        }
        
        json test_config = context->config_manager->getTestConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Create traceroute worker thread
        unsigned int traceroute_thread_id = internalThreadManager_->createThread(
            [](ThreadManager& tm, const std::string& config_file) {
                operation_worker(tm, config_file);
            },
            std::ref(*internalThreadManager_),
            context->config_file
        );
        
        context->worker_thread_id = traceroute_thread_id;
        
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"worker_thread_id", traceroute_thread_id},
                {"message", "Traceroute test worker started"}
            });
        }
        
        // Wait for completion
        bool completed = internalThreadManager_->joinThread(traceroute_thread_id, std::chrono::minutes(10));
        
        if (completed) {
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "finished", {
                    {"phase", "completed"},
                    {"message", "Traceroute test completed successfully"}
                });
            }
        } else {
            context->status.store(ThreadTrackingContext::ThreadStatus::TIMEOUT);
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "failed", {
                    {"phase", "timeout"},
                    {"message", "Traceroute test timed out"}
                });
            }
        }
        
    } catch (const std::exception& e) {
        context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
        throw;
    }
}

void RpcOperationProcessor::executeIperfOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "initialization"},
                {"message", "Starting iperf test"}
            });
        }
        
        json test_config = context->config_manager->getTestConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Create iperf worker thread
        unsigned int iperf_thread_id = internalThreadManager_->createThread(
            [](ThreadManager& tm, const std::string& config_file) {
                operation_worker(tm, config_file);
            },
            std::ref(*internalThreadManager_),
            context->config_file
        );
        
        context->worker_thread_id = iperf_thread_id;
        
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"worker_thread_id", iperf_thread_id},
                {"message", "Iperf test worker started"}
            });
        }
        
        // Monitor progress with periodic updates
        auto start_time = std::chrono::steady_clock::now();
        while (internalThreadManager_->isThreadAlive(iperf_thread_id)) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                    {"phase", "execution"},
                    {"elapsed_seconds", elapsed},
                    {"message", "Iperf test running..."}
                });
            }
        }
        
        // Final status
        bool completed = internalThreadManager_->joinThread(iperf_thread_id, std::chrono::seconds(5));
        
        if (completed) {
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "finished", {
                    {"phase", "completed"},
                    {"message", "Iperf test completed successfully"}
                });
            }
        } else {
            context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "failed", {
                    {"phase", "error"},
                    {"message", "Iperf test failed"}
                });
            }
        }
        
    } catch (const std::exception& e) {
        context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
        throw;
    }
}

void RpcOperationProcessor::executeServersStatusOperation(std::shared_ptr<ThreadTrackingContext> context) {
    try {
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "initialization"},
                {"message", "Starting servers status monitoring"}
            });
        }
        
        // Extract configuration
        std::string servers_list_path = context->config_manager->getServersListPath();
        std::string output_dir = context->config_manager->getOutputDir();
        
        // Create operation config for servers-status
        json operation_config = context->config_manager->getPackageConfig();
        std::string output_file = context->config_manager->getOutputFile();
        
        // Create servers status worker thread
        unsigned int servers_thread_id = internalThreadManager_->createThread(
            [](ThreadManager& tm, const std::string& config_file) {
                operation_worker(tm, config_file);
            },
            std::ref(*internalThreadManager_),
            context->config_file
        );
        
        context->worker_thread_id = servers_thread_id;
        
        if (status_broadcaster_) {
            status_broadcaster_->publishStatusUpdate(context->transaction_id, "running", {
                {"phase", "execution"},
                {"worker_thread_id", servers_thread_id},
                {"message", "Server monitoring started"}
            });
        }
        
        // Wait for monitoring completion or timeout
        bool completed = internalThreadManager_->joinThread(servers_thread_id, std::chrono::minutes(30));
        
        if (completed) {
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "finished", {
                    {"phase", "completed"},
                    {"message", "Server status monitoring completed successfully"}
                });
            }
        } else {
            context->status.store(ThreadTrackingContext::ThreadStatus::TIMEOUT);
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, "failed", {
                    {"phase", "timeout"},
                    {"message", "Server status monitoring timed out"}
                });
            }
        }
        
    } catch (const std::exception& e) {
        context->status.store(ThreadTrackingContext::ThreadStatus::FAILED);
        throw;
    }
}

void RpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
}

void RpcOperationProcessor::setRpcClient(std::shared_ptr<RpcClient> rpcClient) {
    rpcClient_ = rpcClient;
    
    // Initialize enhanced components with RPC client
    if (rpcClient_) {
        status_broadcaster_ = std::make_unique<StatusBroadcaster>(rpcClient_);
        response_handler_ = std::make_unique<RpcResponseHandler>(rpcClient_);
        
        // Start status monitoring thread
        status_monitoring_thread_ = std::thread([this]() {
            this->statusMonitoringLoop();
        });
        
        logInfo("Enhanced RPC components initialized");
    }
}

size_t RpcOperationProcessor::getActiveThreadsCount() const {
    if (thread_manager_) {
        return thread_manager_->getActiveThreads().size();
    }
    return 0;
}

bool RpcOperationProcessor::isRunning() const {
    return !shutdown_requested_.load();
}

void RpcOperationProcessor::shutdown() {
    if (shutdown_requested_.load()) {
        return;
    }
    
    shutdown_requested_.store(true);
    logInfo("Shutting down RpcOperationProcessor...");
    
    // Stop all active operations
    if (thread_manager_) {
        auto active_threads = thread_manager_->getActiveThreads();
        for (const auto& context : active_threads) {
            thread_manager_->stopThread(context->transaction_id);
            if (status_broadcaster_) {
                status_broadcaster_->publishStatusUpdate(context->transaction_id, 
                    "failed", {{"message", "Server shutdown"}});
            }
        }
        thread_manager_->shutdown();
    }
    
    // Stop status monitoring thread
    if (status_monitoring_thread_.joinable()) {
        status_monitoring_thread_.join();
    }
    
    // Stop internal thread manager
    if (internalThreadManager_) {
        // ThreadManager doesn't have shutdown method, just let it be destroyed
        internalThreadManager_.reset();
    }
    
    logInfo("RpcOperationProcessor shutdown complete");
}

void RpcOperationProcessor::statusMonitoringLoop() {
    while (!shutdown_requested_.load()) {
        try {
            // Monitor active operations and publish periodic updates
            if (thread_manager_) {
                auto active_threads = thread_manager_->getActiveThreads();
                
                for (const auto& context : active_threads) {
                    if (context->status.load() == ThreadTrackingContext::ThreadStatus::RUNNING) {
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now() - context->start_time);
                        
                        // Publish periodic update for long-running operations
                        if (elapsed.count() % 30 == 0) { // Every 30 seconds
                            if (status_broadcaster_) {
                                status_broadcaster_->publishStatusUpdate(
                                    context->transaction_id, "running", {
                                        {"elapsed_seconds", elapsed.count()},
                                        {"message", "Operation still running"}
                                    });
                            }
                        }
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            logError("Status monitoring error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

bool RpcOperationProcessor::validateJsonRpcRequest(const json& request) {
    return request.contains("jsonrpc") &&
           request["jsonrpc"].get<std::string>() == "2.0" &&
           request.contains("method") &&
           request.contains("params") &&
           request.contains("id");
}

std::string RpcOperationProcessor::extractTransactionId(const json& request) {
    if (request["id"].is_string()) {
        return request["id"];
    } else if (request["id"].is_number()) {
        return std::to_string(request["id"].get<int>());
    }
    return "unknown";
}

bool RpcOperationProcessor::isValidOperation(const std::string& method) {
    static std::set<std::string> valid_methods = {
        "dns", "ping", "traceroute", "iperf", "servers-status"
    };
    return valid_methods.find(method) != valid_methods.end();
}

std::string RpcOperationProcessor::createTempConfigFile(const json& config) {
    std::string temp_file = "/tmp/rpc_config_" + 
        std::to_string(std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now())) + ".json";
    
    std::ofstream file(temp_file);
    file << config.dump(2);
    file.close();
    
    return temp_file;
}

void RpcOperationProcessor::logInfo(const std::string& message) const {
    if (verbose_) {
        std::cout << "[RpcOperationProcessor] " << message << std::endl;
    }
}

void RpcOperationProcessor::logError(const std::string& message) const {
    std::cerr << "[RpcOperationProcessor] ERROR: " << message << std::endl;
}
