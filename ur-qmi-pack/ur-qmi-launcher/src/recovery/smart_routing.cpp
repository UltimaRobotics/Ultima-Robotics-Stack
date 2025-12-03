#include "recovery/smart_routing.h"
#include "utils/command_logger.h"
#include "utils/timeout_config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <regex>
#include <cstdlib>

using json = nlohmann::json;

// Global smart routing manager instance
SmartRoutingManager g_smart_routing;

// SmartRoutingConfig implementation
bool SmartRoutingConfig::loadFromFile(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open routing config file: " << config_file 
                  << ". Using default routing configuration." << std::endl;
        return false;
    }
    
    try {
        json root = json::parse(file);
        
        // Load basic routing configuration
        if (root.contains("auto_routing_enabled")) {
            auto_routing_enabled = root["auto_routing_enabled"].get<bool>();
        }
        if (root.contains("manual_routing_enabled")) {
            manual_routing_enabled = root["manual_routing_enabled"].get<bool>();
        }
        if (root.contains("backup_existing_routes")) {
            backup_existing_routes = root["backup_existing_routes"].get<bool>();
        }
        
        // Load cellular interface configuration
        if (root.contains("cellular_interface")) {
            cellular_interface = root["cellular_interface"].get<std::string>();
        }
        if (root.contains("cellular_default_metric")) {
            cellular_default_metric = root["cellular_default_metric"].get<int>();
        }
        if (root.contains("cellular_priority_level")) {
            cellular_priority_level = root["cellular_priority_level"].get<int>();
        }
        if (root.contains("set_cellular_as_default")) {
            set_cellular_as_default = root["set_cellular_as_default"].get<bool>();
        }
        if (root.contains("coexist_with_other_interfaces")) {
            coexist_with_other_interfaces = root["coexist_with_other_interfaces"].get<bool>();
        }
        
        // Load interface priorities
        if (root.contains("interface_priorities") && root["interface_priorities"].is_object()) {
            const json& priorities = root["interface_priorities"];
            for (auto it = priorities.begin(); it != priorities.end(); ++it) {
                interface_priorities[it.key()] = it.value().get<int>();
            }
        }
        
        // Load manual routing rules
        if (root.contains("manual_rules") && root["manual_rules"].is_array()) {
            const json& rules = root["manual_rules"];
            for (const auto& rule_json : rules) {
                RoutingRule rule;
                if (rule_json.contains("destination")) rule.destination = rule_json["destination"].get<std::string>();
                if (rule_json.contains("gateway")) rule.gateway = rule_json["gateway"].get<std::string>();
                if (rule_json.contains("interface")) rule.interface = rule_json["interface"].get<std::string>();
                if (rule_json.contains("metric")) rule.metric = rule_json["metric"].get<int>();
                if (rule_json.contains("table")) rule.table = rule_json["table"].get<int>();
                if (rule_json.contains("source")) rule.source = rule_json["source"].get<std::string>();
                if (rule_json.contains("persistent")) rule.persistent = rule_json["persistent"].get<bool>();
                if (rule_json.contains("description")) rule.description = rule_json["description"].get<std::string>();
                manual_rules.push_back(rule);
            }
        }
        
        // Load routing policies
        if (root.contains("preserve_local_routes")) {
            preserve_local_routes = root["preserve_local_routes"].get<bool>();
        }
        if (root.contains("preserve_vpn_routes")) {
            preserve_vpn_routes = root["preserve_vpn_routes"].get<bool>();
        }
        
        // Load protected interfaces
        if (root.contains("protected_interfaces") && root["protected_interfaces"].is_array()) {
            for (const auto& iface : root["protected_interfaces"]) {
                protected_interfaces.push_back(iface.get<std::string>());
            }
        }
        
        // Load priority destinations
        if (root.contains("priority_destinations") && root["priority_destinations"].is_array()) {
            for (const auto& dest : root["priority_destinations"]) {
                priority_destinations.push_back(dest.get<std::string>());
            }
        }
        
        // Load failover configuration
        if (root.contains("enable_failover")) {
            enable_failover = root["enable_failover"].get<bool>();
        }
        if (root.contains("primary_interface")) {
            primary_interface = root["primary_interface"].get<std::string>();
        }
        if (root.contains("backup_interface")) {
            backup_interface = root["backup_interface"].get<std::string>();
        }
        if (root.contains("failover_timeout_ms")) {
            failover_timeout_ms = root["failover_timeout_ms"].get<int>();
        }
        
        std::cout << "Smart routing configuration loaded from: " << config_file << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading routing configuration: " << e.what() << std::endl;
        return false;
    }
}

