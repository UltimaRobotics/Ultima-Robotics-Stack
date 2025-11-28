#include "http_server.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace http_server {

struct ConnectionInfo {
    std::string post_data;
};

HTTPServer::HTTPServer(const std::string& host, int port) 
    : daemon(nullptr), host(host), port(port), running(false), vpn_manager(nullptr) {
}

HTTPServer::~HTTPServer() {
    stop();
}

bool HTTPServer::start() {
    if (running) {
        std::cerr << "HTTP server already running" << std::endl;
        return false;
    }

    daemon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY,
        port,
        nullptr, nullptr,
        &HTTPServer::handleRequest, this,
        MHD_OPTION_END
    );

    if (daemon == nullptr) {
        std::cerr << "Failed to start HTTP server on " << host << ":" << port << std::endl;
        return false;
    }

    running = true;
    std::cout << "HTTP server started on " << host << ":" << port << std::endl;
    return true;
}

void HTTPServer::stop() {
    if (daemon) {
        MHD_stop_daemon(daemon);
        daemon = nullptr;
        running = false;
        std::cout << "HTTP server stopped" << std::endl;
    }
}

bool HTTPServer::isRunning() const {
    return running;
}

void HTTPServer::setVPNManager(vpn_manager::VPNInstanceManager* manager) {
    vpn_manager = manager;
}

bool HTTPServer::isVerbose() const {
    return vpn_manager && vpn_manager->isVerbose();
}

MHD_Result HTTPServer::handleRequest(void* cls, struct MHD_Connection* connection,
                                    const char* url, const char* method,
                                    const char* /*version*/, const char* upload_data,
                                    size_t* upload_data_size, void** con_cls) {

    HTTPServer* server = static_cast<HTTPServer*>(cls);

    // Handle CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response* response = MHD_create_response_from_buffer(0, (void*)"", MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return static_cast<MHD_Result>(ret);
    }

    // Only handle POST requests to /api/operations/
    if (strcmp(url, "/api/operations/") != 0) {
        json error_response;
        error_response["success"] = false;
        error_response["error"] = "Invalid endpoint. Use /api/operations/";
        return sendResponse(connection, error_response.dump(2), MHD_HTTP_NOT_FOUND);
    }

    if (strcmp(method, "POST") != 0) {
        json error_response;
        error_response["success"] = false;
        error_response["error"] = "Only POST method is supported";
        return sendResponse(connection, error_response.dump(2), MHD_HTTP_METHOD_NOT_ALLOWED);
    }

    // Initialize connection info on first call
    if (*con_cls == nullptr) {
        ConnectionInfo* info = new ConnectionInfo();
        *con_cls = info;
        return MHD_YES;
    }

    ConnectionInfo* info = static_cast<ConnectionInfo*>(*con_cls);

    // Accumulate POST data
    if (*upload_data_size > 0) {
        info->post_data.append(upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }

    // Process the complete request
    std::string response_data = server->processOperation(info->post_data);

    // Clean up connection info
    delete info;
    *con_cls = nullptr;

    return sendResponse(connection, response_data, MHD_HTTP_OK);
}

