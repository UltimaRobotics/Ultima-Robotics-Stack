#include "vpn_instance_manager.hpp"
#include "internal/vpn_manager_utils.hpp"
#include "wireguard_routing_provider.hpp"
#include "openvpn_routing_provider.hpp"
#include "../ur-wg_library/wireguard-wrapper/include/wireguard_wrapper.hpp"
#include "../ur-openvpn-library/src/openvpn_wrapper.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <algorithm>
#include <thread>
#include <chrono>
#include <sys/stat.h>

namespace vpn_manager {

std::vector<RoutingRule> VPNInstanceManager::parseRouteOutput(const std::string& route_output, 
                                                               const std::string& instance_name) {
    std::vector<RoutingRule> rules;
    std::istringstream stream(route_output);
    std::string line;
    
    // Skip header lines
    std::getline(stream, line);
    std::getline(stream, line);
    
    // Get expected interface for this instance
    std::string expected_iface = getInterfaceForInstance(instance_name);
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        std::istringstream line_stream(line);
        std::string destination, gateway, netmask, flags, metric, ref, use, iface;
        
        line_stream >> destination >> gateway >> netmask >> flags >> metric >> ref >> use >> iface;
        
        // Only process routes for this VPN interface
        if (iface != expected_iface) {
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "Skipping route - interface mismatch"},
                    {"instance", instance_name},
                    {"expected_iface", expected_iface},
                    {"actual_iface", iface},
                    {"destination", destination},
                    {"gateway", gateway}
                }).dump() << std::endl;
            }
            continue;
        }
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Processing route for instance"},
                {"instance", instance_name},
                {"interface", iface},
                {"destination", destination},
                {"gateway", gateway},
                {"netmask", netmask}
            }).dump() << std::endl;
        }
        
        // Process all routes including default routes (0.0.0.0/1 and 128.0.0.0/1 for OpenVPN)
        // These are important for split-tunnel configurations
        
        // Create automatic routing rule
        RoutingRule rule;
        rule.id = "auto_" + instance_name + "_" + destination + "_" + std::to_string(std::hash<std::string>{}(line));
        rule.name = "Auto: " + destination + " via " + iface;
        rule.vpn_instance = instance_name;
        rule.vpn_profile = instance_name;
        rule.source_type = "Any";
        rule.source_value = "";
        
        // Format destination with CIDR
        std::string cidr = getCidrFromNetmask(netmask);
        rule.destination = destination + "/" + cidr;
        
        rule.gateway = (gateway == "0.0.0.0") ? "VPN Server" : gateway;
        rule.protocol = "both";
        
        // Determine rule type based on destination
        if (destination == "0.0.0.0" && netmask == "128.0.0.0") {
            // Split default route (0.0.0.0/1)
            rule.type = "tunnel_all";
            rule.description = "Automatically detected default route split (first half) for " + instance_name;
        } else if (destination == "128.0.0.0" && netmask == "128.0.0.0") {
            // Split default route (128.0.0.0/1)
            rule.type = "tunnel_all";
            rule.description = "Automatically detected default route split (second half) for " + instance_name;
        } else if (destination == "0.0.0.0" && netmask == "0.0.0.0") {
            // Full default route (skip, usually not VPN-specific)
            continue;
        } else {
            // Regular network route
            rule.type = "tunnel_all";
            rule.description = "Automatically detected route for " + instance_name;
        }
        
        rule.priority = std::stoi(metric.empty() ? "0" : metric);
        rule.enabled = true;
        rule.log_traffic = false;
        rule.apply_to_existing = false;
        
        time_t now = time(nullptr);
        rule.created_date = std::to_string(now);
        rule.last_modified = std::to_string(now);
        rule.is_automatic = true;
        rule.user_modified = false;
        rule.is_applied = true; // Already applied by the system
        
        rules.push_back(rule);
    }
    
    return rules;
}
void VPNInstanceManager::mergeAutomaticRoutes(const std::vector<RoutingRule>& detected_rules, 
                                               const std::string& instance_name) {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    
    // Mark all existing automatic rules for this instance as potentially stale
    std::set<std::string> detected_rule_ids;
    for (const auto& rule : detected_rules) {
        detected_rule_ids.insert(rule.id);
    }
    
    // Remove automatic rules that are no longer detected (unless user modified)
    std::vector<std::string> rules_to_remove;
    for (auto& [id, rule] : routing_rules_) {
        if (rule.vpn_instance == instance_name && rule.is_automatic && !rule.user_modified) {
            if (detected_rule_ids.find(id) == detected_rule_ids.end()) {
                rules_to_remove.push_back(id);
            }
        }
    }
    
    for (const auto& id : rules_to_remove) {
        routing_rules_.erase(id);
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Removed stale automatic route"},
                {"rule_id", id}
            }).dump() << std::endl;
        }
    }
    
    // Add or update detected rules
    for (const auto& rule : detected_rules) {
        auto it = routing_rules_.find(rule.id);
        
        if (it == routing_rules_.end()) {
            // New automatic rule
            routing_rules_[rule.id] = rule;
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "Added new automatic route"},
                    {"rule_id", rule.id},
                    {"destination", rule.destination}
                }).dump() << std::endl;
            }
        } else if (!it->second.user_modified) {
            // Update existing automatic rule (only if not modified by user)
            it->second.destination = rule.destination;
            it->second.gateway = rule.gateway;
            it->second.priority = rule.priority;
            it->second.last_modified = rule.last_modified;
            it->second.is_applied = true;
        }
        // If user_modified is true, keep the user's changes
    }
}

