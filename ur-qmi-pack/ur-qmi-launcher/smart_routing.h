#ifndef SMART_ROUTING_H
#define SMART_ROUTING_H

#include <string>
#include <vector>
#include <map>
#include <functional>

/**
 * @brief Enumeration of routing operation types
 */
enum class RoutingOperation {
    ADD_DEFAULT_ROUTE,
    REMOVE_DEFAULT_ROUTE,
    ADD_PRIORITY_ROUTE,
    REMOVE_PRIORITY_ROUTE,
    ADD_INTERFACE_ROUTE,
    REMOVE_INTERFACE_ROUTE,
    ADD_HOST_ROUTE,
    REMOVE_HOST_ROUTE,
    SET_INTERFACE_METRIC,
    FLUSH_ROUTES,
    BACKUP_ROUTES,
    RESTORE_ROUTES
};

/**
 * @brief Structure to define a routing rule
 */
struct RoutingRule {
    std::string destination = "0.0.0.0/0";     // Network destination (default: all traffic)
    std::string gateway;                        // Gateway IP address
    std::string interface;                      // Network interface name
    int metric = 100;                          // Route priority (lower = higher priority)
    int table = 0;                             // Routing table ID (0 = main table)
    std::string source;                        // Source IP address (optional)
    bool persistent = false;                   // Whether route survives reboots
    std::string description;                   // Human-readable description
};

/**
 * @brief Configuration for smart routing system
 */
struct SmartRoutingConfig {
    // Routing mode
    bool auto_routing_enabled = true;          // Enable automatic routing management
    bool manual_routing_enabled = false;      // Enable manual routing rules
    bool backup_existing_routes = true;       // Backup existing routes before changes
    
    // Cellular interface configuration
    std::string cellular_interface;           // Cellular interface name (e.g., wwan0)
    int cellular_default_metric = 100;        // Default metric for cellular routes
    int cellular_priority_level = 2;          // Priority level (1=highest, higher numbers=lower priority)
    bool set_cellular_as_default = true;      // Set cellular as default internet route
    bool coexist_with_other_interfaces = true; // Allow coexistence with other interfaces
    
    // Interface priorities (interface -> priority level)
    std::map<std::string, int> interface_priorities;
    
    // Manual routing rules
    std::vector<RoutingRule> manual_rules;
    
    // Specific routing policies
    bool preserve_local_routes = true;        // Preserve local network routes
    bool preserve_vpn_routes = true;          // Preserve VPN routes
    std::vector<std::string> protected_interfaces; // Interfaces to never modify
    std::vector<std::string> priority_destinations; // High-priority destinations
    
    // Failover configuration
    bool enable_failover = true;              // Enable automatic failover
    std::string primary_interface;            // Primary interface for failover
    std::string backup_interface;             // Backup interface for failover
    int failover_timeout_ms = 30000;          // Failover detection timeout
    
    // Load configuration from JSON file
    bool loadFromFile(const std::string& config_file);
    
    // Save configuration to JSON file
    bool saveToFile(const std::string& config_file) const;
    
    // Validate configuration
    bool validate() const;
    
    // Print configuration
    void printConfiguration() const;
};

/**
 * @brief Callback function type for routing change notifications
 */
using RoutingChangeCallback = std::function<void(RoutingOperation operation, const RoutingRule& rule, bool success, const std::string& error)>;

/**
 * @brief Smart routing manager class
 */
class SmartRoutingManager {
public:
    /**
     * @brief Constructor
     */
    SmartRoutingManager();
    
    /**
     * @brief Destructor
     */
    ~SmartRoutingManager();
    
    /**
     * @brief Initialize the routing manager
     * @param config Routing configuration
     * @return true if initialization successful
     */
    bool initialize(const SmartRoutingConfig& config);
    
    /**
     * @brief Set routing change callback
     * @param callback Callback function for routing changes
     */
    void setRoutingChangeCallback(const RoutingChangeCallback& callback);
    
    /**
     * @brief Apply cellular interface routing
     * @param interface_name Cellular interface name
     * @param gateway_ip Gateway IP address
     * @param local_ip Local IP address
     * @return true if routing applied successfully
     */
    bool applyCellularRouting(const std::string& interface_name, 
                             const std::string& gateway_ip, 
                             const std::string& local_ip);
    
    /**
     * @brief Remove cellular interface routing
     * @param interface_name Cellular interface name
     * @return true if routing removed successfully
     */
    bool removeCellularRouting(const std::string& interface_name);
    
    /**
     * @brief Set interface priority
     * @param interface_name Interface name
     * @param priority Priority level (1 = highest)
     * @return true if priority set successfully
     */
    bool setInterfacePriority(const std::string& interface_name, int priority);
    
    /**
     * @brief Add custom routing rule
     * @param rule Routing rule to add
     * @return true if rule added successfully
     */
    bool addRoutingRule(const RoutingRule& rule);
    
    /**
     * @brief Remove routing rule
     * @param rule Routing rule to remove
     * @return true if rule removed successfully
     */
    bool removeRoutingRule(const RoutingRule& rule);
    
    /**
     * @brief Get current routing table
     * @return Vector of current routing rules
     */
    std::vector<RoutingRule> getCurrentRoutes();
    
    /**
     * @brief Backup current routing table
     * @return true if backup successful
     */
    bool backupRoutes();
    
    /**
     * @brief Restore routing table from backup
     * @return true if restore successful
     */
    bool restoreRoutes();
    
    /**
     * @brief Perform failover to backup interface
     * @return true if failover successful
     */
    bool performFailover();
    
    /**
     * @brief Check interface connectivity
     * @param interface_name Interface to check
     * @return true if interface is connected and functional
     */
    bool checkInterfaceConnectivity(const std::string& interface_name);
    
    /**
     * @brief Monitor routing health (should be called periodically)
     */
    void monitorRoutingHealth();
    
    /**
     * @brief Get routing status and statistics
     * @return String with routing status information
     */
    std::string getRoutingStatus() const;
    
    /**
     * @brief Enable/disable automatic routing
     * @param enabled Whether automatic routing is enabled
     */
    void setAutoRoutingEnabled(bool enabled);
    
    /**
     * @brief Get current configuration
     * @return Current routing configuration
     */
    const SmartRoutingConfig& getConfiguration() const;

private:
    SmartRoutingConfig m_config;
    RoutingChangeCallback m_routing_callback;
    std::vector<RoutingRule> m_backup_routes;
    bool m_initialized;
    
    // Internal helper methods
    bool executeRoutingCommand(const std::string& command);
    std::string buildRouteCommand(RoutingOperation operation, const RoutingRule& rule);
    bool parseRouteOutput(const std::string& output, std::vector<RoutingRule>& routes);
    int calculateMetric(const std::string& interface_name, int base_priority);
    bool isProtectedInterface(const std::string& interface_name) const;
    bool isLocalRoute(const RoutingRule& rule) const;
    bool isVPNRoute(const RoutingRule& rule) const;
    std::string getInterfaceGateway(const std::string& interface_name);
    bool validateRoutingRule(const RoutingRule& rule) const;
    void notifyRoutingChange(RoutingOperation operation, const RoutingRule& rule, bool success, const std::string& error = "");
};

/**
 * @brief Global smart routing manager instance
 */
extern SmartRoutingManager g_smart_routing;

#endif // SMART_ROUTING_H