#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

namespace OpenWrtNetwork {

struct PackageConfig {
    std::string networkConfigPath;
    std::string firewallConfigPath;
    std::string staticRouteConfigPath;
    std::string resolvConfPath;
    std::string networkProfilesDir;
    std::string networkBackupsDir;
    std::string defaultInterface;
    std::string defaultConnectionMode;
    int defaultMtuSize;
};

struct NetworkStatus {
    bool connected;
    std::string connectionType; // "dhcp" or "static"
    std::string ipv4Address;
    std::string ipv6Address;
    std::string macAddress;
    std::string linkSpeed;
    std::chrono::seconds sessionDuration;
    std::string gatewayAddress;
    std::string externalIP;
    std::vector<std::string> dnsServers;
    double downloadSpeed; // Mbps
    double uploadSpeed; // Mbps
};

struct BasicSettings {
    std::string connectionMode; // "dhcp" or "manual"
    std::string ipv4Address;
    std::string subnetMask;
    std::string defaultGateway;
    int dhcpLeaseTimeRemaining;
    std::vector<std::string> dnsServers;
    std::vector<std::string> searchDomains;
    int interfacePriority;
};

struct AdvancedSettings {
    int mtuSize;
    bool checksumOffload;
    bool tcpSegmentationOffload;
    bool rssOffload;
    bool lsoOffload;
    std::string linkNegotiationMode; // "auto" or "forced"
    int forcedSpeed;
    std::string forcedDuplex; // "full" or "half"
    bool energyEfficientEthernet;
};

// New structures for advanced network management
struct StaticRoute {
    std::string target;        // e.g., "192.168.2.0/24"
    std::string gateway;       // e.g., "192.168.1.1"
    std::string interface;     // e.g., "lan"
    int metric;                // route metric
    bool enabled;
};

struct VlanConfig {
    std::string name;          // e.g., "lan.100"
    int vlanId;               // VLAN ID (1-4094)
    std::string baseInterface; // physical interface
    std::string protocol;      // "8021ad" or "8021q"
    bool enabled;
};

struct FirewallRule {
    std::string name;          // rule name
    std::string type;          // "rule", "redirect", "forwarding"
    std::string src;           // source zone
    std::string dest;          // destination zone
    std::string target;        // "ACCEPT", "DROP", "REJECT"
    std::string proto;         // "tcp", "udp", "icmp", "all"
    std::string srcPort;       // source port
    std::string destPort;      // destination port
    std::string srcIp;         // source IP
    std::string destIp;        // destination IP
    bool enabled;
};

struct BridgeConfig {
    std::string name;          // bridge name e.g., "br-lan"
    std::vector<std::string> interfaces; // member interfaces
    std::string stp;           // "on" or "off"
    std::string forwardDelay;  // STP forward delay
    std::string maxAge;        // STP max age
    std::string helloTime;     // STP hello time
    bool enabled;
};

struct NatRule {
    std::string name;          // rule name
    std::string type;          // "snat", "dnat", "masq"
    std::string src;           // source zone
    std::string dest;          // destination zone
    std::string target;        // target IP
    std::string proto;         // protocol
    std::string srcPort;       // source port
    std::string destPort;      // destination port
    std::string srcIp;         // source IP
    std::string destIp;        // destination IP
    bool enabled;
};

struct ConnectionProfile {
    std::string name;
    std::string description;
    BasicSettings basicSettings;
    AdvancedSettings advancedSettings;
    std::chrono::system_clock::time_point lastUsed;
    std::string gatewayInfo;
    std::vector<std::string> autoApplyConditions;
};

// Operation Configuration Structures
struct OperationConfig {
    // Network operations
    std::string connectionMode;
    std::string staticIp;
    std::string subnetMask;
    std::string defaultGateway;
    std::vector<std::string> dnsServers;
    int mtuSize;
    
    // Static routes
    std::vector<StaticRoute> staticRoutes;
    
    // VLAN configurations
    std::vector<VlanConfig> vlans;
    
    // Firewall rules
    std::vector<FirewallRule> firewallRules;
    
    // Bridge configurations
    std::vector<BridgeConfig> bridges;
    
    // NAT rules
    std::vector<NatRule> natRules;
    
    // Profile operations
    std::string profileName;
    std::string profileDescription;
    std::string activateProfile;
    std::string deleteProfile;
    
    // Interface operations
    bool restartInterface;
    bool restoreDefaults;
    
    // Flags for which operations to perform
    bool setConnectionModeOp;
    bool setStaticIpOp;
    bool setDnsOp;
    bool setMtuOp;
    bool addStaticRoutesOp;
    bool removeStaticRoutesOp;
    bool addVlansOp;
    bool removeVlansOp;
    bool addFirewallRulesOp;
    bool removeFirewallRulesOp;
    bool addBridgesOp;
    bool removeBridgesOp;
    bool addNatRulesOp;
    bool removeNatRulesOp;
    bool createProfileOp;
    bool activateProfileOp;
    bool deleteProfileOp;
    bool restartInterfaceOp;
    bool restoreDefaultsOp;
    
    // Data collection and listing operations
    bool showStatusOp;
    bool showInterfaceInfoOp;
    bool listProfilesOp;
    bool listStaticRoutesOp;
    bool listVlansOp;
    bool listFirewallRulesOp;
    bool listBridgesOp;
    bool listNatRulesOp;
    bool startMonitoringOp;
    bool stopMonitoringOp;
};

class NetworkConfigAPI {
public:
    NetworkConfigAPI();
    ~NetworkConfigAPI();

