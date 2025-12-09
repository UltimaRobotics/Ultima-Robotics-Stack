#include "../include/NetworkConfigAPI.h"
#include "../include/NetworkMonitor.h"
#include "../include/ProfileManager.h"
#include "../include/NetworkExtensions.h"
#include "../include/Utils.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <getopt.h>
#include <chrono>
#include <cstdlib>

using namespace OpenWrtNetwork;

bool hasTestModeFlag(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-t" || std::string(argv[i]) == "--test") {
            return true;
        }
    }
    return false;
}

class CLIApp {
private:
    std::unique_ptr<NetworkConfigAPI> configAPI;
    std::unique_ptr<NetworkMonitor> monitor;
    std::unique_ptr<ProfileManager> profileManager;
    bool running;
    std::string packageConfigPath;
    bool testMode;
    bool verboseMode;
    bool profilesEnabled;
    bool backupsEnabled;

public:
    CLIApp(const std::string& configPath = "", bool isTestMode = false, bool isVerboseMode = false) : running(false), packageConfigPath(configPath), testMode(isTestMode), verboseMode(isVerboseMode), profilesEnabled(false), backupsEnabled(false) {}
    
    ~CLIApp() {}
    
    NetworkConfigAPI* getConfigAPI() { return configAPI.get(); }

    bool initialize() {
        // Set verbose mode in Utils first
        Utils::setVerboseMode(verboseMode);
        
        configAPI = std::make_unique<NetworkConfigAPI>();
        monitor = std::make_unique<NetworkMonitor>();
        profileManager = std::make_unique<ProfileManager>();
        
        // Set test mode in NetworkConfigAPI
        configAPI->setTestMode(testMode);
        
        // Load package configuration if provided
        if (!packageConfigPath.empty()) {
            if (!configAPI->loadPackageConfig(packageConfigPath)) {
                std::cerr << "Failed to load package configuration from: " << packageConfigPath << std::endl;
                return false;
            }
        }
        
        if (!configAPI->initialize()) {
            std::cerr << "Failed to initialize Network Config API" << std::endl;
            return false;
        }
        
        // Get the loaded configuration
        PackageConfig pkgConfig = configAPI->getPackageConfig();
        
        if (!testMode) {
            // Helper function to create directory with fallback
            auto createDirectoryWithFallback = [](const std::string& originalPath, const std::string& fallbackPath, const std::string& dirName) -> std::string {
                std::cout << "Checking " << dirName << " directory: " << originalPath << std::endl;
                if (!Utils::fileExists(originalPath)) {
                    std::cout << "Directory does not exist, creating: " << originalPath << std::endl;
                    if (!Utils::createDirectory(originalPath)) {
                        std::cerr << "Failed to create " << dirName << " directory: " << originalPath << std::endl;
                        std::cerr << "Trying fallback directory: " << fallbackPath << std::endl;
                        
                        if (!Utils::createDirectory(fallbackPath)) {
                            std::cerr << "Failed to create fallback " << dirName << " directory: " << fallbackPath << std::endl;
                            std::cerr << "This may be due to insufficient permissions. Try running with sudo or check directory permissions." << std::endl;
                            return "";
                        } else {
                            std::cout << "Successfully created fallback " << dirName << " directory." << std::endl;
                            return fallbackPath;
                        }
                    } else {
                        std::cout << "Successfully created " << dirName << " directory." << std::endl;
                        return originalPath;
                    }
                } else {
                    std::cout << dirName << " directory already exists." << std::endl;
                    return originalPath;
                }
            };
            
            // Create directories with fallback to user home
            std::string homeDir = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
            std::string profilesFallback = homeDir + "/.ultima-ur-base-network-mann/network-profiles";
            std::string backupsFallback = homeDir + "/.ultima-ur-base-network-mann/network-backups";
            
            std::string actualProfilesDir = createDirectoryWithFallback(
                pkgConfig.networkProfilesDir, profilesFallback, "network profiles");
            std::string actualBackupsDir = createDirectoryWithFallback(
                pkgConfig.networkBackupsDir, backupsFallback, "network backups");
            
            if (actualProfilesDir.empty() || actualBackupsDir.empty()) {
                std::cerr << "Warning: Failed to create profile/backup directories. Profile and backup features will be disabled." << std::endl;
                std::cout << "Continuing with basic network functionality only..." << std::endl;
                
                // Set flags to disable profile and backup features
                pkgConfig.networkProfilesDir = "";
                pkgConfig.networkBackupsDir = "";
            } else {
                // Update package config with actual directories if fallback was used
                if (actualProfilesDir != pkgConfig.networkProfilesDir) {
                    std::cout << "Using fallback profiles directory: " << actualProfilesDir << std::endl;
                    pkgConfig.networkProfilesDir = actualProfilesDir;
                }
                if (actualBackupsDir != pkgConfig.networkBackupsDir) {
                    std::cout << "Using fallback backups directory: " << actualBackupsDir << std::endl;
                    pkgConfig.networkBackupsDir = actualBackupsDir;
                }
            }
            
            // Initialize monitor with configuration
            MonitorConfig monitorConfig;
            monitorConfig.resolvConfPath = pkgConfig.resolvConfPath;
            monitorConfig.defaultInterface = pkgConfig.defaultInterface;
            
            if (!monitor->initialize(monitorConfig)) {
                std::cerr << "Failed to initialize Network Monitor" << std::endl;
                return false;
            }
            
            // Initialize profile manager with configuration (optional)
            if (!pkgConfig.networkProfilesDir.empty()) {
                if (!profileManager->initialize(pkgConfig.networkProfilesDir)) {
                    std::cerr << "Warning: Failed to initialize Profile Manager. Profile features will be disabled." << std::endl;
                    pkgConfig.networkProfilesDir = "";
                    profilesEnabled = false;
                } else {
                    profilesEnabled = true;
                }
            } else {
                std::cout << "Profile Manager disabled due to directory creation failure." << std::endl;
                profilesEnabled = false;
            }
            
            // Set backup directory in BackupManager (optional)
            if (!pkgConfig.networkBackupsDir.empty()) {
                BackupManager::setBackupDirectory(pkgConfig.networkBackupsDir);
                
                // Initialize backup manager (optional)
                if (!BackupManager::initialize()) {
                    std::cerr << "Warning: Failed to initialize Backup Manager. Backup features will be disabled." << std::endl;
                    pkgConfig.networkBackupsDir = "";
                    backupsEnabled = false;
                } else {
                    backupsEnabled = true;
                }
            } else {
                std::cout << "Backup Manager disabled due to directory creation failure." << std::endl;
                backupsEnabled = false;
            }
        }
        
        running = true;
        return true;
    }
    