// Detect routes and save them to routing rules file
void VPNInstanceManager::detectAndSaveAutomaticRoutes(const std::string& instance_name, 
                                                      const std::string& interface_name) {
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Detecting automatic routes"},
            {"instance", instance_name},
            {"interface", interface_name}
        }).dump() << std::endl;
    }
    
    // Retry logic for interface availability
    int max_retries = 5;
    bool interface_ready = false;
    
    for (int retry = 0; retry < max_retries; retry++) {
        std::string check_cmd = "ip link show " + interface_name + " 2>/dev/null";
        std::string check_result = executeCommand(check_cmd);
        
        if (!check_result.empty()) {
            interface_ready = true;
            break;
        }
        
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Interface not available yet, retrying"},
                {"instance", instance_name},
                {"interface", interface_name},
                {"retry", retry + 1},
                {"max_retries", max_retries}
            }).dump() << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    if (!interface_ready) {
        if (verbose_) {
            std::cout << json({
                {"type", "warning"},
                {"message", "Interface not available after retries - route detection failed"},
                {"instance", instance_name},
                {"interface", interface_name}
            }).dump() << std::endl;
        }
        return;
    }
    
    // Wait for routes to stabilize (especially for OpenVPN)
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Execute route -n command
    std::string route_output = executeCommand("route -n");
    
    if (route_output.empty()) {
        if (verbose_) {
            std::cout << json({
                {"type", "warning"},
                {"message", "No route output detected - routing table may be empty"},
                {"instance", instance_name}
            }).dump() << std::endl;
        }
        return;
    }
    
    // Parse routes
    std::vector<RoutingRule> detected_rules = parseRouteOutput(route_output, instance_name);
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Detected automatic routes"},
            {"instance", instance_name},
            {"count", detected_rules.size()}
        }).dump() << std::endl;
        
        // Log each detected route for debugging
        for (const auto& rule : detected_rules) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "Detected route details"},
                {"instance", instance_name},
                {"destination", rule.destination},
                {"gateway", rule.gateway},
                {"rule_name", rule.name}
            }).dump() << std::endl;
        }
    }
    
    // Merge with existing rules
    mergeAutomaticRoutes(detected_rules, instance_name);
    
    // Save to file
    if (!routing_rules_file_path_.empty()) {
        saveRoutingRules(routing_rules_file_path_);
    }
}
bool VPNInstanceManager::addRoutingRule(const RoutingRule& rule) {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    
    if (routing_rules_.find(rule.id) != routing_rules_.end()) {
        std::cerr << "Routing rule with id " << rule.id << " already exists" << std::endl;
        return false;
    }
    
    routing_rules_[rule.id] = rule;
    
    // Save to file
    if (!routing_rules_file_path_.empty()) {
        saveRoutingRules(routing_rules_file_path_);
    }
    
    // If rule is enabled and VPN instance is connected, apply it
    if (rule.enabled) {
        auto it = instances_.find(rule.vpn_instance);
        if (it != instances_.end() && it->second.current_state == ConnectionState::CONNECTED) {
            std::string interface = getInterfaceForInstance(rule.vpn_instance);
            if (!interface.empty()) {
                applyRoutingRule(rule, interface);
            }
        }
    }
    
    return true;
}