    bool initialize();
    bool loadNetworkConfig();
    bool saveNetworkConfig();

    // View Operations
    NetworkStatus getConnectionStatus();
    BasicSettings getBasicSettings();
    AdvancedSettings getAdvancedSettings();
    std::vector<ConnectionProfile> getProfiles();
    
    // Data Collection and Listing Operations
    void showStatus();
    void showInterfaceInfo();
    void listProfiles();
    void listStaticRoutes();
    void listVlans();
    void listFirewallRules();
    void listBridges();
    void listNatRules();
    void startMonitoring();
    void stopMonitoring();

    // Configuration Operations
    bool loadPackageConfig(const std::string& configPath);
    bool loadOperationConfig(const std::string& configPath);
    PackageConfig getPackageConfig() const;
    void setTestMode(bool enabled);
    bool setConnectionMode(const std::string& mode);
    bool setStaticIP(const std::string& ip, const std::string& subnet, const std::string& gateway);
    bool setDnsServers(const std::vector<std::string>& dns);
    bool setMtuSize(int mtu);
    bool setHardwareOffload(const std::string& feature, bool enabled);
    bool setLinkNegotiation(const std::string& mode, int speed = 0, const std::string& duplex = "full");
    bool setEnergyEfficientEthernet(bool enabled);
    
    // Execute operations from config
    bool executeOperationConfig();

    // System Operations
    bool enableInterface(const std::string& interface, bool enabled);
    bool restartInterface(const std::string& interface);
    bool restoreDefaults();

    // Static Route Management
    std::vector<StaticRoute> getStaticRoutes();
    bool addStaticRoute(const StaticRoute& route);
    bool removeStaticRoute(const std::string& target);
    bool updateStaticRoute(const std::string& target, const StaticRoute& route);
    bool enableStaticRoute(const std::string& target, bool enabled);

    // VLAN Configuration
    std::vector<VlanConfig> getVlanConfigs();
    bool addVlanConfig(const VlanConfig& vlan);
    bool removeVlanConfig(const std::string& name);
    bool updateVlanConfig(const std::string& name, const VlanConfig& vlan);
    bool enableVlanConfig(const std::string& name, bool enabled);

    // Firewall Rule Management
    std::vector<FirewallRule> getFirewallRules();
    bool addFirewallRule(const FirewallRule& rule);
    bool removeFirewallRule(const std::string& name);
    bool updateFirewallRule(const std::string& name, const FirewallRule& rule);
    bool enableFirewallRule(const std::string& name, bool enabled);

    // Bridge Configuration
    std::vector<BridgeConfig> getBridgeConfigs();
    bool addBridgeConfig(const BridgeConfig& bridge);
    bool removeBridgeConfig(const std::string& name);
    bool updateBridgeConfig(const std::string& name, const BridgeConfig& bridge);
    bool enableBridgeConfig(const std::string& name, bool enabled);
    bool addInterfaceToBridge(const std::string& bridgeName, const std::string& interface);
    bool removeInterfaceFromBridge(const std::string& bridgeName, const std::string& interface);

    // NAT Rule Management
    std::vector<NatRule> getNatRules();
    bool addNatRule(const NatRule& rule);
    bool removeNatRule(const std::string& name);
    bool updateNatRule(const std::string& name, const NatRule& rule);
    bool enableNatRule(const std::string& name, bool enabled);

    // Validation helper methods
    bool validateIpAddress(const std::string& ip);
    bool validateVlanId(int vlanId);
    bool validatePort(const std::string& port);

private:
    std::string configFilePath;
    std::map<std::string, std::string> configOptions; // Simplified: option -> value
    
    // Package configuration
    PackageConfig packageConfig;
    bool packageConfigLoaded = false;
    
    // Operation configuration
    OperationConfig operationConfig;
    bool operationConfigLoaded = false;
    
    // Test mode flag
    bool testMode = false;
    
    // Configuration files for different services
    std::string networkConfigPath;
    std::string firewallConfigPath;
    std::string staticRouteConfigPath;
    
    bool parseConfigFile();
    bool writeConfigFile();
    std::vector<std::string> parseListOption(const std::string& value);
    std::string formatListOption(const std::vector<std::string>& list);
    
    // Helper methods for new features
    bool parseNetworkConfig(const std::string& configPath, std::map<std::string, std::string>& config);
    bool writeNetworkConfig(const std::string& configPath, const std::map<std::string, std::string>& config);
    bool executeSystemCommand(const std::string& command);
    std::string generateConfigId(const std::string& prefix);
    
    // JSON parsing helper
    bool parseJsonConfig(const std::string& jsonContent, PackageConfig& config);
    bool parseOperationJsonConfig(const std::string& jsonContent, OperationConfig& config);
    
    // Helper methods for parsing operation arrays
    void parseStaticRoutesArray(const std::string& arrayContent, OperationConfig& config);
    void parseVlansArray(const std::string& arrayContent, OperationConfig& config);
    void parseFirewallRulesArray(const std::string& arrayContent, OperationConfig& config);
    void parseBridgesArray(const std::string& arrayContent, OperationConfig& config);
    void parseNatRulesArray(const std::string& arrayContent, OperationConfig& config);
};

} // namespace OpenWrtNetwork
