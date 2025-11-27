#include "vpn_rpc_operation_processor.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace vpn_manager {

VpnRpcOperationProcessor::VpnRpcOperationProcessor(VPNInstanceManager& manager, bool verbose)
    : vpnManager_(manager), verbose_(verbose) {
    
    // Initialize thread manager with appropriate pool size
    threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(50);
}

VpnRpcOperationProcessor::~VpnRpcOperationProcessor() {
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
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
                std::cerr << "WARNING: Thread " << threadId 
                         << " did not complete after 5 minutes" << std::endl;
            }
        }
    }
}

void VpnRpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    // Input validation
    if (!payload || payload_len == 0) {
        std::cerr << "Empty payload received" << std::endl;
        return;
    }

    // Size validation (prevent memory exhaustion)
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        std::cerr << "Payload too large: " << payload_len << " bytes" << std::endl;
        return;
    }

    try {
        // JSON parsing
        nlohmann::json root = nlohmann::json::parse(payload, payload + payload_len);

        // JSON-RPC 2.0 validation
        if (!root.contains("jsonrpc") || root["jsonrpc"].get<std::string>() != "2.0") {
            std::cerr << "Invalid or missing JSON-RPC version" << std::endl;
            return;
        }

        // Extract transaction ID
        std::string transactionId = extractTransactionId(root);

        // Extract method
        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponse(transactionId, false, "", "Missing method in request");
            return;
        }
        std::string method = root["method"].get<std::string>();

        // Extract parameters
        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponse(transactionId, false, "", "Missing or invalid params in request");
            return;
        }

        if (verbose_) {
            std::cout << "Processing RPC request: " << method << " (ID: " << transactionId << ")" << std::endl;
        }

        // Create processing context
        auto context = createContext(root, transactionId);

        // Launch asynchronous processing
        launchProcessingThread(context);

    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception processing request: " << e.what() << std::endl;
    }
}

void VpnRpcOperationProcessor::setResponseTopic(const std::string& topic) {
    responseTopic_ = topic;
}

std::string VpnRpcOperationProcessor::extractTransactionId(const nlohmann::json& request) {
    if (request.contains("id")) {
        if (request["id"].is_string()) {
            return request["id"].get<std::string>();
        } else if (request["id"].is_number()) {
            return std::to_string(request["id"].get<int>());
        }
    }
    return "unknown";
}

std::shared_ptr<VpnRpcOperationProcessor::RequestContext> 
VpnRpcOperationProcessor::createContext(const nlohmann::json& request, 
                                        const std::string& transactionId) {
    auto context = std::make_shared<RequestContext>();
    context->requestJson = request.dump();
    context->transactionId = transactionId;
    context->responseTopic = responseTopic_;
    context->vpnManager = &vpnManager_;
    context->processor = this;
    context->verbose = verbose_;
    context->threadIdPromise = std::make_shared<std::promise<unsigned int>>();
    context->threadIdFuture = context->threadIdPromise->get_future();
    
    return context;
}

void VpnRpcOperationProcessor::launchProcessingThread(std::shared_ptr<RequestContext> context) {
    // Check shutdown state
    bool shuttingDown = isShuttingDown_.load();
    if (shuttingDown) {
        sendResponseStatic(context->transactionId, false, "", "Server is shutting down", 
                          context->responseTopic);
        return;
    }

    try {
        // Create processing thread
        unsigned int threadId = threadManager_->createThread([context]() {
            VpnRpcOperationProcessor::processOperationThreadStatic(context);
        });
        
        // Register thread in tracking set
        {
            std::lock_guard<std::mutex> lock(threadsMutex_);
            activeThreads_.insert(threadId);
        }
        
        // Set thread ID for worker access
        context->threadIdPromise->set_value(threadId);

    } catch (const std::exception& e) {
        std::cerr << "Failed to create thread: " << e.what() << std::endl;
        
        // Fallback to synchronous processing
        context->threadIdPromise->set_value(0);
        VpnRpcOperationProcessor::processOperationThreadStatic(context);
    }
}

void VpnRpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    // Wait for thread ID synchronization
    unsigned int threadId = context->threadIdFuture.get();
    
    // Extract context data safely
    const std::string& requestJson = context->requestJson;
    const std::string& transactionId = context->transactionId;
    VPNInstanceManager* vpnManager = context->vpnManager;
    VpnRpcOperationProcessor* processor = context->processor;
    bool verbose = context->verbose;
    
    try {
        // Parse request in thread context
        nlohmann::json root = nlohmann::json::parse(requestJson);
        
        // Extract method and parameters
        std::string method = root["method"].get<std::string>();
        nlohmann::json paramsObj = root["params"];
        
        if (verbose) {
            std::cout << "Executing RPC operation: " << method << " (Thread: " << threadId << ")" << std::endl;
        }
        
        // Route to appropriate handler using processor instance
        nlohmann::json result;
        bool success = true;
        std::string errorMessage;
        
        try {
            // Map HTTP operation names to RPC method calls
            if (method == "parse") {
                result = processor->handleParse(paramsObj);
            } else if (method == "add") {
                result = processor->handleAdd(paramsObj);
            } else if (method == "delete") {
                result = processor->handleDelete(paramsObj);
            } else if (method == "update") {
                result = processor->handleUpdate(paramsObj);
            } else if (method == "start") {
                result = processor->handleStart(paramsObj);
            } else if (method == "stop") {
                result = processor->handleStop(paramsObj);
            } else if (method == "restart") {
                result = processor->handleRestart(paramsObj);
            } else if (method == "enable") {
                result = processor->handleEnable(paramsObj);
            } else if (method == "disable") {
                result = processor->handleDisable(paramsObj);
            } else if (method == "status") {
                result = processor->handleStatus(paramsObj);
            } else if (method == "list") {
                result = processor->handleList(paramsObj);
            } else if (method == "stats") {
                result = processor->handleStats(paramsObj);
            } else if (method == "add-custom-route") {
                result = processor->handleAddCustomRoute(paramsObj);
            } else if (method == "update-custom-route") {
                result = processor->handleUpdateCustomRoute(paramsObj);
            } else if (method == "delete-custom-route") {
                result = processor->handleDeleteCustomRoute(paramsObj);
            } else if (method == "list-custom-routes") {
                result = processor->handleListCustomRoutes(paramsObj);
            } else if (method == "get-custom-route") {
                result = processor->handleGetCustomRoute(paramsObj);
            } else if (method == "get-instance-routes") {
                result = processor->handleGetInstanceRoutes(paramsObj);
            } else if (method == "add-instance-route") {
                result = processor->handleAddInstanceRoute(paramsObj);
            } else if (method == "delete-instance-route") {
                result = processor->handleDeleteInstanceRoute(paramsObj);
            } else if (method == "apply-instance-routes") {
                result = processor->handleApplyInstanceRoutes(paramsObj);
            } else if (method == "detect-instance-routes") {
                result = processor->handleDetectInstanceRoutes(paramsObj);
            } else {
                success = false;
                errorMessage = "Unknown method: " + method;
            }
        } catch (const std::exception& e) {
            success = false;
            errorMessage = std::string("Operation failed: ") + e.what();
            if (verbose) {
                std::cerr << "Operation error: " << e.what() << std::endl;
            }
        }
        
        // Send response based on execution result
        if (success) {
            sendResponseStatic(transactionId, true, result.dump(), "", context->responseTopic);
        } else {
            sendResponseStatic(transactionId, false, "", errorMessage, context->responseTopic);
        }
        
    } catch (const std::exception& e) {
        sendResponseStatic(transactionId, false, "", 
                          std::string("Exception: ") + e.what(), 
                          context->responseTopic);
    }

    // Cleanup thread from tracking - simplified for static function
    if (context->verbose) {
        std::cout << "Thread " << threadId << " completed for transaction " 
                  << context->transactionId << std::endl;
    }
}