bool VPNInstanceManager::updateRoutingRule(const std::string& rule_id, const RoutingRule& rule) {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    
    auto it = routing_rules_.find(rule_id);
    if (it == routing_rules_.end()) {
        std::cerr << "Routing rule with id " << rule_id << " not found" << std::endl;
        return false;
    }
    
    // Remove old rule if it was applied
    if (it->second.is_applied) {
        std::string interface = getInterfaceForInstance(it->second.vpn_instance);
        if (!interface.empty()) {
            removeRoutingRule(it->second, interface);
        }
    }
    
    // Update rule
    routing_rules_[rule_id] = rule;
    
    // Save to file
    if (!routing_rules_file_path_.empty()) {
        saveRoutingRules(routing_rules_file_path_);
    }
    
    // Apply new rule if enabled and VPN is connected
    if (rule.enabled) {
        auto inst_it = instances_.find(rule.vpn_instance);
        if (inst_it != instances_.end() && inst_it->second.current_state == ConnectionState::CONNECTED) {
            std::string interface = getInterfaceForInstance(rule.vpn_instance);
            if (!interface.empty()) {
                applyRoutingRule(rule, interface);
            }
        }
    }
    
    return true;
}

bool VPNInstanceManager::deleteRoutingRule(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    
    auto it = routing_rules_.find(rule_id);
    if (it == routing_rules_.end()) {
        std::cerr << "Routing rule with id " << rule_id << " not found" << std::endl;
        return false;
    }
    
    // Remove rule if applied
    if (it->second.is_applied) {
        std::string interface = getInterfaceForInstance(it->second.vpn_instance);
        if (!interface.empty()) {
            removeRoutingRule(it->second, interface);
        }
    }
    
    routing_rules_.erase(it);
    
    // Save to file
    if (!routing_rules_file_path_.empty()) {
        saveRoutingRules(routing_rules_file_path_);
    }
    
    return true;
}

json VPNInstanceManager::getRoutingRule(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    
    auto it = routing_rules_.find(rule_id);
    if (it == routing_rules_.end()) {
        return {{"error", "Routing rule not found"}};
    }
    
    json rule;
    rule["id"] = it->second.id;
    rule["name"] = it->second.name;
    rule["vpn_instance"] = it->second.vpn_instance;
    rule["vpn_profile"] = it->second.vpn_profile;
    rule["source_type"] = it->second.source_type;
    rule["source_value"] = it->second.source_value;
    rule["destination"] = it->second.destination;
    rule["gateway"] = it->second.gateway;
    rule["protocol"] = it->second.protocol;
    rule["type"] = it->second.type;
    rule["priority"] = it->second.priority;
    rule["enabled"] = it->second.enabled;
    rule["log_traffic"] = it->second.log_traffic;
    rule["apply_to_existing"] = it->second.apply_to_existing;
    rule["description"] = it->second.description;
    rule["created_date"] = it->second.created_date;
    rule["last_modified"] = it->second.last_modified;
    rule["is_applied"] = it->second.is_applied;
    rule["is_automatic"] = it->second.is_automatic;
    rule["user_modified"] = it->second.user_modified;
    
    return rule;
}

json VPNInstanceManager::getAllRoutingRules() {
    std::lock_guard<std::mutex> lock(routing_mutex_);
    
    json rules = json::array();
    for (const auto& [id, rule] : routing_rules_) {
        json r;
        r["id"] = rule.id;
        r["name"] = rule.name;
        r["vpn_instance"] = rule.vpn_instance;
        r["vpn_profile"] = rule.vpn_profile;
        r["source_type"] = rule.source_type;
        r["source_value"] = rule.source_value;
        r["destination"] = rule.destination;
        r["gateway"] = rule.gateway;
        r["protocol"] = rule.protocol;
        r["type"] = rule.type;
        r["priority"] = rule.priority;
        r["enabled"] = rule.enabled;
        r["log_traffic"] = rule.log_traffic;
        r["apply_to_existing"] = rule.apply_to_existing;
        r["description"] = rule.description;
        r["created_date"] = rule.created_date;
        r["last_modified"] = rule.last_modified;
        r["is_applied"] = rule.is_applied;
        r["is_automatic"] = rule.is_automatic;
        r["user_modified"] = rule.user_modified;
        rules.push_back(r);
    }
    
    return rules;
}