bool SmartRoutingConfig::saveToFile(const std::string& config_file) const {
    json root;
    
    // Save basic configuration
    root["auto_routing_enabled"] = auto_routing_enabled;
    root["manual_routing_enabled"] = manual_routing_enabled;
    root["backup_existing_routes"] = backup_existing_routes;
    
    // Save cellular interface configuration
    root["cellular_interface"] = cellular_interface;
    root["cellular_default_metric"] = cellular_default_metric;
    root["cellular_priority_level"] = cellular_priority_level;
    root["set_cellular_as_default"] = set_cellular_as_default;
    root["coexist_with_other_interfaces"] = coexist_with_other_interfaces;
    
    // Save interface priorities
    json priorities;
    for (const auto& pair : interface_priorities) {
        priorities[pair.first] = pair.second;
    }
    root["interface_priorities"] = priorities;
    
    // Save manual routing rules
    json rules = json::array();
    for (const auto& rule : manual_rules) {
        json rule_json;
        rule_json["destination"] = rule.destination;
        rule_json["gateway"] = rule.gateway;
        rule_json["interface"] = rule.interface;
        rule_json["metric"] = rule.metric;
        rule_json["table"] = rule.table;
        rule_json["source"] = rule.source;
        rule_json["persistent"] = rule.persistent;
        rule_json["description"] = rule.description;
        rules.push_back(rule_json);
    }
    root["manual_rules"] = rules;
    
    // Save routing policies
    root["preserve_local_routes"] = preserve_local_routes;
    root["preserve_vpn_routes"] = preserve_vpn_routes;
    
    // Save protected interfaces
    json protected_ifaces = json::array();
    for (const auto& iface : protected_interfaces) {
        protected_ifaces.push_back(iface);
    }
    root["protected_interfaces"] = protected_ifaces;
    
    // Save priority destinations
    json priority_dests = json::array();
    for (const auto& dest : priority_destinations) {
        priority_dests.push_back(dest);
    }
    root["priority_destinations"] = priority_dests;
    
    // Save failover configuration
    root["enable_failover"] = enable_failover;
    root["primary_interface"] = primary_interface;
    root["backup_interface"] = backup_interface;
    root["failover_timeout_ms"] = failover_timeout_ms;
    
    // Add metadata
    root["description"] = "QMI Connection Manager Smart Routing Configuration";
    root["version"] = "1.0";
    
    std::ofstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not create routing config file: " << config_file << std::endl;
        return false;
    }
    
    file << root.dump(2);
    
    std::cout << "Smart routing configuration saved to: " << config_file << std::endl;
    return true;
}

bool SmartRoutingConfig::validate() const {
    bool valid = true;
    
    // Validate priority levels
    if (cellular_priority_level < 1 || cellular_priority_level > 10) {
        std::cerr << "Warning: Cellular priority level should be between 1-10" << std::endl;
        valid = false;
    }
    
    // Validate metric values
    if (cellular_default_metric < 1 || cellular_default_metric > 9999) {
        std::cerr << "Warning: Cellular metric should be between 1-9999" << std::endl;
        valid = false;
    }
    
    // Validate interface names
    if (!cellular_interface.empty() && cellular_interface.length() > 15) {
        std::cerr << "Warning: Interface name too long: " << cellular_interface << std::endl;
        valid = false;
    }
    
    // Validate manual rules
    for (const auto& rule : manual_rules) {
        if (rule.gateway.empty() && rule.interface.empty()) {
            std::cerr << "Warning: Manual rule missing both gateway and interface" << std::endl;
            valid = false;
        }
    }
    
    return valid;
}