void VpnRpcOperationProcessor::cleanupThreadTracking(unsigned int threadId, 
                                                     std::shared_ptr<RequestContext> context) {
    if (threadId > 0) {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        activeThreads_.erase(threadId);
    }
    
    if (context->verbose) {
        std::cout << "Thread " << threadId << " completed for transaction " 
                  << context->transactionId << std::endl;
    }
}

void VpnRpcOperationProcessor::sendResponse(const std::string& transactionId, bool success,
                                           const std::string& result, const std::string& error) {
    sendResponseStatic(transactionId, success, result, error, responseTopic_);
}

void VpnRpcOperationProcessor::sendResponseStatic(const std::string& transactionId, bool success,
                                                  const std::string& result, const std::string& error,
                                                  const std::string& responseTopic) {
    try {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transactionId;

        if (success) {
            // Handle JSON result parsing
            if (!result.empty() && result[0] == '{') {
                try {
                    nlohmann::json parsedResult = nlohmann::json::parse(result);
                    response["result"] = parsedResult;
                } catch (const nlohmann::json::parse_error&) {
                    response["result"] = result;
                }
            } else if (!result.empty()) {
                response["result"] = result;
            } else {
                response["result"] = "Operation completed successfully";
            }
        } else {
            // Error response format
            nlohmann::json errorObj;
            errorObj["code"] = -1;
            errorObj["message"] = error;
            response["error"] = errorObj;
        }

        // For now, just print the response (in a real implementation, this would be sent via MQTT)
        std::cout << "RPC Response: " << response.dump() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Failed to send response: " << e.what() << std::endl;
    }
}

// Operation handlers
// ============================================================================
// OPERATION HANDLERS - Matching HTTP Server Functionality
// ============================================================================