std::string HTTPServer::processOperation(const std::string& json_data) {
    json response;

    try {
        if (isVerbose()) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "HTTP: Received operation request"},
                {"data", json_data}
            }).dump() << std::endl;
        }

        json operation_json = json::parse(json_data);

        if (!operation_json.contains("operation_type")) {
            response["success"] = false;
            response["error"] = "Missing 'operation_type' field";
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Missing operation_type field"}
                }).dump() << std::endl;
            }
            return response.dump(2);
        }

        std::string operation = operation_json["operation_type"].get<std::string>();
        std::string instance_name = operation_json.value("instance_name", "");

        if (isVerbose()) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "HTTP: Processing operation"},
                {"operation", operation},
                {"instance_name", instance_name}
            }).dump() << std::endl;
        }

        if (!vpn_manager) {
            response["success"] = false;
            response["error"] = "VPN manager not initialized";
            return response.dump(2);
        }

        if (operation == "parse") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing parse operation"}
                }).dump() << std::endl;
            }

            // Parse VPN configuration without making system changes
            std::string config_content = operation_json.value("config_content", "");
            if (config_content.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'config_content' field for parse operation";
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "HTTP: Parse failed - missing config_content"}
                    }).dump() << std::endl;
                }
            } else {
                // TODO: Integrate VPN parser to extract configuration details
                // For now, return basic acknowledgment
                response["success"] = true;
                response["message"] = "Configuration parsed successfully";
                response["parsed_config"] = {
                    {"config_provided", true},
                    {"config_length", config_content.length()}
                };
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "HTTP: Parse completed successfully"},
                        {"config_length", config_content.length()}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "add") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing add operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }

            // Add a new VPN instance
            std::string config_content = operation_json.value("config_content", "");
            std::string vpn_type = operation_json.value("vpn_type", "");
            bool auto_start = operation_json.value("auto_start", true);

            if (instance_name.empty() || config_content.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' or 'config_content' for add operation";
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "HTTP: Add failed - missing required fields"}
                    }).dump() << std::endl;
                }
            } else {
                // Add instance to manager (this will save to config)
                bool added = vpn_manager->addInstance(instance_name, vpn_type, config_content, auto_start);

                if (added) {
                    response["success"] = true;
                    response["message"] = "VPN instance added and started successfully";
                    if (isVerbose()) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "HTTP: Add completed successfully"},
                            {"instance_name", instance_name},
                            {"vpn_type", vpn_type},
                            {"auto_start", auto_start}
                        }).dump() << std::endl;
                    }
                } else {
                    response["success"] = false;
                    response["error"] = "Failed to add VPN instance";
                    if (isVerbose()) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "HTTP: Add failed"},
                            {"instance_name", instance_name}
                        }).dump() << std::endl;
                    }
                }
            }
        }
        else if (operation == "delete") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing delete operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }

            // Delete an existing VPN instance
            std::string vpn_type = operation_json.value("vpn_type", "");

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' for delete operation";
            } else {
                bool success = vpn_manager->deleteInstance(instance_name);
                response["success"] = success;
                response["message"] = success ? "VPN instance deleted successfully" : "Failed to delete VPN instance";
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", success ? "HTTP: Delete completed successfully" : "HTTP: Delete failed"},
                        {"instance_name", instance_name}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "update") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing update operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }

            // Update an existing VPN instance
            std::string config_content = operation_json.value("config_content", "");

            if (instance_name.empty() || config_content.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' or 'config_content' for update operation";
            } else {
                bool success = vpn_manager->updateInstance(instance_name, config_content);

                if (success) {
                    response["success"] = true;
                    response["message"] = "VPN instance updated and restarted successfully";
                    if (isVerbose()) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "HTTP: Update completed successfully"},
                            {"instance_name", instance_name}
                        }).dump() << std::endl;
                    }
                } else {
                    response["success"] = false;
                    response["error"] = "Failed to update VPN instance";
                    if (isVerbose()) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "HTTP: Update failed"},
                            {"instance_name", instance_name}
                        }).dump() << std::endl;
                    }
                }
            }
        }
        else if (operation == "set_auto_routing") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing set_auto_routing operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }

            // Set auto routing for an existing VPN instance
            bool enable_auto_routing = operation_json.value("enable_auto_routing", true);

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' for set_auto_routing operation";
            } else {
                bool success = vpn_manager->setInstanceAutoRouting(instance_name, enable_auto_routing);

                if (success) {
                    response["success"] = true;
                    response["message"] = enable_auto_routing ? 
                        "Auto routing enabled for VPN instance" : 
                        "Auto routing disabled for VPN instance";
                    response["enable_auto_routing"] = enable_auto_routing;
                    if (isVerbose()) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "HTTP: Set auto routing completed successfully"},
                            {"instance_name", instance_name},
                            {"enable_auto_routing", enable_auto_routing}
                        }).dump() << std::endl;
                    }
                } else {
                    response["success"] = false;
                    response["error"] = "Failed to set auto routing for VPN instance";
                    if (isVerbose()) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "HTTP: Set auto routing failed"},
                            {"instance_name", instance_name}
                        }).dump() << std::endl;
                    }
                }
            }
        }
        else if (operation == "start") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing start operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }
            bool success = vpn_manager->startInstance(instance_name);
            response["success"] = success;
            response["message"] = success ? "Instance started" : "Failed to start instance";
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", success ? "HTTP: Start completed" : "HTTP: Start failed"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }
        }
        else if (operation == "stop") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing stop operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }
            bool success = vpn_manager->stopInstance(instance_name);
            response["success"] = success;
            response["message"] = success ? "Instance stopped" : "Failed to stop instance";
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", success ? "HTTP: Stop completed" : "HTTP: Stop failed"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }
        }
        else if (operation == "restart") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing restart operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }
            bool success = vpn_manager->restartInstance(instance_name);
            response["success"] = success;
            response["message"] = success ? "Instance restarted" : "Failed to restart instance";
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", success ? "HTTP: Restart completed" : "HTTP: Restart failed"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }
        }
        else if (operation == "status") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing status operation"},
                    {"instance_name", instance_name.empty() ? "all" : instance_name}
                }).dump() << std::endl;
            }
            if (instance_name.empty()) {
                response["success"] = true;
                response["instances"] = vpn_manager->getAllInstancesStatus();
            } else {
                response["success"] = true;
                response["status"] = vpn_manager->getInstanceStatus(instance_name);
            }
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Status operation completed"}
                }).dump() << std::endl;
            }
        }
        else if (operation == "list") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing list operation"}
                }).dump() << std::endl;
            }

            // List all VPN instances, optionally filtered by type
            std::string vpn_type = operation_json.value("vpn_type", "");
            json all_instances = vpn_manager->getAllInstancesStatus();

            if (vpn_type.empty()) {
                // Return all instances
                response["success"] = true;
                response["instances"] = all_instances;
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "HTTP: List operation completed"},
                        {"total_instances", all_instances.size()}
                    }).dump() << std::endl;
                }
            } else {
                // Filter by VPN type
                json filtered_instances = json::array();
                for (const auto& instance : all_instances) {
                    std::string instance_type = instance.value("type", "");
                    if (instance_type == vpn_type || 
                        (vpn_type == "openvpn" && instance_type == "openvpn") ||
                        (vpn_type == "wireguard" && instance_type == "wireguard")) {
                        filtered_instances.push_back(instance);
                    }
                }
                response["success"] = true;
                response["instances"] = filtered_instances;
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "HTTP: List operation completed"},
                        {"vpn_type", vpn_type},
                        {"filtered_instances", filtered_instances.size()}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "stats") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing stats operation"}
                }).dump() << std::endl;
            }
            response["success"] = true;
            response["stats"] = vpn_manager->getAggregatedStats();
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Stats operation completed"}
                }).dump() << std::endl;
            }
        }
        else if (operation == "enable") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing enable operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' for enable operation";
            } else {
                bool success = vpn_manager->enableInstance(instance_name);
                response["success"] = success;
                response["message"] = success ? "Instance enabled and started" : "Failed to enable instance";
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", success ? "HTTP: Enable completed" : "HTTP: Enable failed"},
                        {"instance_name", instance_name}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "disable") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing disable operation"},
                    {"instance_name", instance_name}
                }).dump() << std::endl;
            }

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' for disable operation";
            } else {
                bool success = vpn_manager->disableInstance(instance_name);
                response["success"] = success;
                response["message"] = success ? "Instance disabled and stopped" : "Failed to disable instance";
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", success ? "HTTP: Disable completed" : "HTTP: Disable failed"},
                        {"instance_name", instance_name}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "add-custom-route") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing add-custom-route operation"}
                }).dump() << std::endl;
            }

            // Parse routing rule from JSON
            vpn_manager::RoutingRule rule;
            rule.id = operation_json.value("id", "");
            rule.name = operation_json.value("name", "");
            rule.vpn_instance = operation_json.value("vpn_instance", "");
            rule.vpn_profile = operation_json.value("vpn_profile", "");
            rule.source_type = operation_json.value("source_type", "Any");
            rule.source_value = operation_json.value("source_value", "");
            rule.destination = operation_json.value("destination", "");
            rule.gateway = operation_json.value("gateway", "VPN Server");
            rule.protocol = operation_json.value("protocol", "both");
            rule.type = operation_json.value("type", "tunnel_all");
            rule.priority = operation_json.value("priority", 100);
            rule.enabled = operation_json.value("enabled", true);
            rule.log_traffic = operation_json.value("log_traffic", false);
            rule.apply_to_existing = operation_json.value("apply_to_existing", false);
            rule.description = operation_json.value("description", "");
            rule.created_date = operation_json.value("created_date", "");
            rule.last_modified = operation_json.value("last_modified", "");
            rule.is_automatic = operation_json.value("is_automatic", false);
            rule.user_modified = operation_json.value("user_modified", false);
            rule.is_applied = false;

            if (rule.id.empty() || rule.vpn_instance.empty() || rule.destination.empty()) {
                response["success"] = false;
                response["error"] = "Missing required fields: id, vpn_instance, destination";
            } else {
                bool success = vpn_manager->addRoutingRule(rule);
                response["success"] = success;
                response["message"] = success ? "Routing rule added successfully" : "Failed to add routing rule";
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", success ? "HTTP: Add custom route completed" : "HTTP: Add custom route failed"},
                        {"rule_id", rule.id}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "update-custom-route") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing update-custom-route operation"}
                }).dump() << std::endl;
            }

            std::string rule_id = operation_json.value("id", "");

            // Parse routing rule from JSON
            vpn_manager::RoutingRule rule;
            rule.id = rule_id;
            rule.name = operation_json.value("name", "");
            rule.vpn_instance = operation_json.value("vpn_instance", "");
            rule.vpn_profile = operation_json.value("vpn_profile", "");
            rule.source_type = operation_json.value("source_type", "Any");
            rule.source_value = operation_json.value("source_value", "");
            rule.destination = operation_json.value("destination", "");
            rule.gateway = operation_json.value("gateway", "VPN Server");
            rule.protocol = operation_json.value("protocol", "both");
            rule.type = operation_json.value("type", "tunnel_all");
            rule.priority = operation_json.value("priority", 100);
            rule.enabled = operation_json.value("enabled", true);
            rule.log_traffic = operation_json.value("log_traffic", false);
            rule.apply_to_existing = operation_json.value("apply_to_existing", false);
            rule.description = operation_json.value("description", "");
            rule.created_date = operation_json.value("created_date", "");
            rule.last_modified = operation_json.value("last_modified", "");
            rule.is_automatic = operation_json.value("is_automatic", false);
            rule.user_modified = operation_json.value("user_modified", false);
            rule.is_applied = false;

            if (rule_id.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'id' field for update-custom-route operation";
            } else {
                bool success = vpn_manager->updateRoutingRule(rule_id, rule);
                response["success"] = success;
                response["message"] = success ? "Routing rule updated successfully" : "Failed to update routing rule";
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", success ? "HTTP: Update custom route completed" : "HTTP: Update custom route failed"},
                        {"rule_id", rule_id}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "delete-custom-route") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing delete-custom-route operation"}
                }).dump() << std::endl;
            }

            std::string rule_id = operation_json.value("id", "");

            if (rule_id.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'id' field for delete-custom-route operation";
            } else {
                bool success = vpn_manager->deleteRoutingRule(rule_id);
                response["success"] = success;
                response["message"] = success ? "Routing rule deleted successfully" : "Failed to delete routing rule";
                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", success ? "HTTP: Delete custom route completed" : "HTTP: Delete custom route failed"},
                        {"rule_id", rule_id}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "list-custom-routes") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing list-custom-routes operation"}
                }).dump() << std::endl;
            }

            response["success"] = true;
            response["routing_rules"] = vpn_manager->getAllRoutingRules();

            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: List custom routes completed"}
                }).dump() << std::endl;
            }
        }
        else if (operation == "get-custom-route") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing get-custom-route operation"}
                }).dump() << std::endl;
            }

            std::string rule_id = operation_json.value("id", "");

            if (rule_id.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'id' field for get-custom-route operation";
            } else {
                json rule = vpn_manager->getRoutingRule(rule_id);

                if (rule.contains("error")) {
                    response["success"] = false;
                    response["error"] = rule["error"];
                } else {
                    response["success"] = true;
                    response["routing_rule"] = rule;
                }

                if (isVerbose()) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "HTTP: Get custom route completed"},
                        {"rule_id", rule_id}
                    }).dump() << std::endl;
                }
            }
        }
        else if (operation == "get-instance-routes") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing get-instance-routes operation"}
                }).dump() << std::endl;
            }

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' field";
            } else {
                json routes = vpn_manager->getInstanceRoutes(instance_name);
                if (routes.contains("error")) {
                    response["success"] = false;
                    response["error"] = routes["error"];
                } else {
                    response["success"] = true;
                    response["routing_rules"] = routes;
                }
            }
        }
        else if (operation == "add-instance-route") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing add-instance-route operation"}
                }).dump() << std::endl;
            }

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' field";
            } else if (!operation_json.contains("route_rule")) {
                response["success"] = false;
                response["error"] = "Missing 'route_rule' field";
            } else {
                vpn_manager::UnifiedRouteRule rule = vpn_manager::UnifiedRouteRule::from_json(operation_json["route_rule"]);
                bool success = vpn_manager->addInstanceRoute(instance_name, rule);
                response["success"] = success;
                if (!success) {
                    response["error"] = "Failed to add route rule";
                }
            }
        }
        else if (operation == "delete-instance-route") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing delete-instance-route operation"}
                }).dump() << std::endl;
            }

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' field";
            } else if (!operation_json.contains("rule_id")) {
                response["success"] = false;
                response["error"] = "Missing 'rule_id' field";
            } else {
                std::string rule_id = operation_json["rule_id"].get<std::string>();
                bool success = vpn_manager->deleteInstanceRoute(instance_name, rule_id);
                response["success"] = success;
                if (!success) {
                    response["error"] = "Rule not found";
                }
            }
        }
        else if (operation == "apply-instance-routes") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing apply-instance-routes operation"}
                }).dump() << std::endl;
            }

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' field";
            } else {
                bool success = vpn_manager->applyInstanceRoutes(instance_name);
                response["success"] = success;
                if (!success) {
                    response["error"] = "Failed to apply routes";
                }
            }
        }
        else if (operation == "detect-instance-routes") {
            if (isVerbose()) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "HTTP: Executing detect-instance-routes operation"}
                }).dump() << std::endl;
            }

            if (instance_name.empty()) {
                response["success"] = false;
                response["error"] = "Missing 'instance_name' field";
            } else {
                int detected = vpn_manager->detectInstanceRoutes(instance_name);
                if (detected < 0) {
                    response["success"] = false;
                    response["error"] = "Failed to detect routes";
                } else {
                    response["success"] = true;
                    response["detected_routes"] = detected;
                }
            }
        }
        else {
            response["success"] = false;
            response["error"] = "Unknown operation type: " + operation;
        }

    } catch (const json::parse_error& e) {
        response["success"] = false;
        response["error"] = "JSON parse error: " + std::string(e.what());
    } catch (const std::exception& e) {
        response["success"] = false;
        response["error"] = "Server error: " + std::string(e.what());
    }

    return response.dump(2);
}

MHD_Result HTTPServer::sendResponse(struct MHD_Connection* connection, 
                                   const std::string& response_data,
                                   int status_code) {
    struct MHD_Response* response = MHD_create_response_from_buffer(
        response_data.size(),
        (void*)response_data.c_str(),
        MHD_RESPMEM_MUST_COPY
    );

    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");

    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);

    return static_cast<MHD_Result>(ret);
}

} // namespace http_server