    void run() {
        std::cout << "OpenWrt Network Configuration CLI";
        if (testMode) {
            std::cout << " (TEST MODE - No system commands will be executed)";
        }
        if (verboseMode) {
            std::cout << " (VERBOSE MODE - All commands will be shown)";
        }
        std::cout << std::endl;
        std::cout << "Type 'help' for available commands or 'quit' to exit" << std::endl;
        std::cout << std::endl;
        
        while (running) {
            std::cout << "openwrt-network> ";
            std::string command;
            std::getline(std::cin, command);
            
            if (command.empty()) {
                continue;
            }
            
            processCommand(command);
        }
    }
    
private:
    void processCommand(const std::string& command) {
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        
        if (cmd == "quit" || cmd == "exit") {
            running = false;
        } else if (cmd == "help") {
            showHelp();
        } else if (cmd == "status") {
            showStatus();
        } else if (cmd == "monitor") {
            startMonitoring();
        } else if (cmd == "stop-monitor") {
            stopMonitoring();
        } else if (cmd == "interface") {
            showInterfaceInfo();
        } else if (cmd == "set-mode") {
            std::string mode;
            iss >> mode;
            setConnectionMode(mode);
        } else if (cmd == "set-static") {
            std::string ip, subnet, gateway;
            iss >> ip >> subnet >> gateway;
            setStaticIP(ip, subnet, gateway);
        } else if (cmd == "set-dns") {
            std::string dnsLine;
            std::getline(iss, dnsLine);
            setDnsServers(Utils::trim(dnsLine));
        } else if (cmd == "set-mtu") {
            int mtu;
            iss >> mtu;
            setMtuSize(mtu);
        } else if (cmd == "profile-create") {
            std::string profileName;
            iss >> profileName;
            createProfile(profileName);
        } else if (cmd == "profile-list") {
            listProfiles();
        } else if (cmd == "profile-activate") {
            std::string profileName;
            iss >> profileName;
            activateProfile(profileName);
        } else if (cmd == "profile-delete") {
            std::string profileName;
            iss >> profileName;
            deleteProfile(profileName);
        } else if (cmd == "restore-defaults") {
            restoreDefaults();
        } else if (cmd == "restart-interface") {
            restartInterface();
        } else if (cmd == "list-static-routes") {
            listStaticRoutes();
        } else if (cmd == "add-static-route") {
            std::string target, gateway, interface;
            int metric;
            iss >> target >> gateway >> interface >> metric;
            addStaticRoute(target, gateway, interface, metric);
        } else if (cmd == "remove-static-route") {
            std::string target;
            iss >> target;
            removeStaticRoute(target);
        } else if (cmd == "enable-static-route") {
            std::string target;
            std::string enabled;
            iss >> target >> enabled;
            enableStaticRoute(target, enabled == "true" || enabled == "1");
        } else if (cmd == "list-vlans") {
            listVlans();
        } else if (cmd == "add-vlan") {
            std::string name, baseInterface, protocol;
            int vlanId;
            iss >> name >> vlanId >> baseInterface >> protocol;
            addVlan(name, vlanId, baseInterface, protocol);
        } else if (cmd == "remove-vlan") {
            std::string name;
            iss >> name;
            removeVlan(name);
        } else if (cmd == "enable-vlan") {
            std::string name, enabled;
            iss >> name >> enabled;
            enableVlan(name, enabled == "true" || enabled == "1");
        } else if (cmd == "list-firewall-rules") {
            listFirewallRules();
        } else if (cmd == "add-firewall-rule") {
            std::string name, src, dest, target, proto;
            iss >> name >> src >> dest >> target >> proto;
            addFirewallRule(name, src, dest, target, proto);
        } else if (cmd == "remove-firewall-rule") {
            std::string name;
            iss >> name;
            removeFirewallRule(name);
        } else if (cmd == "enable-firewall-rule") {
            std::string name, enabled;
            iss >> name >> enabled;
            enableFirewallRule(name, enabled == "true" || enabled == "1");
        } else if (cmd == "list-bridges") {
            listBridges();
        } else if (cmd == "add-bridge") {
            std::string name, stp;
            std::vector<std::string> interfaces;
            iss >> name >> stp;
            std::string interface;
            while (iss >> interface) {
                interfaces.push_back(interface);
            }
            addBridge(name, interfaces, stp);
        } else if (cmd == "remove-bridge") {
            std::string name;
            iss >> name;
            removeBridge(name);
        } else if (cmd == "enable-bridge") {
            std::string name, enabled;
            iss >> name >> enabled;
            enableBridge(name, enabled == "true" || enabled == "1");
        } else if (cmd == "list-nat-rules") {
            listNatRules();
        } else if (cmd == "add-nat-rule") {
            std::string name, type, src, dest, target;
            iss >> name >> type >> src >> dest >> target;
            addNatRule(name, type, src, dest, target);
        } else if (cmd == "remove-nat-rule") {
            std::string name;
            iss >> name;
            removeNatRule(name);
        } else if (cmd == "enable-nat-rule") {
            std::string name, enabled;
            iss >> name >> enabled;
            enableNatRule(name, enabled == "true" || enabled == "1");
        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
            std::cout << "Type 'help' for available commands" << std::endl;
        }
    }
    