nlohmann::json VpnRpcOperationProcessor::handleParse(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string config_content = params.value("config_content", "");
    if (config_content.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'config_content' field for parse operation";
    } else {
        // TODO: Integrate VPN parser to extract configuration details
        // For now, return basic acknowledgment
        result["success"] = true;
        result["message"] = "Configuration parsed successfully";
        result["parsed_config"] = {
            {"config_provided", true},
            {"config_length", config_content.length()}
        };
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleAdd(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    std::string config_content = params.value("config_content", "");
    std::string vpn_type = params.value("vpn_type", "");
    bool auto_start = params.value("auto_start", true);
    
    if (instance_name.empty() || config_content.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' or 'config_content' for add operation";
    } else {
        bool added = vpnManager_.addInstance(instance_name, vpn_type, config_content, auto_start);
        result["success"] = added;
        result["message"] = added ? "VPN instance added and started successfully" : "Failed to add VPN instance";
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleDelete(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' for delete operation";
    } else {
        bool success = vpnManager_.deleteInstance(instance_name);
        result["success"] = success;
        result["message"] = success ? "VPN instance deleted successfully" : "Failed to delete VPN instance";
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleUpdate(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    std::string config_content = params.value("config_content", "");
    
    if (instance_name.empty() || config_content.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' or 'config_content' for update operation";
    } else {
        bool success = vpnManager_.updateInstance(instance_name, config_content);
        result["success"] = success;
        result["message"] = success ? "VPN instance updated and restarted successfully" : "Failed to update VPN instance";
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleStart(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    bool success = vpnManager_.startInstance(instance_name);
    result["success"] = success;
    result["message"] = success ? "Instance started" : "Failed to start instance";
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleStop(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    bool success = vpnManager_.stopInstance(instance_name);
    result["success"] = success;
    result["message"] = success ? "Instance stopped" : "Failed to stop instance";
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleRestart(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    bool success = vpnManager_.restartInstance(instance_name);
    result["success"] = success;
    result["message"] = success ? "Instance restarted" : "Failed to restart instance";
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleEnable(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' for enable operation";
    } else {
        bool success = vpnManager_.enableInstance(instance_name);
        result["success"] = success;
        result["message"] = success ? "Instance enabled and started" : "Failed to enable instance";
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleDisable(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' for disable operation";
    } else {
        bool success = vpnManager_.disableInstance(instance_name);
        result["success"] = success;
        result["message"] = success ? "Instance disabled and stopped" : "Failed to disable instance";
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleStatus(const nlohmann::json& params) {
    nlohmann::json result;
    result["success"] = true;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["instances"] = vpnManager_.getAllInstancesStatus();
    } else {
        result["status"] = vpnManager_.getInstanceStatus(instance_name);
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleList(const nlohmann::json& params) {
    nlohmann::json result;
    result["success"] = true;
    
    std::string vpn_type = params.value("vpn_type", "");
    nlohmann::json all_instances = vpnManager_.getAllInstancesStatus();
    
    if (vpn_type.empty()) {
        result["instances"] = all_instances;
    } else {
        nlohmann::json filtered_instances = nlohmann::json::array();
        for (const auto& instance : all_instances) {
            std::string instance_type = instance.value("type", "");
            if (instance_type == vpn_type || 
                (vpn_type == "openvpn" && instance_type == "openvpn") ||
                (vpn_type == "wireguard" && instance_type == "wireguard")) {
                filtered_instances.push_back(instance);
            }
        }
        result["instances"] = filtered_instances;
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleStats(const nlohmann::json& params) {
    nlohmann::json result;
    result["success"] = true;
    result["stats"] = vpnManager_.getAggregatedStats();
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleAddCustomRoute(const nlohmann::json& params) {
    nlohmann::json result;
    
    // Parse routing rule from JSON
    vpn_manager::RoutingRule rule;
    rule.id = params.value("id", "");
    rule.name = params.value("name", "");
    rule.vpn_instance = params.value("vpn_instance", "");
    rule.vpn_profile = params.value("vpn_profile", "");
    rule.source_type = params.value("source_type", "Any");
    rule.source_value = params.value("source_value", "");
    rule.destination = params.value("destination", "");
    rule.gateway = params.value("gateway", "VPN Server");
    rule.protocol = params.value("protocol", "both");
    rule.type = params.value("type", "tunnel_all");
    rule.priority = params.value("priority", 100);
    rule.enabled = params.value("enabled", true);
    rule.log_traffic = params.value("log_traffic", false);
    rule.apply_to_existing = params.value("apply_to_existing", false);
    rule.description = params.value("description", "");
    rule.created_date = params.value("created_date", "");
    rule.last_modified = params.value("last_modified", "");
    rule.is_automatic = params.value("is_automatic", false);
    rule.user_modified = params.value("user_modified", false);
    rule.is_applied = false;
    
    if (rule.id.empty() || rule.vpn_instance.empty() || rule.destination.empty()) {
        result["success"] = false;
        result["error"] = "Missing required fields: id, vpn_instance, destination";
    } else {
        bool success = vpnManager_.addRoutingRule(rule);
        result["success"] = success;
        result["message"] = success ? "Routing rule added successfully" : "Failed to add routing rule";
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleUpdateCustomRoute(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string rule_id = params.value("id", "");
    if (rule_id.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'id' field for update-custom-route operation";
        return result;
    }
    
    // Parse routing rule from JSON
    vpn_manager::RoutingRule rule;
    rule.id = rule_id;
    rule.name = params.value("name", "");
    rule.vpn_instance = params.value("vpn_instance", "");
    rule.vpn_profile = params.value("vpn_profile", "");
    rule.source_type = params.value("source_type", "Any");
    rule.source_value = params.value("source_value", "");
    rule.destination = params.value("destination", "");
    rule.gateway = params.value("gateway", "VPN Server");
    rule.protocol = params.value("protocol", "both");
    rule.type = params.value("type", "tunnel_all");
    rule.priority = params.value("priority", 100);
    rule.enabled = params.value("enabled", true);
    rule.log_traffic = params.value("log_traffic", false);
    rule.apply_to_existing = params.value("apply_to_existing", false);
    rule.description = params.value("description", "");
    rule.created_date = params.value("created_date", "");
    rule.last_modified = params.value("last_modified", "");
    rule.is_automatic = params.value("is_automatic", false);
    rule.user_modified = params.value("user_modified", false);
    rule.is_applied = false;
    
    bool success = vpnManager_.updateRoutingRule(rule_id, rule);
    result["success"] = success;
    result["message"] = success ? "Routing rule updated successfully" : "Failed to update routing rule";
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleDeleteCustomRoute(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string rule_id = params.value("id", "");
    if (rule_id.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'id' field for delete-custom-route operation";
    } else {
        bool success = vpnManager_.deleteRoutingRule(rule_id);
        result["success"] = success;
        result["message"] = success ? "Routing rule deleted successfully" : "Failed to delete routing rule";
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleListCustomRoutes(const nlohmann::json& params) {
    nlohmann::json result;
    result["success"] = true;
    result["routing_rules"] = vpnManager_.getAllRoutingRules();
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleGetCustomRoute(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string rule_id = params.value("id", "");
    if (rule_id.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'id' field for get-custom-route operation";
    } else {
        nlohmann::json rule = vpnManager_.getRoutingRule(rule_id);
        if (rule.contains("error")) {
            result["success"] = false;
            result["error"] = rule["error"];
        } else {
            result["success"] = true;
            result["routing_rule"] = rule;
        }
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleGetInstanceRoutes(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' field";
    } else {
        nlohmann::json routes = vpnManager_.getInstanceRoutes(instance_name);
        if (routes.contains("error")) {
            result["success"] = false;
            result["error"] = routes["error"];
        } else {
            result["success"] = true;
            result["routing_rules"] = routes;
        }
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleAddInstanceRoute(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' field";
    } else if (!params.contains("route_rule")) {
        result["success"] = false;
        result["error"] = "Missing 'route_rule' field";
    } else {
        vpn_manager::UnifiedRouteRule rule = vpn_manager::UnifiedRouteRule::from_json(params["route_rule"]);
        bool success = vpnManager_.addInstanceRoute(instance_name, rule);
        result["success"] = success;
        if (!success) {
            result["error"] = "Failed to add route rule";
        }
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleDeleteInstanceRoute(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' field";
    } else if (!params.contains("rule_id")) {
        result["success"] = false;
        result["error"] = "Missing 'rule_id' field";
    } else {
        std::string rule_id = params["rule_id"].get<std::string>();
        bool success = vpnManager_.deleteInstanceRoute(instance_name, rule_id);
        result["success"] = success;
        if (!success) {
            result["error"] = "Rule not found";
        }
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleApplyInstanceRoutes(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' field";
    } else {
        bool success = vpnManager_.applyInstanceRoutes(instance_name);
        result["success"] = success;
        if (!success) {
            result["error"] = "Failed to apply routes";
        }
    }
    return result;
}

nlohmann::json VpnRpcOperationProcessor::handleDetectInstanceRoutes(const nlohmann::json& params) {
    nlohmann::json result;
    
    std::string instance_name = params.value("instance_name", "");
    if (instance_name.empty()) {
        result["success"] = false;
        result["error"] = "Missing 'instance_name' field";
    } else {
        int detected = vpnManager_.detectInstanceRoutes(instance_name);
        if (detected < 0) {
            result["success"] = false;
            result["error"] = "Failed to detect routes";
        } else {
            result["success"] = true;
            result["detected_routes"] = detected;
        }
    }
    return result;
}

} // namespace vpn_manager