bool VPNInstanceManager::loadRoutingRules(const std::string& filepath) {
    try {
        routing_rules_file_path_ = filepath;
        
        std::ifstream file(filepath);
        if (!file.good()) {
            // Create empty file if it doesn't exist
            json empty;
            empty["routing_rules"] = json::array();
            std::ofstream out(filepath);
            out << empty.dump(2);
            out.close();
            return true;
        }
        
        json data;
        file >> data;
        file.close();
        
        if (!data.contains("routing_rules") || !data["routing_rules"].is_array()) {
            std::cerr << "Invalid routing rules file format" << std::endl;
            return false;
        }
        
        std::lock_guard<std::mutex> lock(routing_mutex_);
        
        for (const auto& rule_json : data["routing_rules"]) {
            RoutingRule rule;
            rule.id = rule_json.value("id", "");
            rule.name = rule_json.value("name", "");
            rule.vpn_instance = rule_json.value("vpn_instance", "");
            rule.vpn_profile = rule_json.value("vpn_profile", "");
            rule.source_type = rule_json.value("source_type", "Any");
            rule.source_value = rule_json.value("source_value", "");
            rule.destination = rule_json.value("destination", "");
            rule.gateway = rule_json.value("gateway", "VPN Server");
            rule.protocol = rule_json.value("protocol", "both");
            rule.type = rule_json.value("type", "tunnel_all");
            rule.priority = rule_json.value("priority", 100);
            rule.enabled = rule_json.value("enabled", true);
            rule.log_traffic = rule_json.value("log_traffic", false);
            rule.apply_to_existing = rule_json.value("apply_to_existing", false);
            rule.description = rule_json.value("description", "");
            rule.created_date = rule_json.value("created_date", "");
            rule.last_modified = rule_json.value("last_modified", "");
            rule.is_automatic = rule_json.value("is_automatic", false);
            rule.user_modified = rule_json.value("user_modified", false);
            rule.is_applied = false;  // Runtime state, always starts as false
            
            if (!rule.id.empty()) {
                routing_rules_[rule.id] = rule;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load routing rules: " << e.what() << std::endl;
        return false;
    }
}

bool VPNInstanceManager::saveRoutingRules(const std::string& filepath) {
    try {
        json data;
        json rules_array = json::array();
        
        std::lock_guard<std::mutex> lock(routing_mutex_);
        
        for (const auto& [id, rule] : routing_rules_) {
            json r;
            r["id"] = rule.id;
            r["name"] = rule.name;
            r["vpn_instance"] = rule.vpn_instance;
            r["vpn_profile"] = rule.vpn_profile;
            r["source_type"] = rule.source_type;
            r["source_value"] = rule.source_value;
            r["destination"] = rule.destination;
            r["gateway"] = rule.gateway;
            r["protocol"] = rule.protocol;
            r["type"] = rule.type;
            r["priority"] = rule.priority;
            r["enabled"] = rule.enabled;
            r["log_traffic"] = rule.log_traffic;
            r["apply_to_existing"] = rule.apply_to_existing;
            r["description"] = rule.description;
            r["created_date"] = rule.created_date;
            r["last_modified"] = rule.last_modified;
            r["is_automatic"] = rule.is_automatic;
            r["user_modified"] = rule.user_modified;
            // Don't save is_applied (runtime state)
            rules_array.push_back(r);
        }
        
        data["routing_rules"] = rules_array;
        
        std::ofstream file(filepath);
        file << data.dump(2);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save routing rules: " << e.what() << std::endl;
        return false;
    }
}

void VPNInstanceManager::applyRoutingRulesForInstance(const std::string& instance_name) {
    std::string interface = getInterfaceForInstance(instance_name);
    if (interface.empty()) {
        if (verbose_) {
            std::cout << json({
                {"type", "verbose"},
                {"message", "No interface found for instance"},
                {"instance", instance_name}
            }).dump() << std::endl;
        }
        return;
    }
    
    // Detect and save automatic routes first
    detectAndSaveAutomaticRoutes(instance_name, interface);
    
    std::lock_guard<std::mutex> lock(routing_mutex_);
    
    for (auto& [id, rule] : routing_rules_) {
        if (rule.vpn_instance == instance_name && rule.enabled && !rule.is_applied) {
            if (applyRoutingRule(rule, interface)) {
                rule.is_applied = true;
                if (verbose_) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "Applied routing rule"},
                        {"rule_id", rule.id},
                        {"rule_name", rule.name},
                        {"instance", instance_name}
                    }).dump() << std::endl;
                }
            }
        }
    }
}