    void showHelp() {
        std::cout << "OpenWrt Network Configuration CLI" << std::endl;
        std::cout << "Usage: openwrt-network-cli [options]" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -pkg_config <file>  Path to package configuration JSON file" << std::endl;
        std::cout << "  -t                   Enable test mode (no system commands)" << std::endl;
        std::cout << "  -v                   Enable verbose mode (show all commands and results)" << std::endl;
        std::cout << "  -h, --help          Show this help message" << std::endl;
        std::cout << std::endl;
        std::cout << "Available commands:" << std::endl;
        std::cout << "  help                    - Show this help message" << std::endl;
        std::cout << "  status                  - Show current network status" << std::endl;
        std::cout << "  monitor                 - Start real-time monitoring" << std::endl;
        std::cout << "  stop-monitor            - Stop real-time monitoring" << std::endl;
        std::cout << "  interface               - Show interface information" << std::endl;
        std::cout << "  set-mode <mode>         - Set connection mode (dhcp/static)" << std::endl;
        std::cout << "  set-static <ip> <mask> <gw> - Set static IP configuration" << std::endl;
        std::cout << "  set-dns <dns1> [dns2]   - Set DNS servers" << std::endl;
        std::cout << "  set-mtu <size>          - Set MTU size (576-9000)" << std::endl;
        
        if (profilesEnabled) {
            std::cout << "  profile-create <name>   - Create a new profile" << std::endl;
            std::cout << "  profile-list            - List all profiles" << std::endl;
            std::cout << "  profile-activate <name> - Activate a profile" << std::endl;
            std::cout << "  profile-delete <name>   - Delete a profile" << std::endl;
        } else {
            std::cout << "  # Profile commands disabled - could not create profile directories" << std::endl;
        }
        
        std::cout << "  restore-defaults        - Restore default settings" << std::endl;
        std::cout << "  restart-interface       - Restart network interface" << std::endl;
        std::cout << std::endl;
        std::cout << "Static Routes:" << std::endl;
        std::cout << "  list-static-routes      - List all static routes" << std::endl;
        std::cout << "  add-static-route <target> <gw> <iface> <metric> - Add static route" << std::endl;
        std::cout << "  remove-static-route <target> - Remove static route" << std::endl;
        std::cout << "  enable-static-route <target> <true/false> - Enable/disable route" << std::endl;
        std::cout << std::endl;
        std::cout << "VLAN Configuration:" << std::endl;
        std::cout << "  list-vlans              - List all VLAN configurations" << std::endl;
        std::cout << "  add-vlan <name> <id> <base> <protocol> - Add VLAN" << std::endl;
        std::cout << "  remove-vlan <name>      - Remove VLAN" << std::endl;
        std::cout << "  enable-vlan <name> <true/false> - Enable/disable VLAN" << std::endl;
        std::cout << std::endl;
        std::cout << "Firewall Configuration:" << std::endl;
        std::cout << "  list-firewall-rules     - List all firewall rules" << std::endl;
        std::cout << "  add-firewall-rule <name> <src> <dest> <target> <proto> - Add rule" << std::endl;
        std::cout << "  remove-firewall-rule <name> - Remove firewall rule" << std::endl;
        std::cout << "  enable-firewall-rule <name> <true/false> - Enable/disable rule" << std::endl;
        std::cout << std::endl;
        std::cout << "Bridge Configuration:" << std::endl;
        std::cout << "  list-bridges            - List all bridge configurations" << std::endl;
        std::cout << "  add-bridge <name> <stp> [interfaces...] - Add bridge" << std::endl;
        std::cout << "  remove-bridge <name>    - Remove bridge" << std::endl;
        std::cout << "  enable-bridge <name> <true/false> - Enable/disable bridge" << std::endl;
        std::cout << std::endl;
        std::cout << "NAT Configuration:" << std::endl;
        std::cout << "  list-nat-rules          - List all NAT rules" << std::endl;
        std::cout << "  add-nat-rule <name> <type> <src> <dest> <target> - Add NAT rule" << std::endl;
        std::cout << "  remove-nat-rule <name>  - Remove NAT rule" << std::endl;
        std::cout << "  enable-nat-rule <name> <true/false> - Enable/disable NAT rule" << std::endl;
        std::cout << std::endl;
        std::cout << "  quit/exit               - Exit the application" << std::endl;
    }
    
