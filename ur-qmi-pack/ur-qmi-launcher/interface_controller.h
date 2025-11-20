#ifndef INTERFACE_CONTROLLER_H
#define INTERFACE_CONTROLLER_H

#include <string>
#include "smart_routing.h"
#include <vector>
#include <memory>

struct InterfaceConfig {
    std::string interface_name;
    std::string ip_address;
    std::string subnet_mask;
    std::string gateway;
    std::string dns_primary;
    std::string dns_secondary;
    uint32_t mtu;
    bool use_dhcp;
};

struct InterfaceStatus {
    std::string name;
    bool is_up;
    bool has_ip;
    std::string ip_address;
    std::string mac_address;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t mtu;
};

class InterfaceController {
public:
    InterfaceController();
    ~InterfaceController();

    // Interface management
    bool configureInterface(const InterfaceConfig& config);
    bool bringInterfaceUp(const std::string& interface_name);
    bool bringInterfaceDown(const std::string& interface_name);
    bool resetInterface(const std::string& interface_name);

    // IP configuration
    bool setStaticIP(const std::string& interface_name, 
                    const std::string& ip_address,
                    const std::string& subnet_mask,
                    const std::string& gateway);
    bool startDHCP(const std::string& interface_name);
    bool stopDHCP(const std::string& interface_name);
    bool renewDHCP(const std::string& interface_name);

    // DNS configuration
    bool setDNS(const std::string& primary, const std::string& secondary = "");
    bool restoreDNS();

    // Route management
    bool addDefaultRoute(const std::string& gateway, const std::string& interface_name);
    bool removeDefaultRoute(const std::string& gateway);
    bool addRoute(const std::string& destination, const std::string& gateway, 
                 const std::string& interface_name);
    bool removeRoute(const std::string& destination, const std::string& gateway);

    // Interface discovery
    std::vector<std::string> findWWANInterfaces();
    std::vector<std::string> getExistingWWANInterfaces();
    std::string findInterfaceForDevice(const std::string& device_path);
    InterfaceStatus getInterfaceStatus(const std::string& interface_name);
    std::vector<InterfaceStatus> getAllInterfaces();

    // Interface name management
    std::string generateUniqueWWANName(const std::string& base_name = "wwan");
    bool isInterfaceNameTaken(const std::string& interface_name);
    std::vector<int> getUsedWWANNumbers();

    // Smart cleanup and interface availability methods
    bool isInterfaceActive(const std::string& interface_name);
    bool isInterfaceConnected(const std::string& interface_name);
    bool hasValidIPAddress(const std::string& interface_name);
    bool isInterfaceRunning(const std::string& interface_name);
    std::string findFirstAvailableInterface(const std::string& base_name = "wwan");
    bool performSmartCleanup();
    bool cleanupInactiveInterfaces();
    std::vector<std::string> getInactiveWWANInterfaces();

    // Cleanup verification and force cleanup methods
    bool verifyInterfaceCleanup(const std::string& interface_name);
    int countActiveWWANInterfaces();
    bool forceCleanupInterface(const std::string& interface_name);

    // Production-ready interface creation
    bool ensureInterfaceExists(const std::string& interface_name, const std::string& device_path);
    bool createWWANInterface(const std::string& interface_name, const std::string& device_path);
    bool waitForInterfaceCreation(const std::string& interface_name, int timeout_seconds = 10);
    bool bindInterfaceToDevice(const std::string& interface_name, const std::string& device_path);

    // Validation
    bool validateConfiguration(const InterfaceConfig& config);
    bool testConnectivity(const std::string& target = "8.8.8.8");
    bool pingGateway(const std::string& gateway);

    // Smart routing integration
    bool applyCellularRouting(const std::string& interface_name, 
                             const std::string& gateway_ip,
                             const std::string& local_ip);
    bool removeCellularRouting(const std::string& interface_name);

    // QMI Raw IP configuration
    bool verifyAndSetRawIP(const std::string& interface_name);
    bool getRawIPStatus(const std::string& interface_name);
    bool setRawIPMode(const std::string& interface_name, bool enable);
    bool enforceRawIPRequirement(const std::string& interface_name);

    // Hot-disconnect cleanup support
    std::vector<std::string> getActiveInterfaces();
    std::vector<std::string> getActiveRoutes();
    bool cleanupInterface(const std::string& interface_name);
    bool cleanupAllRoutes();

public:
    // Emergency cleanup support methods (moved from private)
    std::string parseInterfaceIP(const std::string& interface_name);
    bool executeCommandSuccess(const std::string& command);

private:
    std::string executeCommand(const std::string& command);
    std::string parseInterfaceMAC(const std::string& interface_name);
    bool parseInterfaceStats(const std::string& interface_name, 
                            uint64_t& bytes_sent, uint64_t& bytes_received);

    std::string m_current_interface;
    std::string m_backup_dns_config;
    bool m_dns_modified;
};

#endif // INTERFACE_CONTROLLER_H