void VPNInstanceManager::removeRoutingRulesForInstance(const std::string& instance_name) {
    std::string interface = getInterfaceForInstance(instance_name);
    if (interface.empty()) {
        interface = "tun0";  // Fallback for cleanup
    }
    
    std::lock_guard<std::mutex> lock(routing_mutex_);
    
    for (auto& [id, rule] : routing_rules_) {
        if (rule.vpn_instance == instance_name && rule.is_applied) {
            if (removeRoutingRule(rule, interface)) {
                rule.is_applied = false;
                if (verbose_) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "Removed routing rule"},
                        {"rule_id", rule.id},
                        {"rule_name", rule.name},
                        {"instance", instance_name}
                    }).dump() << std::endl;
                }
            }
        }
    }
}

bool VPNInstanceManager::applyRoutingRule(const RoutingRule& rule, const std::string& interface_name) {
    // Build ip route command based on rule type and destination CIDR
    std::string cmd;
    
    if (rule.type == "tunnel_all") {
        // Route all traffic to destination network through VPN interface
        cmd = "ip route add " + rule.destination + " dev " + interface_name;
        
        // Add source if specified
        if (rule.source_type == "IP Address" && !rule.source_value.empty()) {
            cmd += " src " + rule.source_value;
        }
        
        // Add metric based on priority (lower = higher priority)
        cmd += " metric " + std::to_string(rule.priority);
        
    } else if (rule.type == "tunnel_specific") {
        // Route specific destination through VPN with gateway if specified
        cmd = "ip route add " + rule.destination + " dev " + interface_name;
        
        if (rule.gateway != "VPN Server" && !rule.gateway.empty()) {
            cmd += " via " + rule.gateway;
        }
        
        cmd += " metric " + std::to_string(rule.priority);
        
    } else if (rule.type == "exclude") {
        // Exclude from VPN - route through original default gateway
        // Get default gateway for exclusion
        std::string default_gw_cmd = "ip route show default | awk '/default/ {print $3}' | head -n1";
        std::string default_gw = executeCommand(default_gw_cmd);
        
        if (!default_gw.empty()) {
            // Remove newline
            default_gw.erase(std::remove(default_gw.begin(), default_gw.end(), '\n'), default_gw.end());
            cmd = "ip route add " + rule.destination + " via " + default_gw;
            cmd += " metric " + std::to_string(rule.priority);
        } else {
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "Cannot exclude route - no default gateway found"},
                    {"rule_id", rule.id}
                }).dump() << std::endl;
            }
            return false;
        }
    }
    
    cmd += " 2>/dev/null || true";
    
    int result = system(cmd.c_str());
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Applied routing command"},
            {"command", cmd},
            {"result", result},
            {"rule_type", rule.type},
            {"destination", rule.destination}
        }).dump() << std::endl;
    }
    
    return result == 0;
}

bool VPNInstanceManager::removeRoutingRule(const RoutingRule& rule, const std::string& interface_name) {
    // Build ip route delete command
    std::string cmd = "ip route del " + rule.destination;
    
    if (rule.source_type == "IP Address" && !rule.source_value.empty()) {
        cmd += " src " + rule.source_value;
    }
    
    cmd += " 2>/dev/null || true";
    
    int result = system(cmd.c_str());
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Removed routing command"},
            {"command", cmd},
            {"result", result}
        }).dump() << std::endl;
    }
    
    return result == 0;
}

