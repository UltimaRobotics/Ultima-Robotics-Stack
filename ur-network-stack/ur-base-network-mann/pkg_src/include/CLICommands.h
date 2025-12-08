#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <mutex>
#include "NetworkConfigAPI.h"
#include "NetworkMonitor.h"
#include "ProfileManager.h"

namespace OpenWrtNetwork {

class CLICommands {
private:
    NetworkConfigAPI* configAPI;
    NetworkMonitor* networkMonitor;
    ProfileManager* profileManager;

public:
    CLICommands(NetworkConfigAPI* api, NetworkMonitor* monitor, ProfileManager* profileMgr);
    
    // Help and information
    void showHelp();
    void showVersion();
    
    // View operations
    void showStatus();
    void showInterfaceInfo();
    void startMonitoring();
    void stopMonitoring();
    
    // Configuration operations
    bool setConnectionMode(const std::string& mode);
    bool setStaticIP(const std::string& ip, const std::string& subnet, const std::string& gateway);
    bool setDnsServers(const std::vector<std::string>& dnsServers);
    bool setMtuSize(int mtu);
    
    // Profile management
    void listProfiles();
    bool createProfile(const std::string& name, const std::string& description = "");
    bool activateProfile(const std::string& name);
    bool deleteProfile(const std::string& name);
    
    // System operations
    bool restoreDefaults();
    
    // Static route management
    void listStaticRoutes();
    bool addStaticRoute(const std::string& target, const std::string& gateway, const std::string& interface, int metric = 0);
    bool removeStaticRoute(const std::string& target);
    bool enableStaticRoute(const std::string& target, bool enabled);
    
    // VLAN configuration
    void listVlans();
    bool addVlan(const std::string& name, int vlanId, const std::string& baseInterface, const std::string& protocol = "8021q");
    bool removeVlan(const std::string& name);
    bool enableVlan(const std::string& name, bool enabled);
    
    // Firewall rule management
    void listFirewallRules();
    bool addFirewallRule(const std::string& name, const std::string& src, const std::string& dest, const std::string& target, const std::string& proto = "all");
    bool removeFirewallRule(const std::string& name);
    bool enableFirewallRule(const std::string& name, bool enabled);
    
    // Bridge configuration
    void listBridges();
    bool addBridge(const std::string& name, const std::vector<std::string>& interfaces, const std::string& stp = "off");
    bool removeBridge(const std::string& name);
    bool enableBridge(const std::string& name, bool enabled);
    bool addInterfaceToBridge(const std::string& bridgeName, const std::string& interface);
    bool removeInterfaceFromBridge(const std::string& bridgeName, const std::string& interface);
    
    // NAT rule management
    void listNatRules();
    bool addNatRule(const std::string& name, const std::string& type, const std::string& src, const std::string& dest, const std::string& target = "");
    bool removeNatRule(const std::string& name);
    bool enableNatRule(const std::string& name, bool enabled);
};

} // namespace OpenWrtNetwork