void SmartRoutingConfig::printConfiguration() const {
    std::cout << "\n=== Smart Routing Configuration ===" << std::endl;
    
    std::cout << "\nBasic Configuration:" << std::endl;
    std::cout << "  Auto routing enabled: " << (auto_routing_enabled ? "Yes" : "No") << std::endl;
    std::cout << "  Manual routing enabled: " << (manual_routing_enabled ? "Yes" : "No") << std::endl;
    std::cout << "  Backup existing routes: " << (backup_existing_routes ? "Yes" : "No") << std::endl;
    
    std::cout << "\nCellular Interface Configuration:" << std::endl;
    std::cout << "  Interface: " << cellular_interface << std::endl;
    std::cout << "  Default metric: " << cellular_default_metric << std::endl;
    std::cout << "  Priority level: " << cellular_priority_level << std::endl;
    std::cout << "  Set as default route: " << (set_cellular_as_default ? "Yes" : "No") << std::endl;
    std::cout << "  Coexist with other interfaces: " << (coexist_with_other_interfaces ? "Yes" : "No") << std::endl;
    
    if (!interface_priorities.empty()) {
        std::cout << "\nInterface Priorities:" << std::endl;
        for (const auto& pair : interface_priorities) {
            std::cout << "  " << pair.first << ": " << pair.second << std::endl;
        }
    }
    
    if (!manual_rules.empty()) {
        std::cout << "\nManual Routing Rules:" << std::endl;
        for (size_t i = 0; i < manual_rules.size(); ++i) {
            const auto& rule = manual_rules[i];
            std::cout << "  Rule " << i + 1 << ":" << std::endl;
            std::cout << "    Destination: " << rule.destination << std::endl;
            std::cout << "    Gateway: " << rule.gateway << std::endl;
            std::cout << "    Interface: " << rule.interface << std::endl;
            std::cout << "    Metric: " << rule.metric << std::endl;
            if (!rule.description.empty()) {
                std::cout << "    Description: " << rule.description << std::endl;
            }
        }
    }
    
    std::cout << "\nFailover Configuration:" << std::endl;
    std::cout << "  Failover enabled: " << (enable_failover ? "Yes" : "No") << std::endl;
    std::cout << "  Primary interface: " << primary_interface << std::endl;
    std::cout << "  Backup interface: " << backup_interface << std::endl;
    std::cout << "  Failover timeout: " << failover_timeout_ms << "ms" << std::endl;
    
    std::cout << "=====================================\n" << std::endl;
}

// SmartRoutingManager implementation
SmartRoutingManager::SmartRoutingManager() : m_initialized(false) {
}

SmartRoutingManager::~SmartRoutingManager() {
}

bool SmartRoutingManager::initialize(const SmartRoutingConfig& config) {
    m_config = config;
    
    if (!m_config.validate()) {
        std::cerr << "Warning: Smart routing configuration has validation issues" << std::endl;
    }
    
    // Backup existing routes if requested
    if (m_config.backup_existing_routes) {
        if (!backupRoutes()) {
            std::cerr << "Warning: Failed to backup existing routes" << std::endl;
        }
    }
    
    m_initialized = true;
    std::cout << "Smart routing manager initialized" << std::endl;
    return true;
}

void SmartRoutingManager::setRoutingChangeCallback(const RoutingChangeCallback& callback) {
    m_routing_callback = callback;
}