std::string VPNInstanceManager::getInterfaceForInstance(const std::string& instance_name) {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    
    auto it = instances_.find(instance_name);
    if (it == instances_.end()) {
        return "";
    }
    
    // First check if interface name is specified and cached
    if (!it->second.interface_name.empty()) {
        // Verify it still exists
        std::string verify_cmd = "ip link show " + it->second.interface_name + " 2>/dev/null";
        std::string verify_result = executeCommand(verify_cmd);
        if (!verify_result.empty()) {
            return it->second.interface_name;
        }
    }
    
    // Detect actual interface from system for this instance
    std::string detect_cmd;
    if (it->second.type == VPNType::WIREGUARD) {
        // Find all WireGuard interfaces
        detect_cmd = "ip link show type wireguard 2>/dev/null | grep -o '^[0-9]*: [^:@]*' | awk '{print $2}' | tr -d ':'";
    } else if (it->second.type == VPNType::OPENVPN) {
        // Find all tun/tap interfaces
        detect_cmd = "ip link show 2>/dev/null | grep -E '^[0-9]+: (tun|tap)[0-9]*' | grep -o '^[0-9]*: [^:@]*' | awk '{print $2}' | tr -d ':'";
    }
    
    if (!detect_cmd.empty()) {
        std::string detected = executeCommand(detect_cmd);
        if (!detected.empty()) {
            // Remove whitespace
            detected.erase(std::remove(detected.begin(), detected.end(), '\n'), detected.end());
            detected.erase(std::remove(detected.begin(), detected.end(), '\r'), detected.end());
            detected.erase(std::remove(detected.begin(), detected.end(), ' '), detected.end());
            
            if (!detected.empty()) {
                // Cache the detected interface
                it->second.interface_name = detected;
                
                if (verbose_) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "Detected and cached interface for instance"},
                        {"instance", instance_name},
                        {"interface", detected}
                    }).dump() << std::endl;
                }
                return detected;
            }
        }
    }
    
    // No interface found
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "No interface detected for instance"},
            {"instance", instance_name},
            {"vpn_type", vpnTypeToString(it->second.type)}
        }).dump() << std::endl;
    }
    
    return "";
}

// Start route monitoring thread
void VPNInstanceManager::startRouteMonitoring() {
    route_monitor_thread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            if (running_) {
                monitorRoutesForAllInstances();
            }
        }
    });
}

// Monitor routes for all connected instances
void VPNInstanceManager::monitorRoutesForAllInstances() {
    std::vector<std::pair<std::string, std::string>> connected_instances;
    
    // Get list of instances that have active interfaces
    {
        std::lock_guard<std::mutex> lock(instances_mutex_);
        for (const auto& [name, inst] : instances_) {
            // Monitor instances that are in active connection states
            bool should_monitor = false;
            
            if (inst.type == VPNType::OPENVPN) {
                // OpenVPN: monitor from AUTHENTICATING onwards (includes CONNECTED)
                should_monitor = (inst.current_state == ConnectionState::CONNECTED ||
                                inst.current_state == ConnectionState::AUTHENTICATING ||
                                inst.current_state == ConnectionState::CONNECTING);
            } else if (inst.type == VPNType::WIREGUARD) {
                // WireGuard: monitor when connected or connecting
                should_monitor = (inst.current_state == ConnectionState::CONNECTED ||
                                inst.current_state == ConnectionState::CONNECTING);
            }
            
            if (should_monitor) {
                std::string interface = getInterfaceForInstance(name);
                if (!interface.empty()) {
                    // Check if interface actually exists in system
                    std::string check_cmd = "ip link show " + interface + " 2>/dev/null";
                    std::string check_result = executeCommand(check_cmd);
                    
                    if (!check_result.empty()) {
                        connected_instances.push_back({name, interface});
                        
                        if (verbose_) {
                            std::cout << json({
                                {"type", "verbose"},
                                {"message", "Route monitoring - found active instance with interface"},
                                {"instance", name},
                                {"interface", interface},
                                {"state", static_cast<int>(inst.current_state)},
                                {"vpn_type", vpnTypeToString(inst.type)}
                            }).dump() << std::endl;
                        }
                    } else if (verbose_) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "Route monitoring - instance interface not yet available"},
                            {"instance", name},
                            {"interface", interface},
                            {"vpn_type", vpnTypeToString(inst.type)}
                        }).dump() << std::endl;
                    }
                } else if (verbose_) {
                    std::cout << json({
                        {"type", "verbose"},
                        {"message", "Route monitoring - active instance has no interface name"},
                        {"instance", name},
                        {"state", static_cast<int>(inst.current_state)},
                        {"vpn_type", vpnTypeToString(inst.type)}
                    }).dump() << std::endl;
                }
            }
        }
    }
    
    // Monitor each connected instance
    for (const auto& [instance_name, interface] : connected_instances) {
        // Get current route output
        std::string route_output = executeCommand("route -n");
        
        if (route_output.empty()) {
            continue;
        }
        
        // Hash the route output for this interface
        std::string current_hash = hashString(route_output);
        
        // Check if routes have changed
        bool routes_changed = false;
        auto it = last_route_snapshots_.find(instance_name);
        
        if (it == last_route_snapshots_.end()) {
            // First time monitoring this instance
            last_route_snapshots_[instance_name] = current_hash;
            routes_changed = true;
            
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "Initial route snapshot captured"},
                    {"instance", instance_name}
                }).dump() << std::endl;
            }
        } else if (it->second != current_hash) {
            // Routes have changed
            routes_changed = true;
            last_route_snapshots_[instance_name] = current_hash;
            
            if (verbose_) {
                std::cout << json({
                    {"type", "verbose"},
                    {"message", "Route changes detected"},
                    {"instance", instance_name}
                }).dump() << std::endl;
            }
        }
        
        // If routes changed, detect and save them
        if (routes_changed) {
            std::vector<RoutingRule> detected_rules = parseRouteOutput(route_output, instance_name);
            
            if (!detected_rules.empty()) {
                mergeAutomaticRoutes(detected_rules, instance_name);
                
                // Save to routing rules file
                if (!routing_rules_file_path_.empty()) {
                    saveRoutingRules(routing_rules_file_path_);
                    
                    if (verbose_) {
                        std::cout << json({
                            {"type", "verbose"},
                            {"message", "Updated routing rules saved"},
                            {"instance", instance_name},
                            {"rules_count", detected_rules.size()},
                            {"file", routing_rules_file_path_}
                        }).dump() << std::endl;
                    }
                }
            }
        }
    }
}

