#include "../include/NetworkConfigAPI.h"
#include "../include/Utils.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <algorithm>

namespace OpenWrtNetwork {

NetworkConfigAPI::NetworkConfigAPI() : configFilePath("/etc/config/network") {
    // Set default hardcoded paths (will be overridden by package config if loaded)
    networkConfigPath = "/etc/config/network";
    firewallConfigPath = "/etc/config/firewall";
    staticRouteConfigPath = "/etc/config/network";
    
    // Initialize default package config
    packageConfig.networkConfigPath = "/etc/config/network";
    packageConfig.firewallConfigPath = "/etc/config/firewall";
    packageConfig.staticRouteConfigPath = "/etc/config/network";
    packageConfig.resolvConfPath = "/etc/resolv.conf";
    packageConfig.networkProfilesDir = "/etc/Ultima-Config/ur-base-network-mann/network-profiles";
    packageConfig.networkBackupsDir = "/etc/Ultima-Config/ur-base-network-mann/network-backups";
    packageConfig.defaultInterface = "eth0";
    packageConfig.defaultConnectionMode = "dhcp";
    packageConfig.defaultMtuSize = 1500;
}

NetworkConfigAPI::~NetworkConfigAPI() {
}

bool NetworkConfigAPI::initialize() {
    // Use package config paths if available, otherwise use defaults
    if (packageConfigLoaded) {
        configFilePath = packageConfig.networkConfigPath;
        networkConfigPath = packageConfig.networkConfigPath;
        firewallConfigPath = packageConfig.firewallConfigPath;
        staticRouteConfigPath = packageConfig.staticRouteConfigPath;
    }
    
    if (!Utils::fileExists(configFilePath)) {
        std::cerr << "Network config file not found: " << configFilePath << std::endl;
        return false;
    }
    return loadNetworkConfig();
}

bool NetworkConfigAPI::loadNetworkConfig() {
    return parseConfigFile();
}

bool NetworkConfigAPI::loadPackageConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open package config file: " << configPath << std::endl;
        return false;
    }
    
    std::string jsonContent((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();
    
    if (parseJsonConfig(jsonContent, packageConfig)) {
        packageConfigLoaded = true;
        std::cout << "Package configuration loaded from: " << configPath << std::endl;
        return true;
    } else {
        std::cerr << "Failed to parse package config file: " << configPath << std::endl;
        return false;
    }
}

bool NetworkConfigAPI::loadOperationConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open operation config file: " << configPath << std::endl;
        return false;
    }
    
    std::string jsonContent((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();
    
    if (parseOperationJsonConfig(jsonContent, operationConfig)) {
        operationConfigLoaded = true;
        std::cout << "Operation configuration loaded from: " << configPath << std::endl;
        return true;
    } else {
        std::cerr << "Failed to parse operation config file: " << configPath << std::endl;
        return false;
    }
}

bool NetworkConfigAPI::executeOperationConfig() {
    if (!operationConfigLoaded) {
        std::cerr << "No operation configuration loaded" << std::endl;
        return false;
    }
    
    std::cout << "Executing operations from configuration..." << std::endl;
    bool success = true;
    
    // Network operations
    if (operationConfig.setConnectionModeOp) {
        std::cout << "Setting connection mode to: " << operationConfig.connectionMode << std::endl;
        if (!setConnectionMode(operationConfig.connectionMode)) {
            std::cerr << "Failed to set connection mode" << std::endl;
            success = false;
        }
    }
    
    if (operationConfig.setStaticIpOp) {
        std::cout << "Setting static IP: " << operationConfig.staticIp << std::endl;
        if (!setStaticIP(operationConfig.staticIp, operationConfig.subnetMask, operationConfig.defaultGateway)) {
            std::cerr << "Failed to set static IP" << std::endl;
            success = false;
        }
    }
    
    if (operationConfig.setDnsOp && !operationConfig.dnsServers.empty()) {
        std::cout << "Setting DNS servers: ";
        for (size_t i = 0; i < operationConfig.dnsServers.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << operationConfig.dnsServers[i];
        }
        std::cout << std::endl;
        if (!setDnsServers(operationConfig.dnsServers)) {
            std::cerr << "Failed to set DNS servers" << std::endl;
            success = false;
        }
    }
    
    if (operationConfig.setMtuOp) {
        std::cout << "Setting MTU size to: " << operationConfig.mtuSize << std::endl;
        if (!setMtuSize(operationConfig.mtuSize)) {
            std::cerr << "Failed to set MTU size" << std::endl;
            success = false;
        }
    }
    
    // Static routes operations
    if (operationConfig.addStaticRoutesOp) {
        for (const auto& route : operationConfig.staticRoutes) {
            std::cout << "Adding static route: " << route.target << " via " << route.gateway << std::endl;
            if (!addStaticRoute(route)) {
                std::cerr << "Failed to add static route: " << route.target << std::endl;
                success = false;
            }
        }
    }
    
    if (operationConfig.removeStaticRoutesOp) {
        for (const auto& route : operationConfig.staticRoutes) {
            std::cout << "Removing static route: " << route.target << std::endl;
            if (!removeStaticRoute(route.target)) {
                std::cerr << "Failed to remove static route: " << route.target << std::endl;
                success = false;
            }
        }
    }
    
    // VLAN operations
    if (operationConfig.addVlansOp) {
        for (const auto& vlan : operationConfig.vlans) {
            std::cout << "Adding VLAN: " << vlan.name << " (ID: " << vlan.vlanId << ")" << std::endl;
            if (!addVlanConfig(vlan)) {
                std::cerr << "Failed to add VLAN: " << vlan.name << std::endl;
                success = false;
            }
        }
    }
    
    if (operationConfig.removeVlansOp) {
        for (const auto& vlan : operationConfig.vlans) {
            std::cout << "Removing VLAN: " << vlan.name << std::endl;
            if (!removeVlanConfig(vlan.name)) {
                std::cerr << "Failed to remove VLAN: " << vlan.name << std::endl;
                success = false;
            }
        }
    }
    
    // Firewall operations
    if (operationConfig.addFirewallRulesOp) {
        for (const auto& rule : operationConfig.firewallRules) {
            std::cout << "Adding firewall rule: " << rule.name << std::endl;
            if (!addFirewallRule(rule)) {
                std::cerr << "Failed to add firewall rule: " << rule.name << std::endl;
                success = false;
            }
        }
    }
    
    if (operationConfig.removeFirewallRulesOp) {
        for (const auto& rule : operationConfig.firewallRules) {
            std::cout << "Removing firewall rule: " << rule.name << std::endl;
            if (!removeFirewallRule(rule.name)) {
                std::cerr << "Failed to remove firewall rule: " << rule.name << std::endl;
                success = false;
            }
        }
    }
    
    // Bridge operations
    if (operationConfig.addBridgesOp) {
        for (const auto& bridge : operationConfig.bridges) {
            std::cout << "Adding bridge: " << bridge.name << std::endl;
            if (!addBridgeConfig(bridge)) {
                std::cerr << "Failed to add bridge: " << bridge.name << std::endl;
                success = false;
            }
        }
    }
    
    if (operationConfig.removeBridgesOp) {
        for (const auto& bridge : operationConfig.bridges) {
            std::cout << "Removing bridge: " << bridge.name << std::endl;
            if (!removeBridgeConfig(bridge.name)) {
                std::cerr << "Failed to remove bridge: " << bridge.name << std::endl;
                success = false;
            }
        }
    }
    
    // NAT operations
    if (operationConfig.addNatRulesOp) {
        for (const auto& rule : operationConfig.natRules) {
            std::cout << "Adding NAT rule: " << rule.name << std::endl;
            if (!addNatRule(rule)) {
                std::cerr << "Failed to add NAT rule: " << rule.name << std::endl;
                success = false;
            }
        }
    }
    
    if (operationConfig.removeNatRulesOp) {
        for (const auto& rule : operationConfig.natRules) {
            std::cout << "Removing NAT rule: " << rule.name << std::endl;
            if (!removeNatRule(rule.name)) {
                std::cerr << "Failed to remove NAT rule: " << rule.name << std::endl;
                success = false;
            }
        }
    }
    
    // Profile operations
    if (operationConfig.createProfileOp) {
        std::cout << "Creating profile: " << operationConfig.profileName << std::endl;
        // Note: Profile creation would need to be implemented
    }
    
    if (operationConfig.activateProfileOp) {
        std::cout << "Activating profile: " << operationConfig.activateProfile << std::endl;
        // Note: Profile activation would need to be implemented
    }
    
    if (operationConfig.deleteProfileOp) {
        std::cout << "Deleting profile: " << operationConfig.deleteProfile << std::endl;
        // Note: Profile deletion would need to be implemented
    }
    
    // System operations
    if (operationConfig.restartInterfaceOp) {
        std::cout << "Restarting interface..." << std::endl;
        if (!restartInterface("eth0")) {
            std::cerr << "Failed to restart interface" << std::endl;
            success = false;
        }
    }
    
    if (operationConfig.restoreDefaultsOp) {
        std::cout << "Restoring defaults..." << std::endl;
        if (!restoreDefaults()) {
            std::cerr << "Failed to restore defaults" << std::endl;
            success = false;
        }
    }
    
    // Data collection and listing operations
    if (operationConfig.showStatusOp) {
        std::cout << "\n=== Network Status ===" << std::endl;
        showStatus();
    }
    
    if (operationConfig.showInterfaceInfoOp) {
        std::cout << "\n=== Interface Information ===" << std::endl;
        showInterfaceInfo();
    }
    
    if (operationConfig.listProfilesOp) {
        std::cout << "\n=== Network Profiles ===" << std::endl;
        listProfiles();
    }
    
    if (operationConfig.listStaticRoutesOp) {
        std::cout << "\n=== Static Routes ===" << std::endl;
        listStaticRoutes();
    }
    
    if (operationConfig.listVlansOp) {
        std::cout << "\n=== VLAN Configurations ===" << std::endl;
        listVlans();
    }
    
    if (operationConfig.listFirewallRulesOp) {
        std::cout << "\n=== Firewall Rules ===" << std::endl;
        listFirewallRules();
    }
    
    if (operationConfig.listBridgesOp) {
        std::cout << "\n=== Bridge Configurations ===" << std::endl;
        listBridges();
    }
    
    if (operationConfig.listNatRulesOp) {
        std::cout << "\n=== NAT Rules ===" << std::endl;
        listNatRules();
    }
    
    if (operationConfig.startMonitoringOp) {
        std::cout << "\n=== Network Monitoring ===" << std::endl;
        startMonitoring();
    }
    
    if (operationConfig.stopMonitoringOp) {
        std::cout << "\n=== Network Monitoring ===" << std::endl;
        stopMonitoring();
    }
    
    std::cout << "Operation execution completed with " << (success ? "success" : "some failures") << std::endl;
    return success;
}

void NetworkConfigAPI::setTestMode(bool enabled) {
    testMode = enabled;
    if (testMode) {
        std::cout << "Test mode enabled - system commands will be simulated" << std::endl;
    }
}

PackageConfig NetworkConfigAPI::getPackageConfig() const {
    return packageConfig;
}

bool NetworkConfigAPI::saveNetworkConfig() {
    return writeConfigFile();
}

NetworkStatus NetworkConfigAPI::getConnectionStatus() {
    NetworkStatus status;
    status.connected = false;
    status.connectionType = "unknown";
    status.ipv4Address = "";
    status.ipv6Address = "";
    status.macAddress = "";
    status.linkSpeed = "";
    status.sessionDuration = std::chrono::seconds(0);
    status.gatewayAddress = "";
    status.externalIP = "";
    status.downloadSpeed = 0.0;
    status.uploadSpeed = 0.0;

    auto it = configOptions.find("proto");
    if (it != configOptions.end()) {
        status.connectionType = it->second;
        status.connected = true;
    }

    it = configOptions.find("ipaddr");
    if (it != configOptions.end()) {
        status.ipv4Address = it->second;
    }

    it = configOptions.find("gateway");
    if (it != configOptions.end()) {
        status.gatewayAddress = it->second;
    }

    return status;
}

BasicSettings NetworkConfigAPI::getBasicSettings() {
    BasicSettings settings;
    settings.connectionMode = "dhcp";
    settings.ipv4Address = "";
    settings.subnetMask = "";
    settings.defaultGateway = "";
    settings.dhcpLeaseTimeRemaining = 0;
    settings.interfacePriority = 0;

    auto it = configOptions.find("proto");
    if (it != configOptions.end()) {
        settings.connectionMode = it->second;
    }

    it = configOptions.find("ipaddr");
    if (it != configOptions.end()) {
        settings.ipv4Address = it->second;
    }

    it = configOptions.find("netmask");
    if (it != configOptions.end()) {
        settings.subnetMask = it->second;
    }

    it = configOptions.find("gateway");
    if (it != configOptions.end()) {
        settings.defaultGateway = it->second;
    }

    return settings;
}

AdvancedSettings NetworkConfigAPI::getAdvancedSettings() {
    AdvancedSettings settings;
    settings.mtuSize = 1500;
    settings.checksumOffload = false;
    settings.tcpSegmentationOffload = false;
    settings.rssOffload = false;
    settings.lsoOffload = false;
    settings.linkNegotiationMode = "auto";
    settings.forcedSpeed = 0;
    settings.forcedDuplex = "full";
    settings.energyEfficientEthernet = false;

    auto it = configOptions.find("mtu");
    if (it != configOptions.end()) {
        try {
            settings.mtuSize = std::stoi(it->second);
        } catch (...) {
            settings.mtuSize = 1500; // Default MTU
        }
    }

    return settings;
}

std::vector<ConnectionProfile> NetworkConfigAPI::getProfiles() {
    std::vector<ConnectionProfile> profiles;
    return profiles;
}

bool NetworkConfigAPI::setConnectionMode(const std::string& mode) {
    configOptions["proto"] = mode;
    return saveNetworkConfig();
}

bool NetworkConfigAPI::setStaticIP(const std::string& ip, const std::string& subnet, const std::string& gateway) {
    configOptions["ipaddr"] = ip;
    configOptions["netmask"] = subnet;
    configOptions["gateway"] = gateway;
    configOptions["proto"] = "static";
    return saveNetworkConfig();
}

bool NetworkConfigAPI::setDnsServers(const std::vector<std::string>& dns) {
    configOptions["dns"] = formatListOption(dns);
    return saveNetworkConfig();
}

bool NetworkConfigAPI::setMtuSize(int mtu) {
    configOptions["mtu"] = std::to_string(mtu);
    return saveNetworkConfig();
}

bool NetworkConfigAPI::setHardwareOffload(const std::string& feature, bool enabled) {
    return true;
}

bool NetworkConfigAPI::setLinkNegotiation(const std::string& mode, int speed, const std::string& duplex) {
    return true;
}

bool NetworkConfigAPI::setEnergyEfficientEthernet(bool enabled) {
    return true;
}

bool NetworkConfigAPI::enableInterface(const std::string& interface, bool enabled) {
    if (testMode) {
        std::cout << "[TEST] Would " << (enabled ? "enable" : "disable") << " interface: " << interface << std::endl;
        return true;
    }
    std::string command = "ifconfig " + interface + " " + (enabled ? "up" : "down");
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::restartInterface(const std::string& interface) {
    if (testMode) {
        std::cout << "[TEST] Would restart interface: " << interface << std::endl;
        return true;
    }
    std::string command = "ifdown " + interface + " && ifup " + interface;
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::restoreDefaults() {
    configOptions.clear();
    configOptions["proto"] = "dhcp";
    configOptions["ipaddr"] = "192.168.1.1";
    configOptions["netmask"] = "255.255.255.0";
    return saveNetworkConfig();
}

// Static Route Management Implementation
std::vector<StaticRoute> NetworkConfigAPI::getStaticRoutes() {
    std::vector<StaticRoute> routes;
    
    std::map<std::string, std::string> routeConfig;
    if (parseNetworkConfig(staticRouteConfigPath, routeConfig)) {
        StaticRoute route;
        route.target = routeConfig["route_target"];
        route.gateway = routeConfig["route_gateway"];
        route.interface = routeConfig["route_interface"];
        
        try {
            route.metric = std::stoi(routeConfig["route_metric"]);
        } catch (...) {
            route.metric = 10; // Default metric
        }
        
        route.enabled = (routeConfig["route_enabled"] == "1");
        routes.push_back(route);
    }
    
    return routes;
}

bool NetworkConfigAPI::addStaticRoute(const StaticRoute& route) {
    if (!validateIpAddress(route.gateway)) {
        return false;
    }
    
    if (testMode) {
        std::cout << "[TEST] Would add static route:" << std::endl;
        std::cout << "  Target: " << route.target << std::endl;
        std::cout << "  Gateway: " << route.gateway << std::endl;
        std::cout << "  Interface: " << route.interface << std::endl;
        std::cout << "  Metric: " << route.metric << std::endl;
        std::cout << "  Enabled: " << (route.enabled ? "Yes" : "No") << std::endl;
        return true;
    }
    
    std::string command = "uci set network." + generateConfigId("route") + "=route ";
    command += " && uci set network." + generateConfigId("route") + ".target='" + route.target + "'";
    command += " && uci set network." + generateConfigId("route") + ".gateway='" + route.gateway + "'";
    command += " && uci set network." + generateConfigId("route") + ".interface='" + route.interface + "'";
    command += " && uci set network." + generateConfigId("route") + ".metric='" + std::to_string(route.metric) + "'";
    command += " && uci set network." + generateConfigId("route") + ".enabled='" + std::string(route.enabled ? "1" : "0") + "'";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::removeStaticRoute(const std::string& target) {
    if (testMode) {
        std::cout << "[TEST] Would remove static route: " << target << std::endl;
        return true;
    }
    std::string command = "uci show network | grep route | grep " + target + " | cut -d. -f2 | cut -d= -f1 | xargs -I {} uci delete network.{}";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::updateStaticRoute(const std::string& target, const StaticRoute& route) {
    removeStaticRoute(target);
    return addStaticRoute(route);
}

bool NetworkConfigAPI::enableStaticRoute(const std::string& target, bool enabled) {
    if (testMode) {
        std::cout << "[TEST] Would " << (enabled ? "enable" : "disable") << " static route: " << target << std::endl;
        return true;
    }
    std::string command = "uci show network | grep route | grep " + target + " | cut -d. -f2 | cut -d= -f1 | xargs -I {} uci set network.{}.enabled='" + std::string(enabled ? "1" : "0") + "'";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

// VLAN Configuration Implementation
std::vector<VlanConfig> NetworkConfigAPI::getVlanConfigs() {
    std::vector<VlanConfig> vlans;
    
    std::map<std::string, std::string> vlanConfig;
    if (parseNetworkConfig(networkConfigPath, vlanConfig)) {
        VlanConfig vlan;
        vlan.name = vlanConfig["vlan_name"];
        
        try {
            vlan.vlanId = std::stoi(vlanConfig["vlan_id"]);
        } catch (...) {
            vlan.vlanId = 1; // Default VLAN ID
        }
        
        vlan.baseInterface = vlanConfig["vlan_base"];
        vlan.protocol = vlanConfig["vlan_protocol"];
        vlan.enabled = (vlanConfig["vlan_enabled"] == "1");
        vlans.push_back(vlan);
    }
    
    return vlans;
}

bool NetworkConfigAPI::addVlanConfig(const VlanConfig& vlan) {
    if (!validateVlanId(vlan.vlanId)) {
        return false;
    }
    
    if (testMode) {
        std::cout << "[TEST] Would add VLAN configuration:" << std::endl;
        std::cout << "  Name: " << vlan.name << std::endl;
        std::cout << "  VLAN ID: " << vlan.vlanId << std::endl;
        std::cout << "  Base Interface: " << vlan.baseInterface << std::endl;
        std::cout << "  Protocol: " << vlan.protocol << std::endl;
        std::cout << "  Enabled: " << (vlan.enabled ? "Yes" : "No") << std::endl;
        return true;
    }
    
    std::string command = "uci set network." + vlan.name + "=interface ";
    command += " && uci set network." + vlan.name + ".ifname='" + vlan.baseInterface + "'";
    command += " && uci set network." + vlan.name + ".type='bridge'";
    command += " && uci set network." + vlan.name + ".vlan_id='" + std::to_string(vlan.vlanId) + "'";
    command += " && uci set network." + vlan.name + ".protocol='" + vlan.protocol + "'";
    command += " && uci set network." + vlan.name + ".enabled='" + std::string(vlan.enabled ? "1" : "0") + "'";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::removeVlanConfig(const std::string& name) {
    if (testMode) {
        std::cout << "[TEST] Would remove VLAN: " << name << std::endl;
        return true;
    }
    std::string command = "uci delete network." + name + " 2>/dev/null";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::updateVlanConfig(const std::string& name, const VlanConfig& vlan) {
    removeVlanConfig(name);
    return addVlanConfig(vlan);
}

bool NetworkConfigAPI::enableVlanConfig(const std::string& name, bool enabled) {
    if (testMode) {
        std::cout << "[TEST] Would " << (enabled ? "enable" : "disable") << " VLAN: " << name << std::endl;
        return true;
    }
    std::string command = "uci set network." + name + ".enabled='" + std::string(enabled ? "1" : "0") + "'";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

// Firewall Rule Management Implementation
std::vector<FirewallRule> NetworkConfigAPI::getFirewallRules() {
    std::vector<FirewallRule> rules;
    
    std::map<std::string, std::string> firewallConfig;
    if (parseNetworkConfig(firewallConfigPath, firewallConfig)) {
        FirewallRule rule;
        rule.name = firewallConfig["rule_name"];
        rule.type = firewallConfig["rule_type"];
        rule.src = firewallConfig["rule_src"];
        rule.dest = firewallConfig["rule_dest"];
        rule.target = firewallConfig["rule_target"];
        rule.proto = firewallConfig["rule_proto"];
        rule.srcPort = firewallConfig["rule_src_port"];
        rule.destPort = firewallConfig["rule_dest_port"];
        rule.srcIp = firewallConfig["rule_src_ip"];
        rule.destIp = firewallConfig["rule_dest_ip"];
        rule.enabled = (firewallConfig["rule_enabled"] == "1");
        rules.push_back(rule);
    }
    
    return rules;
}

bool NetworkConfigAPI::addFirewallRule(const FirewallRule& rule) {
    if (testMode) {
        std::cout << "[TEST] Would add firewall rule:" << std::endl;
        std::cout << "  Name: " << rule.name << std::endl;
        std::cout << "  Source: " << rule.src << std::endl;
        std::cout << "  Destination: " << rule.dest << std::endl;
        std::cout << "  Target: " << rule.target << std::endl;
        std::cout << "  Protocol: " << rule.proto << std::endl;
        if (!rule.srcPort.empty()) {
            std::cout << "  Source Port: " << rule.srcPort << std::endl;
        }
        if (!rule.destPort.empty()) {
            std::cout << "  Destination Port: " << rule.destPort << std::endl;
        }
        if (!rule.srcIp.empty()) {
            std::cout << "  Source IP: " << rule.srcIp << std::endl;
        }
        if (!rule.destIp.empty()) {
            std::cout << "  Destination IP: " << rule.destIp << std::endl;
        }
        std::cout << "  Enabled: " << (rule.enabled ? "Yes" : "No") << std::endl;
        return true;
    }
    
    std::string command = "uci add firewall rule";
    command += " && uci set firewall.@rule[-1].name='" + rule.name + "'";
    command += " && uci set firewall.@rule[-1].src='" + rule.src + "'";
    command += " && uci set firewall.@rule[-1].dest='" + rule.dest + "'";
    command += " && uci set firewall.@rule[-1].target='" + rule.target + "'";
    command += " && uci set firewall.@rule[-1].proto='" + rule.proto + "'";
    
    if (!rule.srcPort.empty()) {
        command += " && uci set firewall.@rule[-1].src_port='" + rule.srcPort + "'";
    }
    if (!rule.destPort.empty()) {
        command += " && uci set firewall.@rule[-1].dest_port='" + rule.destPort + "'";
    }
    if (!rule.srcIp.empty()) {
        command += " && uci set firewall.@rule[-1].src_ip='" + rule.srcIp + "'";
    }
    if (!rule.destIp.empty()) {
        command += " && uci set firewall.@rule[-1].dest_ip='" + rule.destIp + "'";
    }
    
    command += " && uci set firewall.@rule[-1].enabled='" + std::string(rule.enabled ? "1" : "0") + "'";
    command += " && uci commit firewall";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::removeFirewallRule(const std::string& name) {
    if (testMode) {
        std::cout << "[TEST] Would remove firewall rule: " << name << std::endl;
        return true;
    }
    std::string command = "uci show firewall | grep " + name + " | cut -d. -f2 | cut -d= -f1 | xargs -I {} uci delete firewall.{}";
    command += " && uci commit firewall";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::updateFirewallRule(const std::string& name, const FirewallRule& rule) {
    removeFirewallRule(name);
    return addFirewallRule(rule);
}

bool NetworkConfigAPI::enableFirewallRule(const std::string& name, bool enabled) {
    if (testMode) {
        std::cout << "[TEST] Would " << (enabled ? "enable" : "disable") << " firewall rule: " << name << std::endl;
        return true;
    }
    std::string command = "uci show firewall | grep " + name + " | cut -d. -f2 | cut -d= -f1 | xargs -I {} uci set firewall.{}.enabled='" + std::string(enabled ? "1" : "0") + "'";
    command += " && uci commit firewall";
    
    return executeSystemCommand(command);
}

// Bridge Configuration Implementation
std::vector<BridgeConfig> NetworkConfigAPI::getBridgeConfigs() {
    std::vector<BridgeConfig> bridges;
    
    std::map<std::string, std::string> bridgeConfig;
    if (parseNetworkConfig(networkConfigPath, bridgeConfig)) {
        BridgeConfig bridge;
        bridge.name = bridgeConfig["bridge_name"];
        bridge.interfaces = parseListOption(bridgeConfig["bridge_interfaces"]);
        bridge.stp = bridgeConfig["bridge_stp"];
        bridge.forwardDelay = bridgeConfig["bridge_forward_delay"];
        bridge.maxAge = bridgeConfig["bridge_max_age"];
        bridge.helloTime = bridgeConfig["bridge_hello_time"];
        bridge.enabled = (bridgeConfig["bridge_enabled"] == "1");
        bridges.push_back(bridge);
    }
    
    return bridges;
}

bool NetworkConfigAPI::addBridgeConfig(const BridgeConfig& bridge) {
    if (testMode) {
        std::cout << "[TEST] Would add bridge configuration:" << std::endl;
        std::cout << "  Name: " << bridge.name << std::endl;
        std::cout << "  Interfaces: ";
        for (size_t i = 0; i < bridge.interfaces.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << bridge.interfaces[i];
        }
        std::cout << std::endl;
        std::cout << "  STP: " << bridge.stp << std::endl;
        std::cout << "  Forward Delay: " << bridge.forwardDelay << std::endl;
        std::cout << "  Max Age: " << bridge.maxAge << std::endl;
        std::cout << "  Hello Time: " << bridge.helloTime << std::endl;
        std::cout << "  Enabled: " << (bridge.enabled ? "Yes" : "No") << std::endl;
        return true;
    }
    
    std::string command = "uci set network." + bridge.name + "=interface";
    command += " && uci set network." + bridge.name + ".type='bridge'";
    command += " && uci set network." + bridge.name + ".stp='" + bridge.stp + "'";
    command += " && uci set network." + bridge.name + ".forward_delay='" + bridge.forwardDelay + "'";
    command += " && uci set network." + bridge.name + ".max_age='" + bridge.maxAge + "'";
    command += " && uci set network." + bridge.name + ".hello_time='" + bridge.helloTime + "'";
    command += " && uci set network." + bridge.name + ".ifname='" + formatListOption(bridge.interfaces) + "'";
    command += " && uci set network." + bridge.name + ".enabled='" + std::string(bridge.enabled ? "1" : "0") + "'";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::removeBridgeConfig(const std::string& name) {
    if (testMode) {
        std::cout << "[TEST] Would remove bridge: " << name << std::endl;
        return true;
    }
    std::string command = "uci delete network." + name + " 2>/dev/null";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::updateBridgeConfig(const std::string& name, const BridgeConfig& bridge) {
    removeBridgeConfig(name);
    return addBridgeConfig(bridge);
}

bool NetworkConfigAPI::enableBridgeConfig(const std::string& name, bool enabled) {
    if (testMode) {
        std::cout << "[TEST] Would " << (enabled ? "enable" : "disable") << " bridge: " << name << std::endl;
        return true;
    }
    std::string command = "uci set network." + name + ".enabled='" + std::string(enabled ? "1" : "0") + "'";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::addInterfaceToBridge(const std::string& bridgeName, const std::string& interface) {
    std::string command = "uci add_list network." + bridgeName + ".ifname='" + interface + "'";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::removeInterfaceFromBridge(const std::string& bridgeName, const std::string& interface) {
    std::string command = "uci remove_list network." + bridgeName + ".ifname='" + interface + "'";
    command += " && uci commit network";
    
    return executeSystemCommand(command);
}

// NAT Rule Management Implementation
std::vector<NatRule> NetworkConfigAPI::getNatRules() {
    std::vector<NatRule> rules;
    
    std::map<std::string, std::string> natConfig;
    if (parseNetworkConfig(firewallConfigPath, natConfig)) {
        NatRule rule;
        rule.name = natConfig["nat_name"];
        rule.type = natConfig["nat_type"];
        rule.src = natConfig["nat_src"];
        rule.dest = natConfig["nat_dest"];
        rule.target = natConfig["nat_target"];
        rule.proto = natConfig["nat_proto"];
        rule.srcPort = natConfig["nat_src_port"];
        rule.destPort = natConfig["nat_dest_port"];
        rule.srcIp = natConfig["nat_src_ip"];
        rule.destIp = natConfig["nat_dest_ip"];
        rule.enabled = (natConfig["nat_enabled"] == "1");
        rules.push_back(rule);
    }
    
    return rules;
}

bool NetworkConfigAPI::addNatRule(const NatRule& rule) {
    if (testMode) {
        std::cout << "[TEST] Would add NAT rule:" << std::endl;
        std::cout << "  Name: " << rule.name << std::endl;
        std::cout << "  Type: " << rule.type << std::endl;
        std::cout << "  Source: " << rule.src << std::endl;
        std::cout << "  Destination: " << rule.dest << std::endl;
        std::cout << "  Target: " << rule.target << std::endl;
        std::cout << "  Protocol: " << rule.proto << std::endl;
        if (!rule.srcPort.empty()) {
            std::cout << "  Source Port: " << rule.srcPort << std::endl;
        }
        if (!rule.destPort.empty()) {
            std::cout << "  Destination Port: " << rule.destPort << std::endl;
        }
        std::cout << "  Enabled: " << (rule.enabled ? "Yes" : "No") << std::endl;
        return true;
    }
    
    std::string command = "uci add firewall " + rule.type;
    command += " && uci set firewall.@" + rule.type + "[-1].name='" + rule.name + "'";
    command += " && uci set firewall.@" + rule.type + "[-1].src='" + rule.src + "'";
    command += " && uci set firewall.@" + rule.type + "[-1].dest='" + rule.dest + "'";
    
    if (!rule.target.empty()) {
        command += " && uci set firewall.@" + rule.type + "[-1].target='" + rule.target + "'";
    }
    if (!rule.proto.empty()) {
        command += " && uci set firewall.@" + rule.type + "[-1].proto='" + rule.proto + "'";
    }
    if (!rule.srcPort.empty()) {
        command += " && uci set firewall.@" + rule.type + "[-1].src_port='" + rule.srcPort + "'";
    }
    if (!rule.destPort.empty()) {
        command += " && uci set firewall.@" + rule.type + "[-1].dest_port='" + rule.destPort + "'";
    }
    if (!rule.srcIp.empty()) {
        command += " && uci set firewall.@" + rule.type + "[-1].src_ip='" + rule.srcIp + "'";
    }
    if (!rule.destIp.empty()) {
        command += " && uci set firewall.@" + rule.type + "[-1].dest_ip='" + rule.destIp + "'";
    }
    
    command += " && uci set firewall.@" + rule.type + "[-1].enabled='" + std::string(rule.enabled ? "1" : "0") + "'";
    command += " && uci commit firewall";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::removeNatRule(const std::string& name) {
    if (testMode) {
        std::cout << "[TEST] Would remove NAT rule: " << name << std::endl;
        return true;
    }
    std::string command = "uci show firewall | grep " + name + " | cut -d. -f2 | cut -d= -f1 | xargs -I {} uci delete firewall.{}";
    command += " && uci commit firewall";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::updateNatRule(const std::string& name, const NatRule& rule) {
    removeNatRule(name);
    return addNatRule(rule);
}

bool NetworkConfigAPI::enableNatRule(const std::string& name, bool enabled) {
    if (testMode) {
        std::cout << "[TEST] Would " << (enabled ? "enable" : "disable") << " NAT rule: " << name << std::endl;
        return true;
    }
    std::string command = "uci show firewall | grep " + name + " | cut -d. -f2 | cut -d= -f1 | xargs -I {} uci set firewall.{}.enabled='" + std::string(enabled ? "1" : "0") + "'";
    command += " && uci commit firewall";
    
    return executeSystemCommand(command);
}

bool NetworkConfigAPI::parseConfigFile() {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        line = Utils::trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = Utils::trim(line.substr(0, pos));
            std::string value = Utils::trim(line.substr(pos + 1));
            configOptions[key] = value;
        }
    }
    
    return true;
}

bool NetworkConfigAPI::writeConfigFile() {
    std::ofstream file(configFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "config interface 'lan'\n";
    for (const auto& option : configOptions) {
        file << "\toption " << option.first << " '" << option.second << "'\n";
    }
    
    return true;
}

std::vector<std::string> NetworkConfigAPI::parseListOption(const std::string& value) {
    return Utils::split(value, ' ');
}

std::string NetworkConfigAPI::formatListOption(const std::vector<std::string>& list) {
    std::string result;
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) result += " ";
        result += list[i];
    }
    return result;
}

bool NetworkConfigAPI::parseNetworkConfig(const std::string& configPath, std::map<std::string, std::string>& config) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        line = Utils::trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = Utils::trim(line.substr(0, pos));
            std::string value = Utils::trim(line.substr(pos + 1));
            config[key] = value;
        }
    }
    
    return true;
}

bool NetworkConfigAPI::executeSystemCommand(const std::string& command) {
    // Print command in verbose mode
    if (Utils::isVerboseMode()) {
        std::cout << "[VERBOSE] Executing system command: " << command << std::endl;
    }
    
    int result = system(command.c_str());
    
    // Print result in verbose mode
    if (Utils::isVerboseMode()) {
        if (result == 0) {
            std::cout << "[VERBOSE] System command executed successfully" << std::endl;
        } else {
            std::cout << "[VERBOSE] System command failed with exit code: " << result << std::endl;
        }
    }
    
    return result == 0;
}

std::string NetworkConfigAPI::generateConfigId(const std::string& prefix) {
    static int counter = 0;
    return prefix + std::to_string(++counter);
}

bool NetworkConfigAPI::validateIpAddress(const std::string& ip) {
    return Utils::isValidIPv4(ip);
}

bool NetworkConfigAPI::validateVlanId(int vlanId) {
    return vlanId >= 1 && vlanId <= 4094;
}

bool NetworkConfigAPI::validatePort(const std::string& port) {
    try {
        int portNum = std::stoi(port);
        return portNum >= 1 && portNum <= 65535;
    } catch (...) {
        return false;
    }
}

bool NetworkConfigAPI::parseJsonConfig(const std::string& jsonContent, PackageConfig& config) {
    // Simple JSON parser - this is a basic implementation
    // In a production environment, you would use a proper JSON library
    
    // Reset config to defaults
    config.networkConfigPath = "/etc/config/network";
    config.firewallConfigPath = "/etc/config/firewall";
    config.staticRouteConfigPath = "/etc/config/network";
    config.resolvConfPath = "/etc/resolv.conf";
    config.networkProfilesDir = "/etc/Ultima-Config/ur-base-network-mann/network-profiles";
    config.networkBackupsDir = "/etc/Ultima-Config/ur-base-network-mann/network-backups";
    config.defaultInterface = "eth0";
    config.defaultConnectionMode = "dhcp";
    config.defaultMtuSize = 1500;
    
    // Parse config_files section
    size_t configFilesPos = jsonContent.find("\"config_files\"");
    if (configFilesPos != std::string::npos) {
        size_t braceStart = jsonContent.find("{", configFilesPos);
        size_t braceEnd = jsonContent.find("}", braceStart);
        
        if (braceStart != std::string::npos && braceEnd != std::string::npos) {
            std::string configFilesSection = jsonContent.substr(braceStart + 1, braceEnd - braceStart - 1);
            
            // Extract network config path
            size_t networkPos = configFilesSection.find("\"network\"");
            if (networkPos != std::string::npos) {
                size_t colonPos = configFilesSection.find(":", networkPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = configFilesSection.find("\"", colonPos);
                    size_t quoteEnd = configFilesSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.networkConfigPath = configFilesSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract firewall config path
            size_t firewallPos = configFilesSection.find("\"firewall\"");
            if (firewallPos != std::string::npos) {
                size_t colonPos = configFilesSection.find(":", firewallPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = configFilesSection.find("\"", colonPos);
                    size_t quoteEnd = configFilesSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.firewallConfigPath = configFilesSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract static routes config path
            size_t staticRoutesPos = configFilesSection.find("\"static_routes\"");
            if (staticRoutesPos != std::string::npos) {
                size_t colonPos = configFilesSection.find(":", staticRoutesPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = configFilesSection.find("\"", colonPos);
                    size_t quoteEnd = configFilesSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.staticRouteConfigPath = configFilesSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract resolv.conf path
            size_t resolvPos = configFilesSection.find("\"resolv_conf\"");
            if (resolvPos != std::string::npos) {
                size_t colonPos = configFilesSection.find(":", resolvPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = configFilesSection.find("\"", colonPos);
                    size_t quoteEnd = configFilesSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.resolvConfPath = configFilesSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract network profiles directory
            size_t profilesPos = configFilesSection.find("\"network_profiles\"");
            if (profilesPos != std::string::npos) {
                size_t colonPos = configFilesSection.find(":", profilesPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = configFilesSection.find("\"", colonPos);
                    size_t quoteEnd = configFilesSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.networkProfilesDir = configFilesSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract network backups directory
            size_t backupsPos = configFilesSection.find("\"network_backups\"");
            if (backupsPos != std::string::npos) {
                size_t colonPos = configFilesSection.find(":", backupsPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = configFilesSection.find("\"", colonPos);
                    size_t quoteEnd = configFilesSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.networkBackupsDir = configFilesSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
        }
    }
    
    // Parse defaults section
    size_t defaultsPos = jsonContent.find("\"defaults\"");
    if (defaultsPos != std::string::npos) {
        size_t braceStart = jsonContent.find("{", defaultsPos);
        size_t braceEnd = jsonContent.find("}", braceStart);
        
        if (braceStart != std::string::npos && braceEnd != std::string::npos) {
            std::string defaultsSection = jsonContent.substr(braceStart + 1, braceEnd - braceStart - 1);
            
            // Extract default interface
            size_t interfacePos = defaultsSection.find("\"interface\"");
            if (interfacePos != std::string::npos) {
                size_t colonPos = defaultsSection.find(":", interfacePos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = defaultsSection.find("\"", colonPos);
                    size_t quoteEnd = defaultsSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.defaultInterface = defaultsSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract default connection mode
            size_t modePos = defaultsSection.find("\"connection_mode\"");
            if (modePos != std::string::npos) {
                size_t colonPos = defaultsSection.find(":", modePos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = defaultsSection.find("\"", colonPos);
                    size_t quoteEnd = defaultsSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.defaultConnectionMode = defaultsSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract default MTU size
            size_t mtuPos = defaultsSection.find("\"mtu_size\"");
            if (mtuPos != std::string::npos) {
                size_t colonPos = defaultsSection.find(":", mtuPos);
                if (colonPos != std::string::npos) {
                    std::string mtuValue = defaultsSection.substr(colonPos + 1);
                    // Remove any whitespace and commas
                    mtuValue.erase(std::remove_if(mtuValue.begin(), mtuValue.end(), ::isspace), mtuValue.end());
                    if (!mtuValue.empty() && mtuValue.back() == ',') {
                        mtuValue.pop_back();
                    }
                    try {
                        config.defaultMtuSize = std::stoi(mtuValue);
                    } catch (...) {
                        config.defaultMtuSize = 1500; // Keep default if parsing fails
                    }
                }
            }
        }
    }
    
    return true;
}

bool NetworkConfigAPI::parseOperationJsonConfig(const std::string& jsonContent, OperationConfig& config) {
    // Initialize all operation flags to false
    config.setConnectionModeOp = false;
    config.setStaticIpOp = false;
    config.setDnsOp = false;
    config.setMtuOp = false;
    config.addStaticRoutesOp = false;
    config.removeStaticRoutesOp = false;
    config.addVlansOp = false;
    config.removeVlansOp = false;
    config.addFirewallRulesOp = false;
    config.removeFirewallRulesOp = false;
    config.addBridgesOp = false;
    config.removeBridgesOp = false;
    config.addNatRulesOp = false;
    config.removeNatRulesOp = false;
    config.createProfileOp = false;
    config.activateProfileOp = false;
    config.deleteProfileOp = false;
    config.restartInterfaceOp = false;
    config.restoreDefaultsOp = false;
    
    // Initialize data collection and listing operations
    config.showStatusOp = false;
    config.showInterfaceInfoOp = false;
    config.listProfilesOp = false;
    config.listStaticRoutesOp = false;
    config.listVlansOp = false;
    config.listFirewallRulesOp = false;
    config.listBridgesOp = false;
    config.listNatRulesOp = false;
    config.startMonitoringOp = false;
    config.stopMonitoringOp = false;
    
    // Set default values
    config.connectionMode = "dhcp";
    config.staticIp = "";
    config.subnetMask = "";
    config.defaultGateway = "";
    config.mtuSize = 1500;
    config.restartInterface = false;
    config.restoreDefaults = false;
    
    // Parse network operations section
    size_t networkPos = jsonContent.find("\"network\"");
    if (networkPos != std::string::npos) {
        size_t braceStart = jsonContent.find("{", networkPos);
        size_t braceEnd = jsonContent.find("}", braceStart);
        
        if (braceStart != std::string::npos && braceEnd != std::string::npos) {
            std::string networkSection = jsonContent.substr(braceStart + 1, braceEnd - braceStart - 1);
            
            // Extract connection mode
            size_t modePos = networkSection.find("\"connection_mode\"");
            if (modePos != std::string::npos) {
                size_t colonPos = networkSection.find(":", modePos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = networkSection.find("\"", colonPos);
                    size_t quoteEnd = networkSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.connectionMode = networkSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                        config.setConnectionModeOp = true;
                    }
                }
            }
            
            // Extract static IP
            size_t ipPos = networkSection.find("\"static_ip\"");
            if (ipPos != std::string::npos) {
                size_t colonPos = networkSection.find(":", ipPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = networkSection.find("\"", colonPos);
                    size_t quoteEnd = networkSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.staticIp = networkSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                        config.setStaticIpOp = true;
                    }
                }
            }
            
            // Extract subnet mask
            size_t maskPos = networkSection.find("\"subnet_mask\"");
            if (maskPos != std::string::npos) {
                size_t colonPos = networkSection.find(":", maskPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = networkSection.find("\"", colonPos);
                    size_t quoteEnd = networkSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.subnetMask = networkSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract gateway
            size_t gwPos = networkSection.find("\"gateway\"");
            if (gwPos != std::string::npos) {
                size_t colonPos = networkSection.find(":", gwPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = networkSection.find("\"", colonPos);
                    size_t quoteEnd = networkSection.find("\"", quoteStart + 1);
                    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                        config.defaultGateway = networkSection.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
            }
            
            // Extract DNS servers
            size_t dnsPos = networkSection.find("\"dns_servers\"");
            if (dnsPos != std::string::npos) {
                size_t bracketStart = networkSection.find("[", dnsPos);
                size_t bracketEnd = networkSection.find("]", bracketStart);
                if (bracketStart != std::string::npos && bracketEnd != std::string::npos) {
                    std::string dnsArray = networkSection.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
                    config.dnsServers.clear();
                    
                    size_t pos = 0;
                    while (pos < dnsArray.length()) {
                        size_t quoteStart = dnsArray.find("\"", pos);
                        if (quoteStart == std::string::npos) break;
                        size_t quoteEnd = dnsArray.find("\"", quoteStart + 1);
                        if (quoteEnd == std::string::npos) break;
                        
                        std::string dns = dnsArray.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                        if (!dns.empty()) {
                            config.dnsServers.push_back(dns);
                        }
                        pos = quoteEnd + 1;
                    }
                    if (!config.dnsServers.empty()) {
                        config.setDnsOp = true;
                    }
                }
            }
            
            // Extract MTU size
            size_t mtuPos = networkSection.find("\"mtu_size\"");
            if (mtuPos != std::string::npos) {
                size_t colonPos = networkSection.find(":", mtuPos);
                if (colonPos != std::string::npos) {
                    std::string mtuValue = networkSection.substr(colonPos + 1);
                    mtuValue.erase(std::remove_if(mtuValue.begin(), mtuValue.end(), ::isspace), mtuValue.end());
                    try {
                        config.mtuSize = std::stoi(mtuValue);
                        config.setMtuOp = true;
                    } catch (...) {
                        config.mtuSize = 1500;
                    }
                }
            }
        }
    }
    
    // Parse static routes section
    size_t routesPos = jsonContent.find("\"static_routes\"");
    if (routesPos != std::string::npos) {
        size_t arrayStart = jsonContent.find("[", routesPos);
        size_t arrayEnd = jsonContent.find("]", arrayStart);
        
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string routesArray = jsonContent.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            parseStaticRoutesArray(routesArray, config);
        }
    }
    
    // Parse VLANs section
    size_t vlansPos = jsonContent.find("\"vlans\"");
    if (vlansPos != std::string::npos) {
        size_t arrayStart = jsonContent.find("[", vlansPos);
        size_t arrayEnd = jsonContent.find("]", arrayStart);
        
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string vlansArray = jsonContent.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            parseVlansArray(vlansArray, config);
        }
    }
    
    // Parse firewall rules section
    size_t firewallPos = jsonContent.find("\"firewall_rules\"");
    if (firewallPos != std::string::npos) {
        size_t arrayStart = jsonContent.find("[", firewallPos);
        size_t arrayEnd = jsonContent.find("]", arrayStart);
        
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string firewallArray = jsonContent.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            parseFirewallRulesArray(firewallArray, config);
        }
    }
    
    // Parse bridges section
    size_t bridgesPos = jsonContent.find("\"bridges\"");
    if (bridgesPos != std::string::npos) {
        size_t arrayStart = jsonContent.find("[", bridgesPos);
        size_t arrayEnd = jsonContent.find("]", arrayStart);
        
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string bridgesArray = jsonContent.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            parseBridgesArray(bridgesArray, config);
        }
    }
    
    // Parse NAT rules section
    size_t natPos = jsonContent.find("\"nat_rules\"");
    if (natPos != std::string::npos) {
        size_t arrayStart = jsonContent.find("[", natPos);
        size_t arrayEnd = jsonContent.find("]", arrayStart);
        
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string natArray = jsonContent.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            parseNatRulesArray(natArray, config);
        }
    }
    
    // Parse system operations section
    size_t systemPos = jsonContent.find("\"system\"");
    if (systemPos != std::string::npos) {
        size_t braceStart = jsonContent.find("{", systemPos);
        size_t braceEnd = jsonContent.find("}", braceStart);
        
        if (braceStart != std::string::npos && braceEnd != std::string::npos) {
            std::string systemSection = jsonContent.substr(braceStart + 1, braceEnd - braceStart - 1);
            
            // Extract restart interface flag
            size_t restartPos = systemSection.find("\"restart_interface\"");
            if (restartPos != std::string::npos) {
                size_t colonPos = systemSection.find(":", restartPos);
                if (colonPos != std::string::npos) {
                    std::string value = systemSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.restartInterfaceOp = (value == "true");
                }
            }
            
            // Extract restore defaults flag
            size_t restorePos = systemSection.find("\"restore_defaults\"");
            if (restorePos != std::string::npos) {
                size_t colonPos = systemSection.find(":", restorePos);
                if (colonPos != std::string::npos) {
                    std::string value = systemSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.restoreDefaultsOp = (value == "true");
                }
            }
        }
    }
    
    // Parse data collection and listing operations section
    size_t listingPos = jsonContent.find("\"listing\"");
    if (listingPos != std::string::npos) {
        size_t braceStart = jsonContent.find("{", listingPos);
        size_t braceEnd = jsonContent.find("}", braceStart);
        
        if (braceStart != std::string::npos && braceEnd != std::string::npos) {
            std::string listingSection = jsonContent.substr(braceStart + 1, braceEnd - braceStart - 1);
            
            // Extract show status flag
            size_t statusPos = listingSection.find("\"show_status\"");
            if (statusPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", statusPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.showStatusOp = (value == "true");
                }
            }
            
            // Extract show interface info flag
            size_t interfacePos = listingSection.find("\"show_interface_info\"");
            if (interfacePos != std::string::npos) {
                size_t colonPos = listingSection.find(":", interfacePos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.showInterfaceInfoOp = (value == "true");
                }
            }
            
            // Extract list profiles flag
            size_t profilesPos = listingSection.find("\"list_profiles\"");
            if (profilesPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", profilesPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.listProfilesOp = (value == "true");
                }
            }
            
            // Extract list static routes flag
            size_t routesPos = listingSection.find("\"list_static_routes\"");
            if (routesPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", routesPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.listStaticRoutesOp = (value == "true");
                }
            }
            
            // Extract list VLANs flag
            size_t vlansPos = listingSection.find("\"list_vlans\"");
            if (vlansPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", vlansPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.listVlansOp = (value == "true");
                }
            }
            
            // Extract list firewall rules flag
            size_t firewallPos = listingSection.find("\"list_firewall_rules\"");
            if (firewallPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", firewallPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.listFirewallRulesOp = (value == "true");
                }
            }
            
            // Extract list bridges flag
            size_t bridgesPos = listingSection.find("\"list_bridges\"");
            if (bridgesPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", bridgesPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.listBridgesOp = (value == "true");
                }
            }
            
            // Extract list NAT rules flag
            size_t natPos = listingSection.find("\"list_nat_rules\"");
            if (natPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", natPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.listNatRulesOp = (value == "true");
                }
            }
            
            // Extract start monitoring flag
            size_t startMonitorPos = listingSection.find("\"start_monitoring\"");
            if (startMonitorPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", startMonitorPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.startMonitoringOp = (value == "true");
                }
            }
            
            // Extract stop monitoring flag
            size_t stopMonitorPos = listingSection.find("\"stop_monitoring\"");
            if (stopMonitorPos != std::string::npos) {
                size_t colonPos = listingSection.find(":", stopMonitorPos);
                if (colonPos != std::string::npos) {
                    std::string value = listingSection.substr(colonPos + 1);
                    value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                    config.stopMonitoringOp = (value == "true");
                }
            }
        }
    }
    
    return true;
}

// Helper methods for parsing arrays
void NetworkConfigAPI::parseStaticRoutesArray(const std::string& arrayContent, OperationConfig& config) {
    // Look for route objects and extract their properties
    config.staticRoutes.clear();
    
    size_t pos = 0;
    while (pos < arrayContent.length()) {
        size_t braceStart = arrayContent.find("{", pos);
        if (braceStart == std::string::npos) break;
        size_t braceEnd = arrayContent.find("}", braceStart);
        if (braceEnd == std::string::npos) break;
        
        std::string routeObject = arrayContent.substr(braceStart + 1, braceEnd - braceStart - 1);
        
        StaticRoute route;
        route.target = "";
        route.gateway = "";
        route.interface = "lan";
        route.metric = 10;
        route.enabled = true;
        
        // Extract target
        size_t targetPos = routeObject.find("\"target\"");
        if (targetPos != std::string::npos) {
            size_t colonPos = routeObject.find(":", targetPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = routeObject.find("\"", colonPos);
                size_t quoteEnd = routeObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    route.target = routeObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract gateway
        size_t gwPos = routeObject.find("\"gateway\"");
        if (gwPos != std::string::npos) {
            size_t colonPos = routeObject.find(":", gwPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = routeObject.find("\"", colonPos);
                size_t quoteEnd = routeObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    route.gateway = routeObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract interface
        size_t ifacePos = routeObject.find("\"interface\"");
        if (ifacePos != std::string::npos) {
            size_t colonPos = routeObject.find(":", ifacePos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = routeObject.find("\"", colonPos);
                size_t quoteEnd = routeObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    route.interface = routeObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract metric
        size_t metricPos = routeObject.find("\"metric\"");
        if (metricPos != std::string::npos) {
            size_t colonPos = routeObject.find(":", metricPos);
            if (colonPos != std::string::npos) {
                std::string metricValue = routeObject.substr(colonPos + 1);
                metricValue.erase(std::remove_if(metricValue.begin(), metricValue.end(), ::isspace), metricValue.end());
                try {
                    route.metric = std::stoi(metricValue);
                } catch (...) {
                    route.metric = 10;
                }
            }
        }
        
        if (!route.target.empty() && !route.gateway.empty()) {
            config.staticRoutes.push_back(route);
            config.addStaticRoutesOp = true;
        }
        
        pos = braceEnd + 1;
    }
}

void NetworkConfigAPI::parseVlansArray(const std::string& arrayContent, OperationConfig& config) {
    config.vlans.clear();
    
    size_t pos = 0;
    while (pos < arrayContent.length()) {
        size_t braceStart = arrayContent.find("{", pos);
        if (braceStart == std::string::npos) break;
        size_t braceEnd = arrayContent.find("}", braceStart);
        if (braceEnd == std::string::npos) break;
        
        std::string vlanObject = arrayContent.substr(braceStart + 1, braceEnd - braceStart - 1);
        
        VlanConfig vlan;
        vlan.name = "";
        vlan.vlanId = 1;
        vlan.baseInterface = "eth0";
        vlan.protocol = "802.1q";
        vlan.enabled = true;
        
        // Extract name
        size_t namePos = vlanObject.find("\"name\"");
        if (namePos != std::string::npos) {
            size_t colonPos = vlanObject.find(":", namePos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = vlanObject.find("\"", colonPos);
                size_t quoteEnd = vlanObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    vlan.name = vlanObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract VLAN ID
        size_t idPos = vlanObject.find("\"vlan_id\"");
        if (idPos != std::string::npos) {
            size_t colonPos = vlanObject.find(":", idPos);
            if (colonPos != std::string::npos) {
                std::string idValue = vlanObject.substr(colonPos + 1);
                idValue.erase(std::remove_if(idValue.begin(), idValue.end(), ::isspace), idValue.end());
                try {
                    vlan.vlanId = std::stoi(idValue);
                } catch (...) {
                    vlan.vlanId = 1;
                }
            }
        }
        
        // Extract base interface
        size_t basePos = vlanObject.find("\"base_interface\"");
        if (basePos != std::string::npos) {
            size_t colonPos = vlanObject.find(":", basePos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = vlanObject.find("\"", colonPos);
                size_t quoteEnd = vlanObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    vlan.baseInterface = vlanObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract protocol
        size_t protoPos = vlanObject.find("\"protocol\"");
        if (protoPos != std::string::npos) {
            size_t colonPos = vlanObject.find(":", protoPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = vlanObject.find("\"", colonPos);
                size_t quoteEnd = vlanObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    vlan.protocol = vlanObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        if (!vlan.name.empty()) {
            config.vlans.push_back(vlan);
            config.addVlansOp = true;
        }
        
        pos = braceEnd + 1;
    }
}

void NetworkConfigAPI::parseFirewallRulesArray(const std::string& arrayContent, OperationConfig& config) {
    config.firewallRules.clear();
    
    size_t pos = 0;
    while (pos < arrayContent.length()) {
        size_t braceStart = arrayContent.find("{", pos);
        if (braceStart == std::string::npos) break;
        size_t braceEnd = arrayContent.find("}", braceStart);
        if (braceEnd == std::string::npos) break;
        
        std::string ruleObject = arrayContent.substr(braceStart + 1, braceEnd - braceStart - 1);
        
        FirewallRule rule;
        rule.name = "";
        rule.type = "rule";
        rule.src = "";
        rule.dest = "";
        rule.target = "";
        rule.proto = "";
        rule.enabled = true;
        
        // Extract name
        size_t namePos = ruleObject.find("\"name\"");
        if (namePos != std::string::npos) {
            size_t colonPos = ruleObject.find(":", namePos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = ruleObject.find("\"", colonPos);
                size_t quoteEnd = ruleObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.name = ruleObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract source
        size_t srcPos = ruleObject.find("\"src\"");
        if (srcPos != std::string::npos) {
            size_t colonPos = ruleObject.find(":", srcPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = ruleObject.find("\"", colonPos);
                size_t quoteEnd = ruleObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.src = ruleObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract destination
        size_t destPos = ruleObject.find("\"dest\"");
        if (destPos != std::string::npos) {
            size_t colonPos = ruleObject.find(":", destPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = ruleObject.find("\"", colonPos);
                size_t quoteEnd = ruleObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.dest = ruleObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract target
        size_t targetPos = ruleObject.find("\"target\"");
        if (targetPos != std::string::npos) {
            size_t colonPos = ruleObject.find(":", targetPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = ruleObject.find("\"", colonPos);
                size_t quoteEnd = ruleObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.target = ruleObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract protocol
        size_t protoPos = ruleObject.find("\"proto\"");
        if (protoPos != std::string::npos) {
            size_t colonPos = ruleObject.find(":", protoPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = ruleObject.find("\"", colonPos);
                size_t quoteEnd = ruleObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.proto = ruleObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        if (!rule.name.empty()) {
            config.firewallRules.push_back(rule);
            config.addFirewallRulesOp = true;
        }
        
        pos = braceEnd + 1;
    }
}

void NetworkConfigAPI::parseBridgesArray(const std::string& arrayContent, OperationConfig& config) {
    config.bridges.clear();
    
    size_t pos = 0;
    while (pos < arrayContent.length()) {
        size_t braceStart = arrayContent.find("{", pos);
        if (braceStart == std::string::npos) break;
        size_t braceEnd = arrayContent.find("}", braceStart);
        if (braceEnd == std::string::npos) break;
        
        std::string bridgeObject = arrayContent.substr(braceStart + 1, braceEnd - braceStart - 1);
        
        BridgeConfig bridge;
        bridge.name = "";
        bridge.stp = "off";
        bridge.forwardDelay = "15";
        bridge.maxAge = "20";
        bridge.helloTime = "2";
        bridge.enabled = true;
        
        // Extract name
        size_t namePos = bridgeObject.find("\"name\"");
        if (namePos != std::string::npos) {
            size_t colonPos = bridgeObject.find(":", namePos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = bridgeObject.find("\"", colonPos);
                size_t quoteEnd = bridgeObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    bridge.name = bridgeObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract STP
        size_t stpPos = bridgeObject.find("\"stp\"");
        if (stpPos != std::string::npos) {
            size_t colonPos = bridgeObject.find(":", stpPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = bridgeObject.find("\"", colonPos);
                size_t quoteEnd = bridgeObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    bridge.stp = bridgeObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract interfaces (simplified - just look for array)
        size_t interfacesPos = bridgeObject.find("\"interfaces\"");
        if (interfacesPos != std::string::npos) {
            size_t bracketStart = bridgeObject.find("[", interfacesPos);
            size_t bracketEnd = bridgeObject.find("]", bracketStart);
            if (bracketStart != std::string::npos && bracketEnd != std::string::npos) {
                std::string interfacesArray = bridgeObject.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
                
                size_t ifacePos = 0;
                while (ifacePos < interfacesArray.length()) {
                    size_t quoteStart = interfacesArray.find("\"", ifacePos);
                    if (quoteStart == std::string::npos) break;
                    size_t quoteEnd = interfacesArray.find("\"", quoteStart + 1);
                    if (quoteEnd == std::string::npos) break;
                    
                    std::string iface = interfacesArray.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    if (!iface.empty()) {
                        bridge.interfaces.push_back(iface);
                    }
                    ifacePos = quoteEnd + 1;
                }
            }
        }
        
        if (!bridge.name.empty()) {
            config.bridges.push_back(bridge);
            config.addBridgesOp = true;
        }
        
        pos = braceEnd + 1;
    }
}

void NetworkConfigAPI::parseNatRulesArray(const std::string& arrayContent, OperationConfig& config) {
    config.natRules.clear();
    
    size_t pos = 0;
    while (pos < arrayContent.length()) {
        size_t braceStart = arrayContent.find("{", pos);
        if (braceStart == std::string::npos) break;
        size_t braceEnd = arrayContent.find("}", braceStart);
        if (braceEnd == std::string::npos) break;
        
        std::string natObject = arrayContent.substr(braceStart + 1, braceEnd - braceStart - 1);
        
        NatRule rule;
        rule.name = "";
        rule.type = "redirect";
        rule.src = "";
        rule.dest = "";
        rule.target = "";
        rule.proto = "";
        rule.enabled = true;
        
        // Extract name
        size_t namePos = natObject.find("\"name\"");
        if (namePos != std::string::npos) {
            size_t colonPos = natObject.find(":", namePos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = natObject.find("\"", colonPos);
                size_t quoteEnd = natObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.name = natObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract type
        size_t typePos = natObject.find("\"type\"");
        if (typePos != std::string::npos) {
            size_t colonPos = natObject.find(":", typePos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = natObject.find("\"", colonPos);
                size_t quoteEnd = natObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.type = natObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract source
        size_t srcPos = natObject.find("\"src\"");
        if (srcPos != std::string::npos) {
            size_t colonPos = natObject.find(":", srcPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = natObject.find("\"", colonPos);
                size_t quoteEnd = natObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.src = natObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract destination
        size_t destPos = natObject.find("\"dest\"");
        if (destPos != std::string::npos) {
            size_t colonPos = natObject.find(":", destPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = natObject.find("\"", colonPos);
                size_t quoteEnd = natObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.dest = natObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract target
        size_t targetPos = natObject.find("\"target\"");
        if (targetPos != std::string::npos) {
            size_t colonPos = natObject.find(":", targetPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = natObject.find("\"", colonPos);
                size_t quoteEnd = natObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.target = natObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        // Extract protocol
        size_t protoPos = natObject.find("\"proto\"");
        if (protoPos != std::string::npos) {
            size_t colonPos = natObject.find(":", protoPos);
            if (colonPos != std::string::npos) {
                size_t quoteStart = natObject.find("\"", colonPos);
                size_t quoteEnd = natObject.find("\"", quoteStart + 1);
                if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                    rule.proto = natObject.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
        }
        
        if (!rule.name.empty()) {
            config.natRules.push_back(rule);
            config.addNatRulesOp = true;
        }
        
        pos = braceEnd + 1;
    }
}

// Data Collection and Listing Operations Implementation
void NetworkConfigAPI::showStatus() {
    std::cout << "Network Status:" << std::endl;
    std::cout << "================" << std::endl;
    
    NetworkStatus status = getConnectionStatus();
    
    std::cout << "Connection Status: " << (status.connected ? "Connected" : "Disconnected") << std::endl;
    std::cout << "Connection Type: " << status.connectionType << std::endl;
    std::cout << "IPv4 Address: " << status.ipv4Address << std::endl;
    std::cout << "IPv6 Address: " << status.ipv6Address << std::endl;
    std::cout << "MAC Address: " << status.macAddress << std::endl;
    std::cout << "Link Speed: " << status.linkSpeed << std::endl;
    std::cout << "Gateway: " << status.gatewayAddress << std::endl;
    std::cout << "External IP: " << status.externalIP << std::endl;
    std::cout << "DNS Servers: ";
    for (const auto& dns : status.dnsServers) {
        std::cout << dns << " ";
    }
    std::cout << std::endl;
    std::cout << "Download Speed: " << status.downloadSpeed << " Mbps" << std::endl;
    std::cout << "Upload Speed: " << status.uploadSpeed << " Mbps" << std::endl;
    
    BasicSettings basic = getBasicSettings();
    std::cout << "\nBasic Settings:" << std::endl;
    std::cout << "Connection Mode: " << basic.connectionMode << std::endl;
    std::cout << "Interface Priority: " << basic.interfacePriority << std::endl;
    
    AdvancedSettings advanced = getAdvancedSettings();
    std::cout << "\nAdvanced Settings:" << std::endl;
    std::cout << "MTU Size: " << advanced.mtuSize << std::endl;
    std::cout << "Link Negotiation: " << advanced.linkNegotiationMode << std::endl;
    std::cout << "Checksum Offload: " << (advanced.checksumOffload ? "Enabled" : "Disabled") << std::endl;
    std::cout << "TCP Segmentation Offload: " << (advanced.tcpSegmentationOffload ? "Enabled" : "Disabled") << std::endl;
    std::cout << "RSS Offload: " << (advanced.rssOffload ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Energy Efficient Ethernet: " << (advanced.energyEfficientEthernet ? "Enabled" : "Disabled") << std::endl;
}

void NetworkConfigAPI::showInterfaceInfo() {
    std::cout << "Interface Information:" << std::endl;
    std::cout << "======================" << std::endl;
    
    // Get interface info from monitor (simplified implementation)
    std::cout << "Interface: br-lan" << std::endl;
    std::cout << "Status: Up" << std::endl;
    std::cout << "MAC Address: 00:11:22:33:44:55" << std::endl;
    std::cout << "Speed: 1 Gbps" << std::endl;
    std::cout << "Duplex: Full" << std::endl;
    std::cout << "MTU: " << getAdvancedSettings().mtuSize << std::endl;
}

void NetworkConfigAPI::listProfiles() {
    auto profiles = getProfiles();
    std::cout << "Network Profiles:" << std::endl;
    std::cout << "==================" << std::endl;
    
    if (profiles.empty()) {
        std::cout << "No profiles found" << std::endl;
        return;
    }
    
    for (const auto& profile : profiles) {
        std::cout << "Name: " << profile.name << std::endl;
        std::cout << "Description: " << profile.description << std::endl;
        std::cout << "Connection Mode: " << profile.basicSettings.connectionMode << std::endl;
        std::cout << "MTU Size: " << profile.advancedSettings.mtuSize << std::endl;
        std::cout << "---------" << std::endl;
    }
}

void NetworkConfigAPI::listStaticRoutes() {
    auto routes = getStaticRoutes();
    std::cout << "Static Routes:" << std::endl;
    std::cout << "================" << std::endl;
    
    if (routes.empty()) {
        std::cout << "No static routes found" << std::endl;
        return;
    }
    
    for (const auto& route : routes) {
        std::cout << "Target: " << route.target << std::endl;
        std::cout << "Gateway: " << route.gateway << std::endl;
        std::cout << "Interface: " << route.interface << std::endl;
        std::cout << "Metric: " << route.metric << std::endl;
        std::cout << "Enabled: " << (route.enabled ? "Yes" : "No") << std::endl;
        std::cout << "---------" << std::endl;
    }
}

void NetworkConfigAPI::listVlans() {
    auto vlans = getVlanConfigs();
    std::cout << "VLAN Configurations:" << std::endl;
    std::cout << "====================" << std::endl;
    
    if (vlans.empty()) {
        std::cout << "No VLAN configurations found" << std::endl;
        return;
    }
    
    for (const auto& vlan : vlans) {
        std::cout << "Name: " << vlan.name << std::endl;
        std::cout << "VLAN ID: " << vlan.vlanId << std::endl;
        std::cout << "Base Interface: " << vlan.baseInterface << std::endl;
        std::cout << "Protocol: " << vlan.protocol << std::endl;
        std::cout << "Enabled: " << (vlan.enabled ? "Yes" : "No") << std::endl;
        std::cout << "---------" << std::endl;
    }
}

void NetworkConfigAPI::listFirewallRules() {
    auto rules = getFirewallRules();
    std::cout << "Firewall Rules:" << std::endl;
    std::cout << "===============" << std::endl;
    
    if (rules.empty()) {
        std::cout << "No firewall rules found" << std::endl;
        return;
    }
    
    for (const auto& rule : rules) {
        std::cout << "Name: " << rule.name << std::endl;
        std::cout << "Type: " << rule.type << std::endl;
        std::cout << "Source: " << rule.src << std::endl;
        std::cout << "Destination: " << rule.dest << std::endl;
        std::cout << "Target: " << rule.target << std::endl;
        std::cout << "Protocol: " << rule.proto << std::endl;
        if (!rule.srcPort.empty()) {
            std::cout << "Source Port: " << rule.srcPort << std::endl;
        }
        if (!rule.destPort.empty()) {
            std::cout << "Destination Port: " << rule.destPort << std::endl;
        }
        std::cout << "Enabled: " << (rule.enabled ? "Yes" : "No") << std::endl;
        std::cout << "---------" << std::endl;
    }
}

void NetworkConfigAPI::listBridges() {
    auto bridges = getBridgeConfigs();
    std::cout << "Bridge Configurations:" << std::endl;
    std::cout << "======================" << std::endl;
    
    if (bridges.empty()) {
        std::cout << "No bridge configurations found" << std::endl;
        return;
    }
    
    for (const auto& bridge : bridges) {
        std::cout << "Name: " << bridge.name << std::endl;
        std::cout << "STP: " << bridge.stp << std::endl;
        std::cout << "Forward Delay: " << bridge.forwardDelay << std::endl;
        std::cout << "Max Age: " << bridge.maxAge << std::endl;
        std::cout << "Hello Time: " << bridge.helloTime << std::endl;
        std::cout << "Interfaces: ";
        for (const auto& iface : bridge.interfaces) {
            std::cout << iface << " ";
        }
        std::cout << std::endl;
        std::cout << "Enabled: " << (bridge.enabled ? "Yes" : "No") << std::endl;
        std::cout << "---------" << std::endl;
    }
}

void NetworkConfigAPI::listNatRules() {
    auto rules = getNatRules();
    std::cout << "NAT Rules:" << std::endl;
    std::cout << "==========" << std::endl;
    
    if (rules.empty()) {
        std::cout << "No NAT rules found" << std::endl;
        return;
    }
    
    for (const auto& rule : rules) {
        std::cout << "Name: " << rule.name << std::endl;
        std::cout << "Type: " << rule.type << std::endl;
        std::cout << "Source: " << rule.src << std::endl;
        std::cout << "Destination: " << rule.dest << std::endl;
        std::cout << "Target: " << rule.target << std::endl;
        std::cout << "Protocol: " << rule.proto << std::endl;
        if (!rule.srcPort.empty()) {
            std::cout << "Source Port: " << rule.srcPort << std::endl;
        }
        if (!rule.destPort.empty()) {
            std::cout << "Destination Port: " << rule.destPort << std::endl;
        }
        std::cout << "Enabled: " << (rule.enabled ? "Yes" : "No") << std::endl;
        std::cout << "---------" << std::endl;
    }
}

void NetworkConfigAPI::startMonitoring() {
    std::cout << "Starting network monitoring..." << std::endl;
    std::cout << "Monitoring is now active (simulated)" << std::endl;
}

void NetworkConfigAPI::stopMonitoring() {
    std::cout << "Stopping network monitoring..." << std::endl;
    std::cout << "Monitoring has been stopped (simulated)" << std::endl;
}

} // namespace OpenWrtNetwork