bool SmartRoutingManager::applyCellularRouting(const std::string& interface_name, 
                                              const std::string& gateway_ip, 
                                              const std::string& local_ip) {
    if (!m_initialized) {
        std::cerr << "Smart routing manager not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Applying cellular routing for interface: " << interface_name << std::endl;
    
    bool success = true;
    
    // Calculate metric based on priority
    int metric = calculateMetric(interface_name, m_config.cellular_priority_level);
    
    if (m_config.set_cellular_as_default) {
        // Add default route through cellular interface
        RoutingRule default_rule;
        default_rule.destination = "0.0.0.0/0";
        default_rule.gateway = gateway_ip;
        default_rule.interface = interface_name;
        default_rule.metric = metric;
        default_rule.description = "Cellular default route";
        
        if (!addRoutingRule(default_rule)) {
            std::cerr << "Failed to add cellular default route" << std::endl;
            success = false;
        }
    }
    
    // Add interface-specific route for local subnet
    if (!local_ip.empty()) {
        // Extract network from local IP (assume /24 for cellular)
        std::string network = local_ip.substr(0, local_ip.find_last_of('.')) + ".0/24";
        
        RoutingRule local_rule;
        local_rule.destination = network;
        local_rule.interface = interface_name;
        local_rule.metric = metric - 10; // Higher priority for local traffic
        local_rule.description = "Cellular local network route";
        
        if (!addRoutingRule(local_rule)) {
            std::cerr << "Failed to add cellular local route" << std::endl;
            success = false;
        }
    }
    
    // Apply manual rules for cellular interface
    if (m_config.manual_routing_enabled) {
        for (const auto& rule : m_config.manual_rules) {
            if (rule.interface == interface_name || rule.interface.empty()) {
                RoutingRule manual_rule = rule;
                if (manual_rule.interface.empty()) {
                    manual_rule.interface = interface_name;
                }
                if (manual_rule.gateway.empty()) {
                    manual_rule.gateway = gateway_ip;
                }
                
                if (!addRoutingRule(manual_rule)) {
                    std::cerr << "Failed to add manual routing rule" << std::endl;
                    success = false;
                }
            }
        }
    }
    
    // Update interface priorities if needed
    if (m_config.interface_priorities.find(interface_name) == m_config.interface_priorities.end()) {
        setInterfacePriority(interface_name, m_config.cellular_priority_level);
    }
    
    std::cout << "Cellular routing " << (success ? "applied successfully" : "failed") << std::endl;
    return success;
}

bool SmartRoutingManager::removeCellularRouting(const std::string& interface_name) {
    if (!m_initialized) {
        std::cerr << "Smart routing manager not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Removing cellular routing for interface: " << interface_name << std::endl;
    
    // Get current routes
    std::vector<RoutingRule> current_routes = getCurrentRoutes();
    bool success = true;
    
    // Remove all routes for this interface
    for (const auto& route : current_routes) {
        if (route.interface == interface_name) {
            if (!removeRoutingRule(route)) {
                std::cerr << "Failed to remove route for interface: " << interface_name << std::endl;
                success = false;
            }
        }
    }
    
    std::cout << "Cellular routing removal " << (success ? "completed successfully" : "failed") << std::endl;
    return success;
}

bool SmartRoutingManager::addRoutingRule(const RoutingRule& rule) {
    if (!validateRoutingRule(rule)) {
        std::cerr << "Invalid routing rule" << std::endl;
        return false;
    }
    
    // Check if interface is protected
    if (isProtectedInterface(rule.interface)) {
        std::cerr << "Cannot modify protected interface: " << rule.interface << std::endl;
        return false;
    }
    
    std::string command = buildRouteCommand(RoutingOperation::ADD_INTERFACE_ROUTE, rule);
    bool success = executeRoutingCommand(command);
    
    notifyRoutingChange(RoutingOperation::ADD_INTERFACE_ROUTE, rule, success);
    return success;
}

bool SmartRoutingManager::removeRoutingRule(const RoutingRule& rule) {
    std::string command = buildRouteCommand(RoutingOperation::REMOVE_INTERFACE_ROUTE, rule);
    bool success = executeRoutingCommand(command);
    
    notifyRoutingChange(RoutingOperation::REMOVE_INTERFACE_ROUTE, rule, success);
    return success;
}

bool SmartRoutingManager::executeRoutingCommand(const std::string& command) {
    CommandLogger::logCommand(command);
    int result = system(command.c_str());
    int exit_code = WEXITSTATUS(result);
    
    CommandLogger::logCommandResult(command, (exit_code == 0) ? "SUCCESS" : "FAILED", exit_code);
    return exit_code == 0;
}

std::string SmartRoutingManager::buildRouteCommand(RoutingOperation operation, const RoutingRule& rule) {
    std::stringstream cmd;
    
    switch (operation) {
        case RoutingOperation::ADD_INTERFACE_ROUTE:
        case RoutingOperation::ADD_DEFAULT_ROUTE:
            cmd << "ip route add " << rule.destination;
            if (!rule.gateway.empty()) {
                cmd << " via " << rule.gateway;
            }
            if (!rule.interface.empty()) {
                cmd << " dev " << rule.interface;
            }
            if (rule.metric > 0) {
                cmd << " metric " << rule.metric;
            }
            if (!rule.source.empty()) {
                cmd << " src " << rule.source;
            }
            if (rule.table > 0) {
                cmd << " table " << rule.table;
            }
            break;
            
        case RoutingOperation::REMOVE_INTERFACE_ROUTE:
        case RoutingOperation::REMOVE_DEFAULT_ROUTE:
            cmd << "ip route del " << rule.destination;
            if (!rule.gateway.empty()) {
                cmd << " via " << rule.gateway;
            }
            if (!rule.interface.empty()) {
                cmd << " dev " << rule.interface;
            }
            break;
            
        case RoutingOperation::SET_INTERFACE_METRIC:
            // This would require getting current route and modifying it
            cmd << "ip route change " << rule.destination << " dev " << rule.interface 
                << " metric " << rule.metric;
            break;
            
        default:
            std::cerr << "Unsupported routing operation" << std::endl;
            return "";
    }
    
    cmd << " 2>/dev/null"; // Suppress errors for cleaner output
    return cmd.str();
}

std::vector<RoutingRule> SmartRoutingManager::getCurrentRoutes() {
    std::vector<RoutingRule> routes;
    
    std::string command = "ip route show";
    CommandLogger::logCommand(command);
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to execute route command" << std::endl;
        return routes;
    }
    
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int exit_code = pclose(pipe);
    CommandLogger::logCommandResult(command, output, WEXITSTATUS(exit_code));
    
    parseRouteOutput(output, routes);
    return routes;
}

bool SmartRoutingManager::parseRouteOutput(const std::string& output, std::vector<RoutingRule>& routes) {
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        RoutingRule rule;
        
        // Parse route line (simplified parsing)
        std::regex route_regex(R"(^(\S+)\s+via\s+(\S+)\s+dev\s+(\S+)(?:\s+metric\s+(\d+))?(?:\s+src\s+(\S+))?.*$)");
        std::smatch match;
        
        if (std::regex_match(line, match, route_regex)) {
            rule.destination = match[1].str();
            rule.gateway = match[2].str();
            rule.interface = match[3].str();
            if (match[4].matched) {
                rule.metric = std::stoi(match[4].str());
            }
            if (match[5].matched) {
                rule.source = match[5].str();
            }
            routes.push_back(rule);
        } else {
            // Try simpler pattern for direct interface routes
            std::regex direct_regex(R"(^(\S+)\s+dev\s+(\S+)(?:\s+metric\s+(\d+))?.*$)");
            if (std::regex_match(line, match, direct_regex)) {
                rule.destination = match[1].str();
                rule.interface = match[2].str();
                if (match[3].matched) {
                    rule.metric = std::stoi(match[3].str());
                }
                routes.push_back(rule);
            }
        }
    }
    
    return true;
}