bool VPNInstanceManager::initializeRoutingForInstance(const std::string& instance_name) {
    auto it = instances_.find(instance_name);
    if (it == instances_.end()) {
        return false;
    }
    
    auto& inst = it->second;
    
    if (inst.type == VPNType::WIREGUARD) {
        auto wg_wrapper = std::static_pointer_cast<wireguard::WireGuardWrapper>(inst.wrapper_instance);
        inst.routing_provider = std::make_unique<WireGuardRoutingProvider>(wg_wrapper.get());
    } else if (inst.type == VPNType::OPENVPN) {
        auto ovpn_wrapper = std::static_pointer_cast<openvpn::OpenVPNWrapper>(inst.wrapper_instance);
        inst.routing_provider = std::make_unique<OpenVPNRoutingProvider>(ovpn_wrapper.get());
    } else {
        return false;
    }
    
    inst.routing_provider->setEventCallback(
        [this, instance_name](RouteEventType event_type, 
                             const UnifiedRouteRule& rule,
                             const std::string& error_msg) {
            handleRoutingEvent(instance_name, event_type, rule, error_msg);
        }
    );
    
    std::string interface_name = getInterfaceForInstance(instance_name);
    if (!inst.routing_provider->initialize(interface_name)) {
        inst.routing_provider.reset();
        return false;
    }
    
    inst.routing_initialized = true;
    
    loadRoutingRulesForInstance(instance_name);
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Routing provider initialized for instance"},
            {"instance", instance_name},
            {"interface", interface_name}
        }).dump() << std::endl;
    }
    
    return true;
}

void VPNInstanceManager::handleRoutingEvent(
    const std::string& instance_name,
    RouteEventType event_type,
    const UnifiedRouteRule& rule,
    const std::string& error_msg) {
    
    json event_json;
    event_json["type"] = "routing_event";
    event_json["instance"] = instance_name;
    event_json["event_type"] = routeEventTypeToString(event_type);
    event_json["rule"] = rule.to_json();
    
    if (!error_msg.empty()) {
        event_json["error"] = error_msg;
    }
    
    std::cout << event_json.dump() << std::endl;
    
    if (event_type == RouteEventType::DETECTED && rule.is_automatic) {
        saveRoutingRulesForInstance(instance_name);
    }
}

bool VPNInstanceManager::loadRoutingRulesForInstance(const std::string& instance_name) {
    if (routing_config_dir_.empty()) {
        routing_config_dir_ = "config/vpn-configs";
    }
    
    mkdir(routing_config_dir_.c_str(), 0755);
    
    std::string filename = routing_config_dir_ + "/" + instance_name + "-routes.json";
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
        return false;
    }
    
    std::ifstream file(filename);
    if (!file.good()) {
        return true;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    bool result = inst_it->second.routing_provider->importRulesJson(content);
    
    if (verbose_ && result) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Loaded routing rules for instance"},
            {"instance", instance_name},
            {"file", filename}
        }).dump() << std::endl;
    }
    
    return result;
}

