#include "CLICommands.h"
#include "NetworkConfigAPI.h"
#include "NetworkMonitor.h"
#include "ProfileManager.h"
#include "Utils.h"
#include <iostream>
#include <vector>

namespace OpenWrtNetwork {

CLICommands::CLICommands(NetworkConfigAPI* api, NetworkMonitor* monitor, ProfileManager* profileMgr)
    : configAPI(api), networkMonitor(monitor), profileManager(profileMgr) {
}

void CLICommands::showHelp() {
    std::cout << "OpenWrt Network Configuration CLI - Help" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;
    
    std::cout << "VIEW OPERATIONS:" << std::endl;
    std::cout << "  status                  - Show current network status and configuration" << std::endl;
    std::cout << "  interface               - Show detailed interface information" << std::endl;
    std::cout << "  monitor                 - Start real-time network monitoring" << std::endl;
    std::cout << "  stop-monitor            - Stop real-time monitoring" << std::endl;
    std::cout << "  external-ip             - Get external/public IP address" << std::endl;
    std::cout << "  dns-servers             - Show current DNS servers" << std::endl;
    std::cout << "  gateway                 - Show current gateway address" << std::endl;
    std::cout << std::endl;
    
    std::cout << "BASIC CONFIGURATION:" << std::endl;
    std::cout << "  set-mode <mode>         - Set connection mode (dhcp|static)" << std::endl;
    std::cout << "  set-static <ip> <mask> <gw> - Configure static IP settings" << std::endl;
    std::cout << "  set-dns <dns1> [dns2]   - Set DNS servers (space-separated)" << std::endl;
    std::cout << "  set-mtu <size>          - Set MTU size (576-9000)" << std::endl;
    std::cout << "  renew-dhcp              - Renew DHCP lease" << std::endl;
    std::cout << std::endl;
    
    std::cout << "ADVANCED CONFIGURATION:" << std::endl;
    std::cout << "  set-offload <feature> <on|off> - Configure hardware offload" << std::endl;
    std::cout << "     Features: checksum, tso, rss, lso" << std::endl;
    std::cout << "  set-negotiation <mode> [speed] [duplex] - Set link negotiation" << std::endl;
    std::cout << "     Modes: auto, forced" << std::endl;
    std::cout << "     Speed: 10, 100, 1000, 2500, 5000, 10000" << std::endl;
    std::cout << "     Duplex: full, half" << std::endl;
    std::cout << "  set-eee <on|off>        - Enable/disable Energy Efficient Ethernet" << std::endl;
    std::cout << std::endl;
    
    std::cout << "PROFILE MANAGEMENT:" << std::endl;
    std::cout << "  profile-create <name>   - Create new profile from current settings" << std::endl;
    std::cout << "  profile-list            - List all available profiles" << std::endl;
    std::cout << "  profile-show <name>     - Show detailed profile information" << std::endl;
    std::cout << "  profile-activate <name> - Activate a profile" << std::endl;
    std::cout << "  profile-delete <name>   - Delete a profile" << std::endl;
    std::cout << "  profile-auto <name> <conditions> - Set auto-switch conditions" << std::endl;
    std::cout << std::endl;
    
    std::cout << "SYSTEM OPERATIONS:" << std::endl;
    std::cout << "  enable-interface <name> - Enable network interface" << std::endl;
    std::cout << "  disable-interface <name> - Disable network interface" << std::endl;
    std::cout << "  restart-interface <name> - Restart network interface" << std::endl;
    std::cout << "  restore-defaults        - Restore factory default settings" << std::endl;
    std::cout << "  backup-config           - Backup current configuration" << std::endl;
    std::cout << "  restore-backup <file>   - Restore configuration from backup" << std::endl;
    std::cout << std::endl;
    
    std::cout << "OTHER:" << std::endl;
    std::cout << "  help                    - Show this help message" << std::endl;
    std::cout << "  version                 - Show version information" << std::endl;
    std::cout << "  quit/exit               - Exit the application" << std::endl;
}

void CLICommands::showStatus() {
    std::cout << "Network Status Report" << std::endl;
    std::cout << "====================" << std::endl;
    
    // Connection Status
    NetworkStatus status = configAPI->getConnectionStatus();
    std::cout << "\n📡 CONNECTION STATUS:" << std::endl;
    std::cout << "   Status: " << (status.connected ? "🟢 Connected" : "🔴 Disconnected") << std::endl;
    std::cout << "   Type: " << std::uppercase << status.connectionType << std::endl;
    std::cout << "   IPv4: " << status.ipv4Address << std::endl;
    std::cout << "   IPv6: " << status.ipv6Address << std::endl;
    std::cout << "   MAC: " << status.macAddress << std::endl;
    std::cout << "   Link Speed: " << status.linkSpeed << std::endl;
    std::cout << "   Gateway: " << status.gatewayAddress << std::endl;
    std::cout << "   External IP: " << status.externalIP << std::endl;
    
    std::cout << "\n🌐 DNS SERVERS:" << std::endl;
    if (status.dnsServers.empty()) {
        std::cout << "   None configured" << std::endl;
    } else {
        for (size_t i = 0; i < status.dnsServers.size(); ++i) {
            std::cout << "   DNS" << (i+1) << ": " << status.dnsServers[i] << std::endl;
        }
    }
    
    std::cout << "\n📊 REAL-TIME TRAFFIC:" << std::endl;
    std::cout << "   Download: " << std::fixed << std::setprecision(2) << status.downloadSpeed << " Mbps" << std::endl;
    std::cout << "   Upload: " << std::fixed << std::setprecision(2) << status.uploadSpeed << " Mbps" << std::endl;
    
    // Basic Settings
    BasicSettings basic = configAPI->getBasicSettings();
    std::cout << "\n⚙️  BASIC SETTINGS:" << std::endl;
    std::cout << "   Connection Mode: " << std::uppercase << basic.connectionMode << std::endl;
    std::cout << "   Interface Priority: " << basic.interfacePriority << std::endl;
    
    if (basic.connectionMode == "static") {
        std::cout << "   Static IP: " << basic.ipv4Address << "/" << basic.subnetMask << std::endl;
        std::cout << "   Gateway: " << basic.defaultGateway << std::endl;
    }
    
    if (basic.dhcpLeaseTimeRemaining > 0) {
        std::cout << "   DHCP Lease: " << basic.dhcpLeaseTimeRemaining << " seconds remaining" << std::endl;
    }
    
    // Advanced Settings
    AdvancedSettings advanced = configAPI->getAdvancedSettings();
    std::cout << "\n🔧 ADVANCED SETTINGS:" << std::endl;
    std::cout << "   MTU: " << advanced.mtuSize << " bytes";
    if (advanced.mtuSize > 1500) {
        std::cout << " (Jumbo Frames)";
    }
    std::cout << std::endl;
    
    std::cout << "   Link Negotiation: " << std::uppercase << advanced.linkNegotiationMode << std::endl;
    if (advanced.linkNegotiationMode == "forced") {
        std::cout << "   Forced Speed: " << advanced.forcedSpeed << " Mbps" << std::endl;
        std::cout << "   Forced Duplex: " << std::uppercase << advanced.forcedDuplex << std::endl;
    }
    
    std::cout << "\n🚀 HARDWARE OFFLOAD:" << std::endl;
    std::cout << "   Checksum Offload: " << (advanced.checksumOffload ? "✅ Enabled" : "❌ Disabled") << std::endl;
    std::cout << "   TCP Segmentation: " << (advanced.tcpSegmentationOffload ? "✅ Enabled" : "❌ Disabled") << std::endl;
    std::cout << "   RSS: " << (advanced.rssOffload ? "✅ Enabled" : "❌ Disabled") << std::endl;
    std::cout << "   LSO: " << (advanced.lsoOffload ? "✅ Enabled" : "❌ Disabled") << std::endl;
    std::cout << "   Energy Efficient Ethernet: " << (advanced.energyEfficientEthernet ? "✅ Enabled" : "❌ Disabled") << std::endl;
    
    // Active Profile
    std::string activeProfile = profileManager->getActiveProfile();
    if (!activeProfile.empty()) {
        std::cout << "\n📋 ACTIVE PROFILE: " << activeProfile << std::endl;
    }
}

void CLICommands::showInterfaceInfo() {
    InterfaceInfo info = networkMonitor->getInterfaceInfo();
    
    std::cout << "Interface Information: " << info.name << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "Status: " << (info.isUp ? "🟢 UP" : "🔴 DOWN") << std::endl;
    std::cout << "Link: " << (info.isConnected ? "🟢 Connected" : "🔴 No Link") << std::endl;
    std::cout << "MAC Address: " << info.macAddress << std::endl;
    std::cout << "IPv4 Address: " << info.ipv4Address << std::endl;
    std::cout << "IPv6 Address: " << info.ipv6Address << std::endl;
    std::cout << "Link Speed: " << info.linkSpeed << std::endl;
    std::cout << "MTU: " << info.mtu << " bytes" << std::endl;
    
    // Get additional interface statistics
    std::string ethtoolOutput = Utils::executeCommand("ethtool " + info.name);
    
    std::cout << "\nDetailed Information:" << std::endl;
    
    size_t pos = ethtoolOutput.find("Speed:");
    if (pos != std::string::npos) {
        size_t endPos = ethtoolOutput.find("\n", pos);
        std::cout << ethtoolOutput.substr(pos, endPos - pos) << std::endl;
    }
    
    pos = ethtoolOutput.find("Duplex:");
    if (pos != std::string::npos) {
        size_t endPos = ethtoolOutput.find("\n", pos);
        std::cout << ethtoolOutput.substr(pos, endPos - pos) << std::endl;
    }
    
    pos = ethtoolOutput.find("Port:");
    if (pos != std::string::npos) {
        size_t endPos = ethtoolOutput.find("\n", pos);
        std::cout << ethtoolOutput.substr(pos, endPos - pos) << std::endl;
    }
    
    pos = ethtoolOutput.find("Link detected:");
    if (pos != std::string::npos) {
        size_t endPos = ethtoolOutput.find("\n", pos);
        std::cout << ethtoolOutput.substr(pos, endPos - pos) << std::endl;
    }
}

void CLICommands::startMonitoring() {
    if (networkMonitor->isMonitoring()) {
        std::cout << "⚠️  Monitoring is already running" << std::endl;
        return;
    }
    
    std::cout << "🔍 Starting real-time network monitoring..." << std::endl;
    std::cout << "Press Ctrl+C to stop monitoring" << std::endl;
    std::cout << std::endl;
    
    networkMonitor->onStatsUpdate([](const NetworkStats& stats) {
        // Clear current line and show updated stats
        std::cout << "\r📊 Down: " << std::fixed << std::setprecision(2) << std::setw(6) << stats.downloadSpeed 
                  << " Mbps | Up: " << std::setw(6) << stats.uploadSpeed << " Mbps | "
                  << "📥 RX: " << std::setw(10) << stats.totalBytesReceived << " bytes | "
                  << "📤 TX: " << std::setw(10) << stats.totalBytesSent << " bytes";
        std::cout.flush();
    });
    
    networkMonitor->startMonitoring();
    
    // Wait for user to stop monitoring
    while (networkMonitor->isMonitoring()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n\n✅ Monitoring stopped" << std::endl;
}

void CLICommands::stopMonitoring() {
    if (!networkMonitor->isMonitoring()) {
        std::cout << "⚠️  Monitoring is not running" << std::endl;
        return;
    }
    
    networkMonitor->stopMonitoring();
    std::cout << "✅ Monitoring stopped" << std::endl;
}

bool CLICommands::setConnectionMode(const std::string& mode) {
    std::string lowerMode = Utils::toLower(mode);
    
    if (lowerMode != "dhcp" && lowerMode != "static") {
        std::cout << "❌ Invalid mode. Use 'dhcp' or 'static'" << std::endl;
        return false;
    }
    
    std::cout << "🔄 Setting connection mode to " << std::uppercase << lowerMode << "..." << std::endl;
    
    if (configAPI->setConnectionMode(lowerMode)) {
        std::cout << "✅ Connection mode set to " << std::uppercase << lowerMode << std::endl;
        std::cout << "🔄 Restarting interface to apply changes..." << std::endl;
        
        if (configAPI->restartInterface("eth0")) {
            std::cout << "✅ Interface restarted successfully" << std::endl;
        } else {
            std::cout << "⚠️  Interface restart failed - changes may not take effect" << std::endl;
        }
        return true;
    } else {
        std::cout << "❌ Failed to set connection mode" << std::endl;
        return false;
    }
}

bool CLICommands::setStaticIP(const std::string& ip, const std::string& subnet, const std::string& gateway) {
    // Validate inputs
    if (!Utils::isValidIPv4(ip)) {
        std::cout << "❌ Invalid IP address: " << ip << std::endl;
        return false;
    }
    
    if (!Utils::isValidIPv4(subnet)) {
        std::cout << "❌ Invalid subnet mask: " << subnet << std::endl;
        return false;
    }
    
    if (!Utils::isValidIPv4(gateway)) {
        std::cout << "❌ Invalid gateway address: " << gateway << std::endl;
        return false;
    }
    
    std::cout << "🔄 Configuring static IP..." << std::endl;
    std::cout << "   IP Address: " << ip << std::endl;
    std::cout << "   Subnet Mask: " << subnet << std::endl;
    std::cout << "   Gateway: " << gateway << std::endl;
    
    if (configAPI->setStaticIP(ip, subnet, gateway)) {
        std::cout << "✅ Static IP configuration applied" << std::endl;
        std::cout << "🔄 Restarting interface to apply changes..." << std::endl;
        
        if (configAPI->restartInterface("eth0")) {
            std::cout << "✅ Interface restarted successfully" << std::endl;
        } else {
            std::cout << "⚠️  Interface restart failed - changes may not take effect" << std::endl;
        }
        return true;
    } else {
        std::cout << "❌ Failed to set static IP configuration" << std::endl;
        return false;
    }
}

bool CLICommands::setDnsServers(const std::vector<std::string>& dnsServers) {
    if (dnsServers.empty()) {
        std::cout << "❌ No DNS servers specified" << std::endl;
        return false;
    }
    
    // Validate DNS servers
    for (const auto& dns : dnsServers) {
        if (!Utils::isValidIPv4(dns)) {
            std::cout << "❌ Invalid DNS server: " << dns << std::endl;
            return false;
        }
    }
    
    std::cout << "🔄 Setting DNS servers..." << std::endl;
    for (size_t i = 0; i < dnsServers.size(); ++i) {
        std::cout << "   DNS" << (i+1) << ": " << dnsServers[i] << std::endl;
    }
    
    if (configAPI->setDnsServers(dnsServers)) {
        std::cout << "✅ DNS servers updated successfully" << std::endl;
        return true;
    } else {
        std::cout << "❌ Failed to set DNS servers" << std::endl;
        return false;
    }
}

bool CLICommands::setMtuSize(int mtu) {
    if (mtu < 576 || mtu > 9000) {
        std::cout << "❌ MTU size must be between 576 and 9000 bytes" << std::endl;
        return false;
    }
    
    std::cout << "🔄 Setting MTU size to " << mtu << " bytes..." << std::endl;
    
    if (configAPI->setMtuSize(mtu)) {
        std::cout << "✅ MTU size set to " << mtu << " bytes" << std::endl;
        if (mtu > 1500) {
            std::cout << "   📌 Note: Jumbo Frames enabled" << std::endl;
        }
        std::cout << "🔄 Restarting interface to apply changes..." << std::endl;
        
        if (configAPI->restartInterface("eth0")) {
            std::cout << "✅ Interface restarted successfully" << std::endl;
        } else {
            std::cout << "⚠️  Interface restart failed - changes may not take effect" << std::endl;
        }
        return true;
    } else {
        std::cout << "❌ Failed to set MTU size" << std::endl;
        return false;
    }
}

void CLICommands::listProfiles() {
    auto profiles = profileManager->getAllProfiles();
    std::string activeProfile = profileManager->getActiveProfile();
    
    std::cout << "Network Profiles" << std::endl;
    std::cout << "================" << std::endl;
    
    if (profiles.empty()) {
        std::cout << "No profiles found. Use 'profile-create <name>' to create one." << std::endl;
        return;
    }
    
    for (const auto& profile : profiles) {
        std::cout << "\n📋 " << profile.name;
        if (profile.name == activeProfile) {
            std::cout << " [🟢 ACTIVE]";
        }
        std::cout << std::endl;
        
        std::cout << "   Description: " << profile.description << std::endl;
        std::cout << "   Connection: " << std::uppercase << profile.basicSettings.connectionMode << std::endl;
        
        if (profile.basicSettings.connectionMode == "static") {
            std::cout << "   IP: " << profile.basicSettings.ipv4Address << "/" 
                      << profile.basicSettings.subnetMask << std::endl;
            std::cout << "   Gateway: " << profile.basicSettings.defaultGateway << std::endl;
        }
        
        std::cout << "   MTU: " << profile.advancedSettings.mtuSize << " bytes" << std::endl;
        
        if (!profile.autoApplyConditions.empty()) {
            std::cout << "   Auto-switch: ";
            for (size_t i = 0; i < profile.autoApplyConditions.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << profile.autoApplyConditions[i];
            }
            std::cout << std::endl;
        }
        
        auto timeT = std::chrono::system_clock::to_time_t(profile.lastUsed);
        std::cout << "   Last used: " << std::ctime(&timeT);
    }
}

bool CLICommands::createProfile(const std::string& name, const std::string& description) {
    if (name.empty()) {
        std::cout << "Profile name cannot be empty" << std::endl;
        return false;
    }
    
    // Check if profile already exists
    auto existingProfile = profileManager->getProfile(name);
    if (!existingProfile.name.empty()) {
        std::cout << "Profile '" << name << "' already exists" << std::endl;
        return false;
    }
    
    std::cout << "Creating profile '" << name << "' from current settings..." << std::endl;
    
    ConnectionProfile profile;
    profile.name = name;
    profile.description = description.empty() ? "Created via CLI" : description;
    profile.basicSettings = configAPI->getBasicSettings();
    profile.advancedSettings = configAPI->getAdvancedSettings();
    profile.lastUsed = std::chrono::system_clock::now();
    
    if (profileManager->createProfile(profile)) {
        std::cout << "Profile '" << name << "' created successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to create profile '" << name << "'" << std::endl;
        return false;
    }
}

bool CLICommands::activateProfile(const std::string& name) {
    auto profile = profileManager->getProfile(name);
    if (profile.name.empty()) {
        std::cout << "Profile '" << name << "' not found" << std::endl;
        return false;
    }
    
    std::cout << "Activating profile '" << name << "'..." << std::endl;
    
    if (profileManager->activateProfile(name, *configAPI)) {
        std::cout << "Profile '" << name << "' activated successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to activate profile '" << name << "'" << std::endl;
        return false;
    }
}

bool CLICommands::deleteProfile(const std::string& name) {
    auto profile = profileManager->getProfile(name);
    if (profile.name.empty()) {
        std::cout << "Profile '" << name << "' not found" << std::endl;
        return false;
    }
    
    std::cout << "Are you sure you want to delete profile '" << name << "'? (y/N): ";
    std::string confirmation;
    std::getline(std::cin, confirmation);
    
    if (Utils::toLower(confirmation) != "y" && Utils::toLower(confirmation) != "yes") {
        std::cout << "Profile deletion cancelled" << std::endl;
        return false;
    }
    
    if (profileManager->deleteProfile(name)) {
        std::cout << "Profile '" << name << "' deleted successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to delete profile '" << name << "'" << std::endl;
        return false;
    }
}

bool CLICommands::restoreDefaults() {
    std::cout << "WARNING: This will restore all network settings to factory defaults." << std::endl;
    std::cout << "         All custom configurations will be lost." << std::endl;
    std::cout << "         Are you sure? (y/N): ";
    
    std::string confirmation;
    std::getline(std::cin, confirmation);
    
    if (Utils::toLower(confirmation) != "y" && Utils::toLower(confirmation) != "yes") {
        std::cout << "Operation cancelled" << std::endl;
        return false;
    }
    
    std::cout << "Restoring default settings..." << std::endl;
    
    if (configAPI->restoreDefaults()) {
        std::cout << "Default settings restored" << std::endl;
        std::cout << "Restarting interface to apply changes..." << std::endl;
        
        if (configAPI->restartInterface("eth0")) {
            std::cout << "Interface restarted successfully" << std::endl;
        } else {
            std::cout << "Interface restart failed - you may need to restart manually" << std::endl;
        }
        return true;
    } else {
        std::cout << "Failed to restore default settings" << std::endl;
        return false;
    }
}

void CLICommands::showVersion() {
    std::cout << "OpenWrt Network Configuration CLI" << std::endl;
    std::cout << "Version 1.0.0" << std::endl;
    std::cout << "Built for OpenWrt systems" << std::endl;
    std::cout << "Supports wired network configuration and monitoring" << std::endl;
    std::cout << "Advanced features: Static routes, VLAN, Firewall, Bridges, NAT" << std::endl;
}

// Static Route Management Implementation
void CLICommands::listStaticRoutes() {
    auto routes = configAPI->getStaticRoutes();
    std::cout << "Static Routes:" << std::endl;
    std::cout << "Target\t\tGateway\t\tInterface\tMetric\tStatus" << std::endl;
    std::cout << "------\t\t-------\t\t---------\t------\t------" << std::endl;
    
    for (const auto& route : routes) {
        std::cout << route.target << "\t" << route.gateway << "\t" 
                  << route.interface << "\t" << route.metric << "\t" 
                  << (route.enabled ? "Enabled" : "Disabled") << std::endl;
    }
}

bool CLICommands::addStaticRoute(const std::string& target, const std::string& gateway, const std::string& interface, int metric) {
    StaticRoute route;
    route.target = target;
    route.gateway = gateway;
    route.interface = interface;
    route.metric = metric;
    route.enabled = true;
    
    if (configAPI->addStaticRoute(route)) {
        std::cout << "Static route added successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to add static route" << std::endl;
        return false;
    }
}

bool CLICommands::removeStaticRoute(const std::string& target) {
    if (configAPI->removeStaticRoute(target)) {
        std::cout << "Static route removed successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to remove static route" << std::endl;
        return false;
    }
}

bool CLICommands::enableStaticRoute(const std::string& target, bool enabled) {
    if (configAPI->enableStaticRoute(target, enabled)) {
        std::cout << "Static route " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to " << (enabled ? "enable" : "disable") << " static route" << std::endl;
        return false;
    }
}

// VLAN Configuration Implementation
void CLICommands::listVlans() {
    auto vlans = configAPI->getVlanConfigs();
    std::cout << "VLAN Configurations:" << std::endl;
    std::cout << "Name\t\tVLAN ID\tBase Interface\tProtocol\tStatus" << std::endl;
    std::cout << "----\t\t-------\t--------------\t--------\t------" << std::endl;
    
    for (const auto& vlan : vlans) {
        std::cout << vlan.name << "\t" << vlan.vlanId << "\t" 
                  << vlan.baseInterface << "\t" << vlan.protocol << "\t" 
                  << (vlan.enabled ? "Enabled" : "Disabled") << std::endl;
    }
}

bool CLICommands::addVlan(const std::string& name, int vlanId, const std::string& baseInterface, const std::string& protocol) {
    VlanConfig vlan;
    vlan.name = name;
    vlan.vlanId = vlanId;
    vlan.baseInterface = baseInterface;
    vlan.protocol = protocol;
    vlan.enabled = true;
    
    if (configAPI->addVlanConfig(vlan)) {
        std::cout << "VLAN configuration added successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to add VLAN configuration" << std::endl;
        return false;
    }
}

bool CLICommands::removeVlan(const std::string& name) {
    if (configAPI->removeVlanConfig(name)) {
        std::cout << "VLAN configuration removed successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to remove VLAN configuration" << std::endl;
        return false;
    }
}

bool CLICommands::enableVlan(const std::string& name, bool enabled) {
    if (configAPI->enableVlanConfig(name, enabled)) {
        std::cout << "VLAN configuration " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to " << (enabled ? "enable" : "disable") << " VLAN configuration" << std::endl;
        return false;
    }
}

// Firewall Rule Management Implementation
void CLICommands::listFirewallRules() {
    auto rules = configAPI->getFirewallRules();
    std::cout << "Firewall Rules:" << std::endl;
    std::cout << "Name\t\tSource\tDestination\tTarget\tProtocol\tStatus" << std::endl;
    std::cout << "----\t\t------\t-----------\t------\t--------\t------" << std::endl;
    
    for (const auto& rule : rules) {
        std::cout << rule.name << "\t" << rule.src << "\t" 
                  << rule.dest << "\t" << rule.target << "\t" 
                  << rule.proto << "\t" << (rule.enabled ? "Enabled" : "Disabled") << std::endl;
    }
}

bool CLICommands::addFirewallRule(const std::string& name, const std::string& src, const std::string& dest, const std::string& target, const std::string& proto) {
    FirewallRule rule;
    rule.name = name;
    rule.src = src;
    rule.dest = dest;
    rule.target = target;
    rule.proto = proto;
    rule.enabled = true;
    
    if (configAPI->addFirewallRule(rule)) {
        std::cout << "Firewall rule added successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to add firewall rule" << std::endl;
        return false;
    }
}

bool CLICommands::removeFirewallRule(const std::string& name) {
    if (configAPI->removeFirewallRule(name)) {
        std::cout << "Firewall rule removed successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to remove firewall rule" << std::endl;
        return false;
    }
}

bool CLICommands::enableFirewallRule(const std::string& name, bool enabled) {
    if (configAPI->enableFirewallRule(name, enabled)) {
        std::cout << "Firewall rule " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to " << (enabled ? "enable" : "disable") << " firewall rule" << std::endl;
        return false;
    }
}

// Bridge Configuration Implementation
void CLICommands::listBridges() {
    auto bridges = configAPI->getBridgeConfigs();
    std::cout << "Bridge Configurations:" << std::endl;
    std::cout << "Name\t\tInterfaces\t\t\tSTP\tStatus" << std::endl;
    std::cout << "----\t\t----------\t\t\t---\t------" << std::endl;
    
    for (const auto& bridge : bridges) {
        std::string interfaces;
        for (size_t i = 0; i < bridge.interfaces.size(); ++i) {
            if (i > 0) interfaces += ",";
            interfaces += bridge.interfaces[i];
        }
        std::cout << bridge.name << "\t" << interfaces << "\t\t" 
                  << bridge.stp << "\t" << (bridge.enabled ? "Enabled" : "Disabled") << std::endl;
    }
}

bool CLICommands::addBridge(const std::string& name, const std::vector<std::string>& interfaces, const std::string& stp) {
    BridgeConfig bridge;
    bridge.name = name;
    bridge.interfaces = interfaces;
    bridge.stp = stp;
    bridge.forwardDelay = "15";
    bridge.maxAge = "20";
    bridge.helloTime = "2";
    bridge.enabled = true;
    
    if (configAPI->addBridgeConfig(bridge)) {
        std::cout << "Bridge configuration added successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to add bridge configuration" << std::endl;
        return false;
    }
}

bool CLICommands::removeBridge(const std::string& name) {
    if (configAPI->removeBridgeConfig(name)) {
        std::cout << "Bridge configuration removed successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to remove bridge configuration" << std::endl;
        return false;
    }
}

bool CLICommands::enableBridge(const std::string& name, bool enabled) {
    if (configAPI->enableBridgeConfig(name, enabled)) {
        std::cout << "Bridge configuration " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to " << (enabled ? "enable" : "disable") << " bridge configuration" << std::endl;
        return false;
    }
}

bool CLICommands::addInterfaceToBridge(const std::string& bridgeName, const std::string& interface) {
    if (configAPI->addInterfaceToBridge(bridgeName, interface)) {
        std::cout << "Interface added to bridge successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to add interface to bridge" << std::endl;
        return false;
    }
}

bool CLICommands::removeInterfaceFromBridge(const std::string& bridgeName, const std::string& interface) {
    if (configAPI->removeInterfaceFromBridge(bridgeName, interface)) {
        std::cout << "Interface removed from bridge successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to remove interface from bridge" << std::endl;
        return false;
    }
}

// NAT Rule Management Implementation
void CLICommands::listNatRules() {
    auto rules = configAPI->getNatRules();
    std::cout << "NAT Rules:" << std::endl;
    std::cout << "Name\t\tType\tSource\tDestination\tTarget\tStatus" << std::endl;
    std::cout << "----\t\t----\t------\t-----------\t------\t------" << std::endl;
    
    for (const auto& rule : rules) {
        std::cout << rule.name << "\t" << rule.type << "\t" 
                  << rule.src << "\t" << rule.dest << "\t" 
                  << rule.target << "\t" << (rule.enabled ? "Enabled" : "Disabled") << std::endl;
    }
}

bool CLICommands::addNatRule(const std::string& name, const std::string& type, const std::string& src, const std::string& dest, const std::string& target) {
    NatRule rule;
    rule.name = name;
    rule.type = type;
    rule.src = src;
    rule.dest = dest;
    rule.target = target;
    rule.enabled = true;
    
    if (configAPI->addNatRule(rule)) {
        std::cout << "NAT rule added successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to add NAT rule" << std::endl;
        return false;
    }
}

bool CLICommands::removeNatRule(const std::string& name) {
    if (configAPI->removeNatRule(name)) {
        std::cout << "NAT rule removed successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to remove NAT rule" << std::endl;
        return false;
    }
}

bool CLICommands::enableNatRule(const std::string& name, bool enabled) {
    if (configAPI->enableNatRule(name, enabled)) {
        std::cout << "NAT rule " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to " << (enabled ? "enable" : "disable") << " NAT rule" << std::endl;
        return false;
    }
}

} // namespace OpenWrtNetwork