    void showStatus() {
        std::cout << "Network Status:" << std::endl;
        std::cout << "================" << std::endl;
        
        NetworkStatus status = configAPI->getConnectionStatus();
        
        std::cout << "Connection Status: " << (status.connected ? "Connected" : "Disconnected") << std::endl;
        std::cout << "Connection Type: " << status.connectionType << std::endl;
        std::cout << "IPv4 Address: " << status.ipv4Address << std::endl;
        std::cout << "IPv6 Address: " << status.ipv6Address << std::endl;
        std::cout << "MAC Address: " << status.macAddress << std::endl;
        std::cout << "Link Speed: " << status.linkSpeed << std::endl;
        std::cout << "Gateway: " << status.gatewayAddress << std::endl;
        std::cout << "External IP: " << status.externalIP << std::endl;
        
        std::cout << "DNS Servers: ";
        for (size_t i = 0; i < status.dnsServers.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << status.dnsServers[i];
        }
        std::cout << std::endl;
        
        std::cout << "Download Speed: " << std::fixed << std::setprecision(2) << status.downloadSpeed << " Mbps" << std::endl;
        std::cout << "Upload Speed: " << std::fixed << std::setprecision(2) << status.uploadSpeed << " Mbps" << std::endl;
        
        BasicSettings basic = configAPI->getBasicSettings();
        std::cout << "\nBasic Settings:" << std::endl;
        std::cout << "Connection Mode: " << basic.connectionMode << std::endl;
        std::cout << "Interface Priority: " << basic.interfacePriority << std::endl;
        
        AdvancedSettings advanced = configAPI->getAdvancedSettings();
        std::cout << "\nAdvanced Settings:" << std::endl;
        std::cout << "MTU Size: " << advanced.mtuSize << std::endl;
        std::cout << "Link Negotiation: " << advanced.linkNegotiationMode << std::endl;
        std::cout << "Checksum Offload: " << (advanced.checksumOffload ? "Enabled" : "Disabled") << std::endl;
        std::cout << "TCP Segmentation Offload: " << (advanced.tcpSegmentationOffload ? "Enabled" : "Disabled") << std::endl;
        std::cout << "RSS Offload: " << (advanced.rssOffload ? "Enabled" : "Disabled") << std::endl;
        std::cout << "Energy Efficient Ethernet: " << (advanced.energyEfficientEthernet ? "Enabled" : "Disabled") << std::endl;
    }
    