int SmartRoutingManager::calculateMetric(const std::string& interface_name, int base_priority) {
    // Base metric calculation: priority level * 100 + interface-specific offset
    int base_metric = base_priority * 100;
    
    // Check if interface has specific priority configured
    auto it = m_config.interface_priorities.find(interface_name);
    if (it != m_config.interface_priorities.end()) {
        base_metric = it->second * 100;
    }
    
    // Add interface-specific offset based on type
    if (interface_name.find("wwan") != std::string::npos || 
        interface_name.find("cellular") != std::string::npos) {
        base_metric += 10; // Cellular interfaces
    } else if (interface_name.find("eth") != std::string::npos) {
        base_metric += 5;  // Ethernet interfaces
    } else if (interface_name.find("wlan") != std::string::npos || 
               interface_name.find("wifi") != std::string::npos) {
        base_metric += 20; // WiFi interfaces
    }
    
    return base_metric;
}

bool SmartRoutingManager::validateRoutingRule(const RoutingRule& rule) const {
    // Basic validation
    if (rule.destination.empty()) {
        std::cerr << "Route destination cannot be empty" << std::endl;
        return false;
    }
    
    if (rule.gateway.empty() && rule.interface.empty()) {
        std::cerr << "Route must have either gateway or interface specified" << std::endl;
        return false;
    }
    
    if (rule.metric < 0 || rule.metric > 9999) {
        std::cerr << "Route metric must be between 0-9999" << std::endl;
        return false;
    }
    
    return true;
}