bool VPNInstanceManager::saveRoutingRulesForInstance(const std::string& instance_name) {
    if (routing_config_dir_.empty()) {
        routing_config_dir_ = "config/vpn-configs";
    }
    
    mkdir(routing_config_dir_.c_str(), 0755);
    
    std::string filename = routing_config_dir_ + "/" + instance_name + "-routes.json";
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
        return false;
    }
    
    std::string json_content = inst_it->second.routing_provider->exportRulesJson();
    
    std::ofstream file(filename);
    if (!file.good()) {
        return false;
    }
    
    file << json_content;
    file.close();
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Saved routing rules for instance"},
            {"instance", instance_name},
            {"file", filename}
        }).dump() << std::endl;
    }
    
    return true;
}

bool VPNInstanceManager::migrateRoutingRules() {
    std::string old_file = "config/routing-rules.json";
    
    std::ifstream file(old_file);
    if (!file.good()) {
        return true;
    }
    
    json old_data;
    file >> old_data;
    file.close();
    
    if (!old_data.contains("routing_rules")) {
        return false;
    }
    
    std::map<std::string, std::vector<UnifiedRouteRule>> instance_rules;
    
    for (const auto& rule_json : old_data["routing_rules"]) {
        std::string instance_name = rule_json.value("vpn_instance", "");
        if (instance_name.empty()) continue;
        
        UnifiedRouteRule rule = UnifiedRouteRule::from_json(rule_json);
        instance_rules[instance_name].push_back(rule);
    }
    
    for (const auto& [instance_name, rules] : instance_rules) {
        auto inst_it = instances_.find(instance_name);
        if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
            continue;
        }
        
        for (const auto& rule : rules) {
            inst_it->second.routing_provider->addRule(rule);
        }
        
        saveRoutingRulesForInstance(instance_name);
    }
    
    std::rename(old_file.c_str(), (old_file + ".backup").c_str());
    
    if (verbose_) {
        std::cout << json({
            {"type", "verbose"},
            {"message", "Migrated routing rules to per-instance format"},
            {"instances_migrated", instance_rules.size()},
            {"backup_file", old_file + ".backup"}
        }).dump() << std::endl;
    }
    
    return true;
}

json VPNInstanceManager::getInstanceRoutes(const std::string& instance_name) {
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end()) {
        return {{"error", "Instance not found"}};
    }
    
    if (!inst_it->second.routing_provider) {
        return {{"error", "Routing not initialized for instance"}};
    }
    
    auto rules = inst_it->second.routing_provider->getAllRules();
    json rules_json = json::array();
    for (const auto& rule : rules) {
        rules_json.push_back(rule.to_json());
    }
    
    return rules_json;
}

bool VPNInstanceManager::addInstanceRoute(const std::string& instance_name, const UnifiedRouteRule& rule) {
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end()) {
        return false;
    }
    
    if (!inst_it->second.routing_provider) {
        return false;
    }
    
    bool success = inst_it->second.routing_provider->addRule(rule);
    if (success) {
        saveRoutingRulesForInstance(instance_name);
    }
    
    return success;
}

bool VPNInstanceManager::deleteInstanceRoute(const std::string& instance_name, const std::string& rule_id) {
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end()) {
        return false;
    }
    
    if (!inst_it->second.routing_provider) {
        return false;
    }
    
    bool success = inst_it->second.routing_provider->removeRule(rule_id);
    if (success) {
        saveRoutingRulesForInstance(instance_name);
    }
    
    return success;
}

bool VPNInstanceManager::applyInstanceRoutes(const std::string& instance_name) {
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end()) {
        return false;
    }
    
    if (!inst_it->second.routing_provider) {
        return false;
    }
    
    return inst_it->second.routing_provider->applyRules();
}

int VPNInstanceManager::detectInstanceRoutes(const std::string& instance_name) {
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end()) {
        return -1;
    }
    
    if (!inst_it->second.routing_provider) {
        return -1;
    }
    
    int detected = inst_it->second.routing_provider->detectRoutes();
    if (detected >= 0) {
        saveRoutingRulesForInstance(instance_name);
    }
    
    return detected;
}


} // namespace vpn_manager