    void startMonitoring() {
        if (monitor->isMonitoring()) {
            std::cout << "Monitoring is already running" << std::endl;
            return;
        }
        
        std::cout << "Starting real-time monitoring... Press Ctrl+C to stop" << std::endl;
        
        monitor->onStatsUpdate([this](const NetworkStats& stats) {
            std::cout << "\rDownload: " << std::fixed << std::setprecision(2) << stats.downloadSpeed 
                      << " Mbps | Upload: " << stats.uploadSpeed << " Mbps | ";
            std::cout.flush();
        });
        
        monitor->startMonitoring();
        
        // Wait for user interrupt
        while (monitor->isMonitoring()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void stopMonitoring() {
        if (!monitor->isMonitoring()) {
            std::cout << "Monitoring is not running" << std::endl;
            return;
        }
        
        monitor->stopMonitoring();
        std::cout << "\nMonitoring stopped" << std::endl;
    }
    
    void showInterfaceInfo() {
        InterfaceInfo info = monitor->getInterfaceInfo();
        
        std::cout << "Interface Information:" << std::endl;
        std::cout << "======================" << std::endl;
        std::cout << "Name: " << info.name << std::endl;
        std::cout << "Status: " << (info.isUp ? "UP" : "DOWN") << std::endl;
        std::cout << "Connected: " << (info.isConnected ? "Yes" : "No") << std::endl;
        std::cout << "MAC Address: " << info.macAddress << std::endl;
        std::cout << "IPv4 Address: " << info.ipv4Address << std::endl;
        std::cout << "IPv6 Address: " << info.ipv6Address << std::endl;
        std::cout << "Link Speed: " << info.linkSpeed << std::endl;
        std::cout << "MTU: " << info.mtu << std::endl;
    }
    
    void setConnectionMode(const std::string& mode) {
        if (mode != "dhcp" && mode != "static") {
            std::cout << "Invalid mode. Use 'dhcp' or 'static'" << std::endl;
            return;
        }
        
        if (configAPI->setConnectionMode(mode)) {
            std::cout << "Connection mode set to " << mode << std::endl;
            std::cout << "Restarting interface to apply changes..." << std::endl;
            configAPI->restartInterface("eth0");
        } else {
            std::cout << "Failed to set connection mode" << std::endl;
        }
    }
    
    void setStaticIP(const std::string& ip, const std::string& subnet, const std::string& gateway) {
        if (!Utils::isValidIPv4(ip)) {
            std::cout << "Invalid IP address" << std::endl;
            return;
        }
        
        if (!Utils::isValidIPv4(subnet)) {
            std::cout << "Invalid subnet mask" << std::endl;
            return;
        }
        
        if (!Utils::isValidIPv4(gateway)) {
            std::cout << "Invalid gateway address" << std::endl;
            return;
        }
        
        if (configAPI->setStaticIP(ip, subnet, gateway)) {
            std::cout << "Static IP configuration set" << std::endl;
            std::cout << "Restarting interface to apply changes..." << std::endl;
            configAPI->restartInterface("eth0");
        } else {
            std::cout << "Failed to set static IP configuration" << std::endl;
        }
    }
    
    void setDnsServers(const std::string& dnsLine) {
        auto dnsServers = Utils::split(dnsLine, ' ');
        
        for (const auto& dns : dnsServers) {
            if (!Utils::isValidIPv4(dns)) {
                std::cout << "Invalid DNS server: " << dns << std::endl;
                return;
            }
        }
        
        if (configAPI->setDnsServers(dnsServers)) {
            std::cout << "DNS servers set to: ";
            for (size_t i = 0; i < dnsServers.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << dnsServers[i];
            }
            std::cout << std::endl;
        } else {
            std::cout << "Failed to set DNS servers" << std::endl;
        }
    }
    
    void setMtuSize(int mtu) {
        if (mtu < 576 || mtu > 9000) {
            std::cout << "MTU size must be between 576 and 9000" << std::endl;
            return;
        }
        
        if (configAPI->setMtuSize(mtu)) {
            std::cout << "MTU size set to " << mtu << std::endl;
            std::cout << "Restarting interface to apply changes..." << std::endl;
            configAPI->restartInterface("eth0");
        } else {
            std::cout << "Failed to set MTU size" << std::endl;
        }
    }
    
    void createProfile(const std::string& name) {
        if (!profilesEnabled) {
            std::cout << "Profile features are disabled. Could not create profile directories during initialization." << std::endl;
            return;
        }
        
        if (name.empty()) {
            std::cout << "Profile name cannot be empty" << std::endl;
            return;
        }
        
        ConnectionProfile profile;
        profile.name = name;
        profile.description = "Profile created via CLI";
        profile.basicSettings = configAPI->getBasicSettings();
        profile.advancedSettings = configAPI->getAdvancedSettings();
        profile.lastUsed = std::chrono::system_clock::now();
        
        if (profileManager->createProfile(profile)) {
            std::cout << "Profile '" << name << "' created successfully" << std::endl;
        } else {
            std::cout << "Failed to create profile '" << name << "'" << std::endl;
        }
    }
    
    void listProfiles() {
        if (!profilesEnabled) {
            std::cout << "Profile features are disabled. Could not create profile directories during initialization." << std::endl;
            return;
        }
        
        auto profiles = profileManager->getAllProfiles();
        std::string activeProfile = profileManager->getActiveProfile();
        
        std::cout << "Network Profiles:" << std::endl;
        std::cout << "=================" << std::endl;
        
        if (profiles.empty()) {
            std::cout << "No profiles found" << std::endl;
            return;
        }
        
        for (const auto& profile : profiles) {
            std::cout << profile.name;
            if (profile.name == activeProfile) {
                std::cout << " [ACTIVE]";
            }
            std::cout << std::endl;
            std::cout << "  Description: " << profile.description << std::endl;
            std::cout << "  Connection Mode: " << profile.basicSettings.connectionMode << std::endl;
            if (profile.basicSettings.connectionMode == "static") {
                std::cout << "  IP: " << profile.basicSettings.ipv4Address << std::endl;
                std::cout << "  Gateway: " << profile.basicSettings.defaultGateway << std::endl;
            }
            std::cout << "  MTU: " << profile.advancedSettings.mtuSize << std::endl;
            std::cout << std::endl;
        }
    }
    
    void activateProfile(const std::string& name) {
        if (!profilesEnabled) {
            std::cout << "Profile features are disabled. Could not create profile directories during initialization." << std::endl;
            return;
        }
        
        if (profileManager->activateProfile(name, *configAPI)) {
            std::cout << "Profile '" << name << "' activated successfully" << std::endl;
        } else {
            std::cout << "Failed to activate profile '" << name << "'" << std::endl;
        }
    }
    
    void deleteProfile(const std::string& name) {
        if (!profilesEnabled) {
            std::cout << "Profile features are disabled. Could not create profile directories during initialization." << std::endl;
            return;
        }
        if (profileManager->deleteProfile(name)) {
            std::cout << "Profile '" << name << "' deleted successfully" << std::endl;
        } else {
            std::cout << "Failed to delete profile '" << name << "'" << std::endl;
        }
    }
    
    void restoreDefaults() {
        if (configAPI->restoreDefaults()) {
            std::cout << "Default settings restored" << std::endl;
            std::cout << "Restarting interface to apply changes..." << std::endl;
            configAPI->restartInterface("eth0");
        } else {
            std::cout << "Failed to restore default settings" << std::endl;
        }
    }
    
    void restartInterface() {
        std::cout << "Restarting network interface..." << std::endl;
        if (configAPI->restartInterface("eth0")) {
            std::cout << "Interface restarted successfully" << std::endl;
        } else {
            std::cout << "Failed to restart interface" << std::endl;
        }
    }
    
    void listStaticRoutes() {
        auto routes = configAPI->getStaticRoutes();
        std::cout << "Static Routes:" << std::endl;
        std::cout << "================" << std::endl;
        for (const auto& route : routes) {
            std::cout << "Target: " << route.target << std::endl;
            std::cout << "Gateway: " << route.gateway << std::endl;
            std::cout << "Interface: " << route.interface << std::endl;
            std::cout << "Metric: " << route.metric << std::endl;
            std::cout << "Enabled: " << (route.enabled ? "Yes" : "No") << std::endl;
            std::cout << "----------------" << std::endl;
        }
    }
    
    void addStaticRoute(const std::string& target, const std::string& gateway, const std::string& interface, int metric) {
        StaticRoute route;
        route.target = target;
        route.gateway = gateway;
        route.interface = interface;
        route.metric = metric;
        route.enabled = true;
        
        if (configAPI->addStaticRoute(route)) {
            std::cout << "Static route added successfully" << std::endl;
        } else {
            std::cout << "Failed to add static route" << std::endl;
        }
    }
    
    void removeStaticRoute(const std::string& target) {
        if (configAPI->removeStaticRoute(target)) {
            std::cout << "Static route removed successfully" << std::endl;
        } else {
            std::cout << "Failed to remove static route" << std::endl;
        }
    }
    
    void enableStaticRoute(const std::string& target, bool enabled) {
        if (configAPI->enableStaticRoute(target, enabled)) {
            std::cout << "Static route " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        } else {
            std::cout << "Failed to " << (enabled ? "enable" : "disable") << " static route" << std::endl;
        }
    }
    
    void listVlans() {
        auto vlans = configAPI->getVlanConfigs();
        std::cout << "VLAN Configurations:" << std::endl;
        std::cout << "====================" << std::endl;
        for (const auto& vlan : vlans) {
            std::cout << "Name: " << vlan.name << std::endl;
            std::cout << "VLAN ID: " << vlan.vlanId << std::endl;
            std::cout << "Base Interface: " << vlan.baseInterface << std::endl;
            std::cout << "Protocol: " << vlan.protocol << std::endl;
            std::cout << "Enabled: " << (vlan.enabled ? "Yes" : "No") << std::endl;
            std::cout << "-------------------" << std::endl;
        }
    }
    
    void addVlan(const std::string& name, int vlanId, const std::string& baseInterface, const std::string& protocol) {
        VlanConfig vlan;
        vlan.name = name;
        vlan.vlanId = vlanId;
        vlan.baseInterface = baseInterface;
        vlan.protocol = protocol;
        vlan.enabled = true;
        
        if (configAPI->addVlanConfig(vlan)) {
            std::cout << "VLAN added successfully" << std::endl;
        } else {
            std::cout << "Failed to add VLAN" << std::endl;
        }
    }
    
    void removeVlan(const std::string& name) {
        if (configAPI->removeVlanConfig(name)) {
            std::cout << "VLAN removed successfully" << std::endl;
        } else {
            std::cout << "Failed to remove VLAN" << std::endl;
        }
    }
    
    void enableVlan(const std::string& name, bool enabled) {
        if (configAPI->enableVlanConfig(name, enabled)) {
            std::cout << "VLAN " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        } else {
            std::cout << "Failed to " << (enabled ? "enable" : "disable") << " VLAN" << std::endl;
        }
    }
    
    void listFirewallRules() {
        auto rules = configAPI->getFirewallRules();
        std::cout << "Firewall Rules:" << std::endl;
        std::cout << "===============" << std::endl;
        for (const auto& rule : rules) {
            std::cout << "Name: " << rule.name << std::endl;
            std::cout << "Source: " << rule.src << std::endl;
            std::cout << "Destination: " << rule.dest << std::endl;
            std::cout << "Target: " << rule.target << std::endl;
            std::cout << "Protocol: " << rule.proto << std::endl;
            std::cout << "Enabled: " << (rule.enabled ? "Yes" : "No") << std::endl;
            std::cout << "---------------" << std::endl;
        }
    }
    
    void addFirewallRule(const std::string& name, const std::string& src, const std::string& dest, const std::string& target, const std::string& proto) {
        FirewallRule rule;
        rule.name = name;
        rule.src = src;
        rule.dest = dest;
        rule.target = target;
        rule.proto = proto;
        rule.enabled = true;
        
        if (configAPI->addFirewallRule(rule)) {
            std::cout << "Firewall rule added successfully" << std::endl;
        } else {
            std::cout << "Failed to add firewall rule" << std::endl;
        }
    }
    
    void removeFirewallRule(const std::string& name) {
        if (configAPI->removeFirewallRule(name)) {
            std::cout << "Firewall rule removed successfully" << std::endl;
        } else {
            std::cout << "Failed to remove firewall rule" << std::endl;
        }
    }
    
    void enableFirewallRule(const std::string& name, bool enabled) {
        if (configAPI->enableFirewallRule(name, enabled)) {
            std::cout << "Firewall rule " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        } else {
            std::cout << "Failed to " << (enabled ? "enable" : "disable") << " firewall rule" << std::endl;
        }
    }
    
    void listBridges() {
        auto bridges = configAPI->getBridgeConfigs();
        std::cout << "Bridge Configurations:" << std::endl;
        std::cout << "======================" << std::endl;
        for (const auto& bridge : bridges) {
            std::cout << "Name: " << bridge.name << std::endl;
            std::cout << "Interfaces: ";
            for (size_t i = 0; i < bridge.interfaces.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << bridge.interfaces[i];
            }
            std::cout << std::endl;
            std::cout << "STP: " << bridge.stp << std::endl;
            std::cout << "Enabled: " << (bridge.enabled ? "Yes" : "No") << std::endl;
            std::cout << "-------------------" << std::endl;
        }
    }
    
    void addBridge(const std::string& name, const std::vector<std::string>& interfaces, const std::string& stp) {
        BridgeConfig bridge;
        bridge.name = name;
        bridge.interfaces = interfaces;
        bridge.stp = stp;
        bridge.forwardDelay = "15";
        bridge.maxAge = "20";
        bridge.helloTime = "2";
        bridge.enabled = true;
        
        if (configAPI->addBridgeConfig(bridge)) {
            std::cout << "Bridge added successfully" << std::endl;
        } else {
            std::cout << "Failed to add bridge" << std::endl;
        }
    }
    
    void removeBridge(const std::string& name) {
        if (configAPI->removeBridgeConfig(name)) {
            std::cout << "Bridge removed successfully" << std::endl;
        } else {
            std::cout << "Failed to remove bridge" << std::endl;
        }
    }
    
    void enableBridge(const std::string& name, bool enabled) {
        if (configAPI->enableBridgeConfig(name, enabled)) {
            std::cout << "Bridge " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        } else {
            std::cout << "Failed to " << (enabled ? "enable" : "disable") << " bridge" << std::endl;
        }
    }
    
    void listNatRules() {
        auto rules = configAPI->getNatRules();
        std::cout << "NAT Rules:" << std::endl;
        std::cout << "==========" << std::endl;
        for (const auto& rule : rules) {
            std::cout << "Name: " << rule.name << std::endl;
            std::cout << "Type: " << rule.type << std::endl;
            std::cout << "Source: " << rule.src << std::endl;
            std::cout << "Destination: " << rule.dest << std::endl;
            std::cout << "Target: " << rule.target << std::endl;
            std::cout << "Enabled: " << (rule.enabled ? "Yes" : "No") << std::endl;
            std::cout << "---------" << std::endl;
        }
    }
    
    void addNatRule(const std::string& name, const std::string& type, const std::string& src, const std::string& dest, const std::string& target) {
        NatRule rule;
        rule.name = name;
        rule.type = type;
        rule.src = src;
        rule.dest = dest;
        rule.target = target;
        rule.enabled = true;
        
        if (configAPI->addNatRule(rule)) {
            std::cout << "NAT rule added successfully" << std::endl;
        } else {
            std::cout << "Failed to add NAT rule" << std::endl;
        }
    }
    
    void removeNatRule(const std::string& name) {
        if (configAPI->removeNatRule(name)) {
            std::cout << "NAT rule removed successfully" << std::endl;
        } else {
            std::cout << "Failed to remove NAT rule" << std::endl;
        }
    }
    
    void enableNatRule(const std::string& name, bool enabled) {
        if (configAPI->enableNatRule(name, enabled)) {
            std::cout << "NAT rule " << (enabled ? "enabled" : "disabled") << " successfully" << std::endl;
        } else {
            std::cout << "Failed to " << (enabled ? "enable" : "disable") << " NAT rule" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    std::string packageConfigPath;
    std::string operationConfigPath;
    bool isTestMode = false;
    bool isVerboseMode = false;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"pkg_config", required_argument, 0, 1000},
        {"operation_config", required_argument, 0, 1001},
        {"test", no_argument, 0, 't'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "p:o:tvh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
            case 1000:
                packageConfigPath = std::string(optarg);
                break;
            case 'o':
            case 1001:
                operationConfigPath = std::string(optarg);
                break;
            case 't':
                isTestMode = true;
                break;
            case 'v':
                isVerboseMode = true;
                break;
            case 'h':
                std::cout << "OpenWrt Network Configuration CLI" << std::endl;
                std::cout << "Usage: openwrt-network-cli [options]" << std::endl;
                std::cout << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  -pkg_config <file>, --pkg_config <file>" << std::endl;
                std::cout << "                        Path to package configuration JSON file" << std::endl;
                std::cout << "  -operation_config <file>, --operation_config <file>" << std::endl;
                std::cout << "                        Path to operation configuration JSON file" << std::endl;
                std::cout << "  -t, --test             Enable test mode (no system commands)" << std::endl;
                std::cout << "  -v, --verbose          Enable verbose mode (show all commands and results)" << std::endl;
                std::cout << "  -h, --help          Show this help message" << std::endl;
                return 0;
            case '?':
                std::cerr << "Use -h or --help for usage information" << std::endl;
                return 1;
            default:
                break;
        }
    }
    
    if (!Utils::isRoot() && !isTestMode) {
        std::cerr << "This application requires root privileges" << std::endl;
        return 1;
    }
    
    // Set default package config path if not provided
    if (packageConfigPath.empty()) {
        packageConfigPath = "/etc/Ultima-Config/ur-base-network-mann/package-config.json";
    }
    
    CLIApp app(packageConfigPath, isTestMode, isVerboseMode);
    
    if (!app.initialize()) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    // Load and execute operation config if provided
    if (!operationConfigPath.empty()) {
        if (!app.getConfigAPI()->loadOperationConfig(operationConfigPath)) {
            std::cerr << "Failed to load operation configuration" << std::endl;
            return 1;
        }
        
        if (!app.getConfigAPI()->executeOperationConfig()) {
            std::cerr << "Failed to execute operation configuration" << std::endl;
            return 1;
        }
        
        return 0; // Exit after executing operations
    }
    
    app.run();
    
    return 0;
}