bool SmartRoutingManager::setInterfacePriority(const std::string& interface_name, int priority) {
    if (priority < 1 || priority > 10) {
        std::cerr << "Priority must be between 1-10" << std::endl;
        return false;
    }
    
    // Update configuration
    SmartRoutingConfig* config = const_cast<SmartRoutingConfig*>(&m_config);
    config->interface_priorities[interface_name] = priority;
    
    std::cout << "Set priority " << priority << " for interface: " << interface_name << std::endl;
    return true;
}

bool SmartRoutingManager::backupRoutes() {
    m_backup_routes = getCurrentRoutes();
    std::cout << "Backed up " << m_backup_routes.size() << " routing rules" << std::endl;
    return !m_backup_routes.empty();
}

bool SmartRoutingManager::restoreRoutes() {
    if (m_backup_routes.empty()) {
        std::cerr << "No backup routes available" << std::endl;
        return false;
    }
    
    std::cout << "Restoring " << m_backup_routes.size() << " routing rules" << std::endl;
    
    bool success = true;
    for (const auto& rule : m_backup_routes) {
        if (!addRoutingRule(rule)) {
            success = false;
        }
    }
    
    return success;
}

bool SmartRoutingManager::isProtectedInterface(const std::string& interface_name) const {
    return std::find(m_config.protected_interfaces.begin(), 
                     m_config.protected_interfaces.end(), 
                     interface_name) != m_config.protected_interfaces.end();
}

void SmartRoutingManager::notifyRoutingChange(RoutingOperation operation, const RoutingRule& rule, bool success, const std::string& error) {
    if (m_routing_callback) {
        m_routing_callback(operation, rule, success, error);
    }
}

const SmartRoutingConfig& SmartRoutingManager::getConfiguration() const {
    return m_config;
}

void SmartRoutingManager::setAutoRoutingEnabled(bool enabled) {
    SmartRoutingConfig* config = const_cast<SmartRoutingConfig*>(&m_config);
    config->auto_routing_enabled = enabled;
    std::cout << "Auto routing " << (enabled ? "enabled" : "disabled") << std::endl;
}