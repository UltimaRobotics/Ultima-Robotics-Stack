# VPN Routing Collection and Control Methods Guide

## Overview

This comprehensive guide covers the routing collection and control methods available in both the `ur-wg_library` (WireGuard) and `ur-openvpn-library` (OpenVPN) wrapper libraries within the ur-vpn-extended stack. These libraries provide advanced routing capabilities including automatic route detection, custom rule management, real-time monitoring, and policy enforcement.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [ur-vpn-manager Integration](#ur-vpn-manager-integration)
3. [WireGuard Library (ur-wg_library)](#wireguard-library-ur-wg_library)
4. [OpenVPN Library (ur-openvpn-library)](#openvpn-library-ur-openvpn-library)
5. [Unified Routing Interface](#unified-routing-interface)
6. [Advanced Features](#advanced-features)
7. [Best Practices](#best-practices)
8. [Troubleshooting](#troubleshooting)

---

## Architecture Overview

### ur-vpn-manager Binary Routing Workflow

The `ur-vpn-manager` binary serves as the central orchestrator for VPN routing operations:

#### Key Components:
- **VPNInstanceManager**: Core management class handling multiple VPN instances
- **Routing Rules Engine**: Processes and applies routing rules
- **Detection System**: Automatically discovers routes from active VPN connections
- **Provider Abstraction**: Unified interface for both WireGuard and OpenVPN providers

#### Routing Workflow:
1. **Initialization**: Load configuration and routing rules from JSON files
2. **Instance Management**: Start/stop VPN instances with routing context
3. **Route Detection**: Automatically detect routes when VPN connects
4. **Rule Application**: Apply custom routing rules based on policies
5. **Monitoring**: Real-time route monitoring and enforcement
6. **Cleanup**: Remove routes and restore system state on disconnect

### Detection and Collection Process

The system uses multiple detection methods:

#### For WireGuard:
- Interface-specific route scanning (`wg*`, `wiga*`)
- Private network range detection (RFC1918 compliance)
- Netlink-based real-time monitoring
- Polling fallback for compatibility

#### For OpenVPN:
- Tunnel interface detection (`tun*`, `tap*`)
- Split tunnel route identification (0.0.0.0/1, 128.0.0.0/1)
- VPN server gateway detection
- Route pattern matching for fallback scenarios

---

## ur-vpn-manager Integration

### Configuration Structure

```json
{
  "config_file_path": "config/vpn-config.json",
  "custom_routing_rules": "config/routing-rules.json",
  "cached_data_path": "config/cache.json",
  "cleanup_config_path": "config/cleanup-config.json",
  "verbose": true,
  "stats_logging": {
    "enabled": true,
    "openvpn": true,
    "wireguard": true
  }
}
```

### Routing Rules File Format

```json
{
  "routing_rules": [
    {
      "id": "custom_rule_001",
      "name": "Tunnel Office Traffic",
      "vpn_instance": "office_vpn",
      "vpn_profile": "office_profile",
      "source_type": "Any",
      "source_value": "",
      "destination": "192.168.100.0/24",
      "gateway": "VPN Server",
      "protocol": "both",
      "type": "tunnel_specific",
      "priority": 100,
      "enabled": true,
      "log_traffic": false,
      "apply_to_existing": false,
      "description": "Route office network through VPN",
      "created_date": "1704067200",
      "last_modified": "1704067200",
      "is_automatic": false,
      "user_modified": true
    }
  ]
}
```

### Core Manager Methods

#### Route Detection and Management
```cpp
// Detect and save automatic routes for a specific instance
void detectAndSaveAutomaticRoutes(const std::string& instance_name, 
                                 const std::string& interface_name);

// Parse route output from system commands
std::vector<RoutingRule> parseRouteOutput(const std::string& route_output, 
                                         const std::string& instance_name);

// Merge automatic routes with existing rules
void mergeAutomaticRoutes(const std::vector<RoutingRule>& detected_rules, 
                         const std::string& instance_name);
```

#### Rule CRUD Operations
```cpp
bool addRoutingRule(const RoutingRule& rule);
bool updateRoutingRule(const std::string& rule_id, const RoutingRule& rule);
bool deleteRoutingRule(const std::string& rule_id);
json getRoutingRule(const std::string& rule_id);
json getAllRoutingRules();
```

#### Route Application
```cpp
void applyRoutingRulesForInstance(const std::string& instance_name);
void removeRoutingRulesForInstance(const std::string& instance_name);
```

---

## WireGuard Library (ur-wg_library)

### Core Routing Structures

#### Basic Route Rule
```c
typedef struct {
    char id[64];
    char name[256];
    wg_route_type_t type;
    bool is_automatic;
    bool user_modified;
    
    wg_route_src_type_t src_type;
    wg_ip_addr_t src_addr;
    uint8_t src_prefix_len;
    char src_interface[32];
    
    wg_ip_addr_t dest_addr;
    uint8_t dest_prefix_len;
    bool is_ipv6;
    
    wg_ip_addr_t gateway;
    bool has_gateway;
    uint32_t metric;
    uint32_t table_id;
    
    wg_route_protocol_t protocol;
    uint16_t port_start;
    uint16_t port_end;
    
    wg_route_state_t state;
    bool enabled;
    bool log_traffic;
    
    char description[MAX_ROUTE_DESCRIPTION];
    time_t created_time;
    time_t modified_time;
    time_t applied_time;
    
    uint64_t packets_routed;
    uint64_t bytes_routed;
    time_t last_used;
} wg_route_rule_t;
```

#### Enhanced Route Rule (Advanced Features)
```c
typedef struct {
    // Basic identification
    char id[64];
    char name[128];
    char description[512];
    
    // Rule metadata
    uint32_t priority;
    bool enabled;
    bool is_persistent;
    bool is_system_rule;
    time_t created_time;
    time_t modified_time;
    time_t last_hit_time;
    uint64_t hit_count;
    
    // Base rule compatibility
    wg_route_rule_t base_rule;
    
    // Enhanced conditions and actions
    wg_rule_condition_t conditions[MAX_RULE_CONDITIONS];
    size_t condition_count;
    wg_rule_action_t action;
    
    // Statistics and monitoring
    struct {
        uint64_t packets_matched;
        uint64_t bytes_matched;
        uint64_t packets_blocked;
        uint64_t bytes_blocked;
        uint64_t packets_redirected;
        uint64_t bytes_redirected;
        uint64_t avg_processing_time_ns;
        uint64_t max_processing_time_ns;
        uint64_t total_evaluations;
        uint64_t violation_count;
        time_t last_violation;
        char last_violation_details[256];
    } stats;
    
    // Persistence and versioning
    uint32_t version;
    char creator[64];
    char signature[128];
} enhanced_wg_route_rule_t;
```

### C++ Wrapper Interface

#### Basic Route Management
```cpp
class WireGuardWrapper {
public:
    struct RouteRule {
        std::string id;
        std::string name;
        std::string type;
        std::string destination;
        std::string gateway;
        std::string source_type;
        std::string source_value;
        std::string protocol;
        uint32_t metric;
        bool enabled;
        bool is_automatic;
        std::string description;
        
        json to_json() const;
        static RouteRule from_json(const json& j);
    };
    
    // Basic routing operations
    bool addRouteRule(const RouteRule& rule);
    bool removeRouteRule(const std::string& rule_id);
    std::vector<RouteRule> getRouteRules() const;
    RouteRule getRouteRule(const std::string& rule_id) const;
    bool applyPreConnectionRoutes();
    bool detectPostConnectionRoutes();
    void setRouteEventCallback(RouteEventCallback callback);
};
```

#### Enhanced Routing System
```cpp
class WireGuardWrapper {
public:
    // Enhanced route rule with conditions and actions
    struct EnhancedRouteRule {
        std::string id;
        std::string name;
        std::string description;
        uint32_t priority;
        bool enabled;
        bool is_persistent;
        bool is_system_rule;
        
        RouteRule base_rule;
        std::vector<RuleCondition> conditions;
        RuleAction action;
        
        // Statistics
        struct {
            uint64_t packets_matched = 0;
            uint64_t bytes_matched = 0;
            uint64_t packets_blocked = 0;
            uint64_t bytes_blocked = 0;
            uint64_t packets_redirected = 0;
            uint64_t bytes_redirected = 0;
            uint64_t avg_processing_time_ns = 0;
            uint64_t max_processing_time_ns = 0;
            uint64_t total_evaluations = 0;
            uint64_t violation_count = 0;
            time_t last_violation = 0;
            std::string last_violation_details;
        } statistics;
        
        // Metadata
        uint32_t version;
        std::string creator;
        std::string signature;
        time_t created_time;
        time_t modified_time;
        time_t last_hit_time;
        uint64_t hit_count;
        
        json to_json() const;
        static EnhancedRouteRule from_json(const json& j);
    };
    
    // Enhanced routing operations
    bool initializeEnhancedRouting(bool enable_detection = true, 
                                   bool enable_enforcement = true);
    void cleanupEnhancedRouting();
    
    bool addEnhancedRouteRule(const EnhancedRouteRule& rule);
    bool removeEnhancedRouteRule(const std::string& rule_id);
    bool updateEnhancedRouteRule(const std::string& rule_id, 
                                const EnhancedRouteRule& updated_rule);
    EnhancedRouteRule getEnhancedRouteRule(const std::string& rule_id) const;
    std::vector<EnhancedRouteRule> getEnhancedRouteRules() const;
    
    // Detection and enforcement control
    bool startRouteDetection(int interval_ms = 1000, bool netlink_monitoring = true);
    bool stopRouteDetection();
    bool startRouteEnforcement(EnforcementMode mode = EnforcementMode::ACTIVE, 
                              bool auto_correction = true);
    bool stopRouteEnforcement();
    bool setPolicyMode(bool strict_mode = false, bool learning_mode = false);
    
    // Advanced monitoring and statistics
    EnhancedRoutingStats getEnhancedRoutingStats() const;
    json exportEnhancedRules() const;
    bool importEnhancedRules(const json& rules_json);
    
    // Policy management
    bool applyPolicyFile(const std::string& policy_file);
    bool validatePolicy(const std::string& policy_json, std::string& validation_result) const;
    std::string exportPolicy() const;
    
    // Route evaluation and enforcement
    bool shouldAllowRoute(const EnhancedRouteRule& route) const;
    bool enforceRoutePolicy(const EnhancedRouteRule& route);
    
    // Callbacks
    void setViolationCallback(ViolationCallback callback);
};
```

### WireGuard Route Conditions

#### Condition Types
```cpp
enum class ConditionType {
    ALWAYS = WG_CONDITION_ALWAYS,
    TIME_BASED = WG_CONDITION_TIME_BASED,
    SOURCE_IP = WG_CONDITION_SOURCE_IP,
    DESTINATION_IP = WG_CONDITION_DESTINATION_IP,
    PROTOCOL = WG_CONDITION_PROTOCOL,
    PORT_RANGE = WG_CONDITION_PORT_RANGE,
    APPLICATION = WG_CONDITION_APPLICATION,
    USER_ID = WG_CONDITION_USER_ID,
    INTERFACE_STATE = WG_CONDITION_INTERFACE_STATE,
    BANDWIDTH_LIMIT = WG_CONDITION_BANDWIDTH_LIMIT
};
```

#### Condition Examples
```cpp
// Time-based condition
RuleCondition timeCondition;
timeCondition.type = ConditionType::TIME_BASED;
timeCondition.negated = false;
timeCondition.data = json{
    {"start_hour", 9},
    {"start_minute", 0},
    {"end_hour", 17},
    {"end_minute", 0},
    {"days_of_week", {1, 2, 3, 4, 5}}  // Monday-Friday
};

// Source IP condition
RuleCondition sourceCondition;
sourceCondition.type = ConditionType::SOURCE_IP;
sourceCondition.negated = false;
sourceCondition.data = json{
    {"start_ip", "192.168.1.1"},
    {"end_ip", "192.168.1.254"},
    {"prefix_len", 24},
    {"is_ipv6", false}
};

// Bandwidth limit condition
RuleCondition bandwidthCondition;
bandwidthCondition.type = ConditionType::BANDWIDTH_LIMIT;
bandwidthCondition.negated = false;
bandwidthCondition.data = json{
    {"threshold_bps", 1048576000},  // 1 Gbps
    {"time_window_seconds", 60}
};
```

### WireGuard Route Actions

#### Action Types
```cpp
enum class ActionType {
    ALLOW = WG_ACTION_ALLOW,
    BLOCK = WG_ACTION_BLOCK,
    REDIRECT = WG_ACTION_REDIRECT,
    MODIFY_METRIC = WG_ACTION_MODIFY_METRIC,
    FORCE_TUNNEL = WG_ACTION_FORCE_TUNNEL,
    FORCE_IGNORE = WG_ACTION_FORCE_IGNORE,
    LOG_ONLY = WG_ACTION_LOG_ONLY,
    RATE_LIMIT = WG_ACTION_RATE_LIMIT,
    SHAPE_TRAFFIC = WG_ACTION_SHAPE_TRAFFIC
};
```

#### Action Examples
```cpp
// Redirect action
RuleAction redirectAction;
redirectAction.type = ActionType::REDIRECT;
redirectAction.temporary = false;
redirectAction.log_message = "Redirecting traffic to alternative gateway";
redirectAction.data = json{
    {"gateway", "10.0.0.254"},
    {"metric", 200},
    {"interface", "wg-alt"}
};

// Rate limit action
RuleAction rateLimitAction;
rateLimitAction.type = ActionType::RATE_LIMIT;
rateLimitAction.temporary = true;
rateLimitAction.duration_seconds = 300;  // 5 minutes
rateLimitAction.log_message = "Applying rate limit due to policy violation";
rateLimitAction.data = json{
    {"rate_bps", 10485760},  // 10 Mbps
    {"burst_size", 1048576}  // 1 MB burst
};
```

### C API Functions

#### Basic Routing API
```c
// Context management
wg_routing_ctx_t* wg_routing_init(const char *interface_name);
void wg_routing_cleanup(wg_routing_ctx_t *ctx);

// Callback setup
void wg_routing_set_callback(wg_routing_ctx_t *ctx, 
                             wg_route_event_callback_t callback,
                             void *user_data);

// Rule management
int wg_routing_add_rule(wg_routing_ctx_t *ctx, const wg_route_rule_t *rule);
int wg_routing_remove_rule(wg_routing_ctx_t *ctx, const char *rule_id);
int wg_routing_update_rule(wg_routing_ctx_t *ctx, 
                           const char *rule_id,
                           const wg_route_rule_t *updated_rule);

// Rule retrieval
int wg_routing_get_rule(wg_routing_ctx_t *ctx, 
                        const char *rule_id,
                        wg_route_rule_t *out_rule);
int wg_routing_get_all_rules(wg_routing_ctx_t *ctx,
                             wg_route_rule_t **out_rules,
                             size_t *out_count);

// Route operations
int wg_routing_apply_rules(wg_routing_ctx_t *ctx);
int wg_routing_clear_routes(wg_routing_ctx_t *ctx);
int wg_routing_detect_routes(wg_routing_ctx_t *ctx);

// Monitoring
int wg_routing_start_monitoring(wg_routing_ctx_t *ctx, int interval_ms);
void wg_routing_stop_monitoring(wg_routing_ctx_t *ctx);

// Import/Export
char* wg_routing_export_json(wg_routing_ctx_t *ctx);
int wg_routing_import_json(wg_routing_ctx_t *ctx, const char *json_str);

// Statistics
int wg_routing_get_rule_stats(wg_routing_ctx_t *ctx,
                              const char *rule_id,
                              uint64_t *packets,
                              uint64_t *bytes);
```

#### Enhanced Routing API
```c
// Enhanced context initialization
enhanced_wg_routing_ctx_t* enhanced_wg_routing_init(
    const char *interface_name,
    const enhanced_wg_routing_config_t *config);

// Enhanced rule management
int enhanced_wg_routing_add_enhanced_rule(
    enhanced_wg_routing_ctx_t *ctx,
    const enhanced_wg_route_rule_t *rule);
int enhanced_wg_routing_remove_enhanced_rule(
    enhanced_wg_routing_ctx_t *ctx,
    const char *rule_id);
int enhanced_wg_routing_update_enhanced_rule(
    enhanced_wg_routing_ctx_t *ctx,
    const char *rule_id,
    const enhanced_wg_route_rule_t *updated_rule);

// Detection and enforcement control
int enhanced_wg_routing_start_detection(
    enhanced_wg_routing_ctx_t *ctx,
    bool netlink_monitoring,
    int interval_ms);
int enhanced_wg_routing_start_enforcement(
    enhanced_wg_routing_ctx_t *ctx,
    wg_enforcement_mode_t mode,
    bool auto_correction);

// Policy management
int enhanced_wg_routing_apply_policy(
    enhanced_wg_routing_ctx_t *ctx,
    const char *policy_json);
char* enhanced_wg_routing_export_policy(
    enhanced_wg_routing_ctx_t *ctx);
```

---

## OpenVPN Library (ur-openvpn-library)

### Core Routing Structures

#### Basic Route Rule
```c
typedef struct {
    char id[64];
    char name[256];
    
    ovpn_route_type_t type;
    bool is_automatic;
    bool user_modified;
    
    ovpn_route_src_type_t src_type;
    ovpn_ip_addr_t src_addr;
    uint8_t src_prefix_len;
    char src_interface[32];
    
    ovpn_ip_addr_t dest_addr;
    uint8_t dest_prefix_len;
    bool is_ipv6;
    
    ovpn_ip_addr_t gateway;
    bool has_gateway;
    uint32_t metric;
    uint32_t table_id;
    
    ovpn_route_protocol_t protocol;
    uint16_t port_start;
    uint16_t port_end;
    
    ovpn_route_state_t state;
    bool enabled;
    bool log_traffic;
    
    char description[OVPN_MAX_ROUTE_DESC];
    time_t created_time;
    time_t modified_time;
    time_t applied_time;
    
    uint64_t packets_routed;
    uint64_t bytes_routed;
    time_t last_used;
} ovpn_route_rule_t;
```

### C++ Wrapper Interface

#### Basic Route Management
```cpp
class OpenVPNWrapper {
public:
    struct RouteRule {
        std::string id;
        std::string name;
        std::string type;
        std::string destination;
        std::string gateway;
        std::string source_type;
        std::string source_value;
        std::string protocol;
        uint32_t metric;
        bool enabled;
        bool is_automatic;
        std::string description;
        
        json to_json() const;
        static RouteRule from_json(const json& j);
    };
    
    // Basic routing operations
    bool addRouteRule(const RouteRule& rule);
    bool removeRouteRule(const std::string& rule_id);
    std::vector<RouteRule> getRouteRules() const;
    RouteRule getRouteRule(const std::string& rule_id) const;
    bool applyPreConnectionRoutes();
    bool detectPostConnectionRoutes();
    void setRouteEventCallback(RouteEventCallback callback);
    
    // Route control system methods
    bool setRouteControlMode(bool preventDefaultRoutes, bool selectiveRouting);
    bool setPreventDefaultRoutes(bool prevent);
    bool setSelectiveRouting(bool selective);
    bool addCustomRouteRule(const RouteRule& rule);
    std::string getRouteStatistics() const;
};
```

#### Enhanced Routing System
```cpp
class OpenVPNWrapper {
public:
    // Enhanced route rule with conditions and actions
    struct EnhancedRouteRule {
        std::string id;
        std::string name;
        std::string description;
        std::string destination;
        std::string gateway;
        uint32_t priority = 0;
        bool enabled = true;
        bool is_persistent = false;
        
        // Rule conditions
        struct Condition {
            enum Type { 
                ALWAYS = 0, 
                TIME_BASED = 1, 
                SOURCE_IP = 2, 
                DESTINATION_IP = 3, 
                APPLICATION = 4, 
                INTERFACE = 5 
            };
            Type type;
            bool negated = false;
            std::string value;
            
            json to_json() const;
            static Condition from_json(const json& j);
        };
        std::vector<Condition> conditions;
        
        // Rule actions
        enum ActionType { 
            ALLOW = 0, 
            BLOCK = 1, 
            REDIRECT = 2, 
            MODIFY_METRIC = 3, 
            FORCE_TUNNEL = 4, 
            FORCE_IGNORE = 5 
        };
        struct Action {
            ActionType type = ALLOW;
            bool temporary = false;
            uint32_t duration_seconds = 0;
            std::string log_message;
            std::string action_value;
            
            json to_json() const;
            static Action from_json(const json& j);
        };
        Action action;
        
        // Statistics and metadata
        time_t created_time;
        time_t modified_time;
        time_t last_hit_time;
        uint64_t hit_count;
        
        json to_json() const;
        static EnhancedRouteRule from_json(const json& j);
    };
    
    // Enhanced routing operations
    bool initializeEnhancedRouting(bool enable_detection = true, 
                                   bool enable_enforcement = true);
    void cleanupEnhancedRouting();
    
    bool addEnhancedRouteRule(const EnhancedRouteRule& rule);
    bool removeEnhancedRouteRule(const std::string& rule_id);
    bool updateEnhancedRouteRule(const std::string& rule_id, 
                                const EnhancedRouteRule& updated_rule);
    EnhancedRouteRule getEnhancedRouteRule(const std::string& rule_id) const;
    std::vector<EnhancedRouteRule> getEnhancedRouteRules() const;
    
    // Detection and enforcement control
    bool startRouteDetection(int interval_ms = 1000);
    bool stopRouteDetection();
    bool startRouteEnforcement(bool strict_mode = false);
    bool stopRouteEnforcement();
    
    // Advanced monitoring and statistics
    json exportEnhancedRules() const;
    bool importEnhancedRules(const json& rules_json);
    
    // Route evaluation and enforcement
    bool shouldAllowRoute(const EnhancedRouteRule& route) const;
    bool enforceRoutePolicy(const EnhancedRouteRule& route);
    
    // Callbacks
    void setViolationCallback(std::function<void(
        const std::string& violation_type,
        const std::string& rule_id,
        const std::string& details
    )> callback);
};
```

### OpenVPN Route Conditions

#### Condition Types and Examples
```cpp
// Time-based condition
OpenVPNWrapper::EnhancedRouteRule::Condition timeCondition;
timeCondition.type = OpenVPNWrapper::EnhancedRouteRule::Condition::TIME_BASED;
timeCondition.negated = false;
timeCondition.value = "09:00-17:00,1-5";  // 9 AM to 5 PM, Monday-Friday

// Source IP condition
OpenVPNWrapper::EnhancedRouteRule::Condition sourceCondition;
sourceCondition.type = OpenVPNWrapper::EnhancedRouteRule::Condition::SOURCE_IP;
sourceCondition.negated = false;
sourceCondition.value = "192.168.1.0/24";

// Application condition
OpenVPNWrapper::EnhancedRouteRule::Condition appCondition;
appCondition.type = OpenVPNWrapper::EnhancedRouteRule::Condition::APPLICATION;
appCondition.negated = false;
appCondition.value = "/usr/bin/firefox";

// Interface condition
OpenVPNWrapper::EnhancedRouteRule::Condition interfaceCondition;
interfaceCondition.type = OpenVPNWrapper::EnhancedRouteRule::Condition::INTERFACE;
interfaceCondition.negated = false;
interfaceCondition.value = "tun0";
```

### OpenVPN Route Actions

#### Action Types and Examples
```cpp
// Block action
OpenVPNWrapper::EnhancedRouteRule::Action blockAction;
blockAction.type = OpenVPNWrapper::EnhancedRouteRule::Action::BLOCK;
blockAction.temporary = false;
blockAction.log_message = "Blocking traffic per policy";

// Redirect action
OpenVPNWrapper::EnhancedRouteRule::Action redirectAction;
redirectAction.type = OpenVPNWrapper::EnhancedRouteRule::Action::REDIRECT;
redirectAction.temporary = false;
redirectAction.action_value = "10.0.0.254";
redirectAction.log_message = "Redirecting traffic to alternative gateway";

// Force tunnel action
OpenVPNWrapper::EnhancedRouteRule::Action tunnelAction;
tunnelAction.type = OpenVPNWrapper::EnhancedRouteRule::Action::FORCE_TUNNEL;
tunnelAction.temporary = false;
tunnelAction.log_message = "Forcing traffic through VPN tunnel";
```

### C API Functions

#### Basic Routing API
```c
// Context management
ovpn_routing_ctx_t* ovpn_routing_init(const char *interface_name);
void ovpn_routing_cleanup(ovpn_routing_ctx_t *ctx);

// Callback setup
void ovpn_routing_set_callback(ovpn_routing_ctx_t *ctx,
                               ovpn_route_event_callback_t callback,
                               void *user_data);

// Rule management
int ovpn_routing_add_rule(ovpn_routing_ctx_t *ctx, const ovpn_route_rule_t *rule);
int ovpn_routing_remove_rule(ovpn_routing_ctx_t *ctx, const char *rule_id);
int ovpn_routing_update_rule(ovpn_routing_ctx_t *ctx,
                             const char *rule_id,
                             const ovpn_route_rule_t *updated_rule);

// Rule retrieval
int ovpn_routing_get_rule(ovpn_routing_ctx_t *ctx,
                          const char *rule_id,
                          ovpn_route_rule_t *out_rule);
int ovpn_routing_get_all_rules(ovpn_routing_ctx_t *ctx,
                               ovpn_route_rule_t **out_rules,
                               size_t *out_count);

// Route operations
int ovpn_routing_apply_rules(ovpn_routing_ctx_t *ctx);
int ovpn_routing_clear_routes(ovpn_routing_ctx_t *ctx);
int ovpn_routing_detect_routes(ovpn_routing_ctx_t *ctx);

// Monitoring
int ovpn_routing_start_monitoring(ovpn_routing_ctx_t *ctx, int interval_ms);
void ovpn_routing_stop_monitoring(ovpn_routing_ctx_t *ctx);

// Import/Export
char* ovpn_routing_export_json(ovpn_routing_ctx_t *ctx);
int ovpn_routing_import_json(ovpn_routing_ctx_t *ctx, const char *json_str);

// Statistics
int ovpn_routing_get_rule_stats(ovpn_routing_ctx_t *ctx,
                                const char *rule_id,
                                uint64_t *packets,
                                uint64_t *bytes);

// OpenVPN integration
int ovpn_routing_hook_openvpn(ovpn_routing_ctx_t *ctx, void *openvpn_ctx);
```

---

## Unified Routing Interface

### IVPNRoutingProvider Interface

Both libraries implement a unified interface for consistent routing operations:

```cpp
class IVPNRoutingProvider {
public:
    virtual ~IVPNRoutingProvider() = default;
    
    // Lifecycle management
    virtual bool initialize(const std::string& interface_name) = 0;
    virtual void cleanup() = 0;
    
    // Rule management
    virtual bool addRule(const UnifiedRouteRule& rule) = 0;
    virtual bool removeRule(const std::string& rule_id) = 0;
    virtual bool updateRule(const std::string& rule_id, const UnifiedRouteRule& rule) = 0;
    virtual UnifiedRouteRule getRule(const std::string& rule_id) const = 0;
    virtual std::vector<UnifiedRouteRule> getAllRules() const = 0;
    
    // Route operations
    virtual bool applyRules() = 0;
    virtual bool clearRoutes() = 0;
    virtual int detectRoutes() = 0;
    
    // Monitoring
    virtual bool startMonitoring(int interval_ms) = 0;
    virtual void stopMonitoring() = 0;
    
    // Callbacks
    virtual void setEventCallback(RouteEventCallback callback) = 0;
    
    // Import/Export
    virtual std::string exportRulesJson() const = 0;
    virtual bool importRulesJson(const std::string& json_str) = 0;
};
```

### Unified Route Rule Structure

```cpp
struct UnifiedRouteRule {
    std::string id;
    std::string name;
    std::string type;
    std::string destination;
    std::string gateway;
    std::string source_type;
    std::string source_value;
    std::string protocol;
    uint32_t metric;
    bool enabled;
    bool is_automatic;
    std::string description;
    
    json to_json() const;
    static UnifiedRouteRule from_json(const json& j);
};
```

### Route Event Types

```cpp
enum class RouteEventType {
    ADDED,
    REMOVED,
    MODIFIED,
    DETECTED,
    FAILED,
    STATS_UPDATE
};

using RouteEventCallback = std::function<void(
    RouteEventType event_type,
    const UnifiedRouteRule& rule,
    const std::string& error_msg
)>;
```

---

## Advanced Features

### Real-time Route Monitoring

Both libraries support real-time route monitoring through multiple mechanisms:

#### Netlink Monitoring (Preferred)
- Direct kernel communication for instant route change notifications
- Low overhead and high performance
- Available on Linux systems with proper netlink support

#### Polling Fallback
- Periodic route table scanning
- Compatible with all systems
- Configurable interval (default: 1000ms)

#### Event-driven Callbacks
```cpp
// Set up route event callback
wrapper.setRouteEventCallback([](
    const std::string& event_type,
    const RouteRule& rule,
    const std::string& error_msg
) {
    std::cout << "Route event: " << event_type 
              << " for rule: " << rule.id << std::endl;
    if (!error_msg.empty()) {
        std::cout << "Error: " << error_msg << std::endl;
    }
});
```

### Policy Enforcement

#### Enforcement Modes
```cpp
enum class EnforcementMode {
    PASSIVE,    // Log violations only
    WARNING,    // Log and warn user
    ACTIVE,     // Block violating traffic
    PREEMPTIVE  // Prevent violations before they occur
};
```

#### Violation Handling
```cpp
// Set up violation callback
wrapper.setViolationCallback([](
    const std::string& violation_type,
    const std::string& rule_id,
    const std::string& details
) {
    std::cout << "Policy violation: " << violation_type 
              << " for rule: " << rule_id << std::endl;
    std::cout << "Details: " << details << std::endl;
    
    // Implement custom violation handling logic
    if (violation_type == "ROUTE_LEAK") {
        // Take corrective action
    }
});
```

### Statistics and Monitoring

#### Route Statistics
```cpp
// Get detailed statistics for a specific rule
auto stats = wrapper.getRouteRuleStats("rule_id");
std::cout << "Packets routed: " << stats.packets_routed << std::endl;
std::cout << "Bytes routed: " << stats.bytes_routed << std::endl;
std::cout << "Last used: " << std::ctime(&stats.last_used) << std::endl;
```

#### System-wide Statistics
```cpp
// Get enhanced routing statistics (WireGuard)
auto enhancedStats = wgWrapper.getEnhancedRoutingStats();
std::cout << "Routes detected: " << enhancedStats.routes_detected << std::endl;
std::cout << "Violations detected: " << enhancedStats.violations_detected << std::endl;
std::cout << "Detection accuracy: " << enhancedStats.detection_accuracy << "%" << std::endl;
```

### Import/Export Functionality

#### Export Rules to JSON
```cpp
// Export all rules to JSON format
auto rulesJson = wrapper.exportEnhancedRules();
std::ofstream outputFile("routing-rules-backup.json");
outputFile << rulesJson.dump(2);
outputFile.close();
```

#### Import Rules from JSON
```cpp
// Import rules from JSON file
std::ifstream inputFile("routing-rules.json");
json rulesJson;
inputFile >> rulesJson;

if (wrapper.importEnhancedRules(rulesJson)) {
    std::cout << "Rules imported successfully" << std::endl;
} else {
    std::cout << "Failed to import rules" << std::endl;
}
```

### Policy Management

#### Apply Policy from File
```cpp
// Apply routing policy from JSON file
if (wrapper.applyPolicyFile("routing-policy.json")) {
    std::cout << "Policy applied successfully" << std::endl;
} else {
    std::cout << "Failed to apply policy" << std::endl;
}
```

#### Validate Policy
```cpp
// Validate policy before applying
std::string validationResult;
if (wrapper.validatePolicy(policyJson, validationResult)) {
    std::cout << "Policy is valid" << std::endl;
    // Apply the policy
    wrapper.applyPolicy(policyJson);
} else {
    std::cout << "Policy validation failed: " << validationResult << std::endl;
}
```

---

## Best Practices

### 1. Route Rule Design

#### Use Descriptive Names and IDs
```cpp
// Good
RouteRule officeRule;
officeRule.id = "office_network_tunnel_001";
officeRule.name = "Tunnel Office Network Traffic";
officeRule.description = "Route all office network traffic through VPN";

// Avoid
RouteRule rule1;
rule1.id = "rule1";
rule1.name = "Rule";
```

#### Set Appropriate Priorities
```cpp
// Higher priority rules are evaluated first
RouteRule criticalRule;
criticalRule.priority = 10;  // High priority for critical routes

RouteRule defaultRule;
defaultRule.priority = 1000;  // Low priority for default routes
```

#### Use Specific Destinations
```cpp
// Good - Specific network
rule.destination = "192.168.100.0/24";

// Avoid - Too broad
rule.destination = "0.0.0.0/0";
```

### 2. Performance Optimization

#### Enable Netlink Monitoring
```cpp
// Use netlink for better performance
wrapper.startRouteDetection(1000, true);  // true = enable netlink monitoring
```

#### Optimize Detection Intervals
```cpp
// Balance between responsiveness and overhead
wrapper.startRouteDetection(5000, false);  // 5 second interval for polling
```

#### Use Rule Conditions Efficiently
```cpp
// Order conditions from most specific to least specific
EnhancedRouteRule rule;
rule.conditions = {
    createPortCondition(80),      // Most specific
    createProtocolCondition("TCP"),
    createTimeCondition("09:00-17:00")  // Least specific
};
```

### 3. Error Handling

#### Always Check Return Values
```cpp
if (!wrapper.addRouteRule(rule)) {
    auto errorJson = wrapper.getLastErrorJson();
    std::cout << "Failed to add rule: " << errorJson["message"] << std::endl;
    // Handle error appropriately
}
```

#### Use Try-Catch for Exception Handling
```cpp
try {
    wrapper.connect();
    wrapper.startRouteDetection();
} catch (const std::exception& e) {
    std::cout << "Exception during routing setup: " << e.what() << std::endl;
    // Clean up resources
    wrapper.disconnect();
}
```

#### Implement Proper Cleanup
```cpp
// Always clean up enhanced routing
void cleanupRouting() {
    wrapper.stopRouteDetection();
    wrapper.stopRouteEnforcement();
    wrapper.cleanupEnhancedRouting();
    wrapper.disconnect();
}
```

### 4. Security Considerations

#### Validate Route Rules
```cpp
bool validateRouteRule(const RouteRule& rule) {
    // Check destination format
    if (!isValidCIDR(rule.destination)) {
        return false;
    }
    
    // Check metric range
    if (rule.metric > 10000) {
        return false;
    }
    
    // Validate protocol
    if (rule.protocol != "tcp" && rule.protocol != "udp" && 
        rule.protocol != "icmp" && rule.protocol != "both") {
        return false;
    }
    
    return true;
}
```

#### Use Least Privilege Principle
```cpp
// Only request necessary permissions
RouteRule rule;
rule.log_traffic = false;  // Don't log unless necessary
rule.enabled = true;       // Start disabled, enable after validation
```

#### Implement Rate Limiting
```cpp
// Use rate limiting actions to prevent abuse
RuleAction rateLimitAction;
rateLimitAction.type = ActionType::RATE_LIMIT;
rateLimitAction.data = json{
    {"rate_bps", 1048576},   // 1 Mbps limit
    {"burst_size", 104857}   // 100 KB burst
};
```

### 5. Monitoring and Debugging

#### Enable Verbose Logging
```cpp
// Enable verbose mode for debugging
manager.setVerbose(true);

// Set up detailed event callbacks
wrapper.setEventCallback([](const VPNEvent& event) {
    std::cout << "Event: " << event.type << " - " << event.message << std::endl;
    if (!event.data.empty()) {
        std::cout << "Data: " << event.data.dump(2) << std::endl;
    }
});
```

#### Monitor Route Statistics
```cpp
// Regularly check routing statistics
void monitorRoutingStats() {
    auto stats = wrapper.getEnhancedRoutingStats();
    std::cout << "Routes detected: " << stats.routes_detected << std::endl;
    std::cout << "Violations: " << stats.violations_detected << std::endl;
    std::cout << "Success rate: " << stats.enforcement_success_rate << "%" << std::endl;
}
```

#### Use JSON Export for Debugging
```cpp
// Export current state for analysis
void debugRoutingState() {
    auto rulesJson = wrapper.exportEnhancedRules();
    std::ofstream debugFile("debug-routing-state.json");
    debugFile << rulesJson.dump(4);
    debugFile.close();
}
```

---

## Troubleshooting

### Common Issues and Solutions

#### 1. Route Detection Fails

**Symptoms**: No automatic routes detected after VPN connection

**Causes and Solutions**:
- Interface name mismatch: Verify interface name matches VPN interface
- Timing issues: Increase detection delay after connection
- Permission issues: Ensure sufficient privileges for route operations

```cpp
// Add delay for route stabilization
std::this_thread::sleep_for(std::chrono::seconds(3));

// Verify interface exists
std::string checkCmd = "ip link show " + interface_name;
std::string result = executeCommand(checkCmd);
if (result.empty()) {
    std::cout << "Interface not found: " << interface_name << std::endl;
}
```

#### 2. Route Rules Not Applied

**Symptoms**: Routes added but not effective in routing table

**Causes and Solutions**:
- Rule conflicts: Check for conflicting existing routes
- Metric issues: Adjust rule priority/metric
- Gateway problems: Verify gateway is reachable

```cpp
// Check for route conflicts
bool checkRouteConflicts(const RouteRule& newRule) {
    auto existingRules = wrapper.getRouteRules();
    for (const auto& existing : existingRules) {
        if (existing.destination == newRule.destination && 
            existing.id != newRule.id) {
            std::cout << "Route conflict detected with: " << existing.id << std::endl;
            return true;
        }
    }
    return false;
}
```

#### 3. Performance Issues

**Symptoms**: High CPU usage or slow route operations

**Causes and Solutions**:
- Frequent polling: Increase detection interval
- Large rule sets: Optimize rule ordering
- Netlink issues: Fall back to polling

```cpp
// Optimize detection interval
wrapper.startRouteDetection(5000, false);  // 5 second interval

// Use netlink if available, otherwise polling
bool useNetlink = true;
if (!wrapper.startRouteDetection(1000, useNetlink)) {
    std::cout << "Netlink failed, using polling" << std::endl;
    wrapper.startRouteDetection(1000, false);
}
```

#### 4. Memory Leaks

**Symptoms**: Memory usage increases over time

**Causes and Solutions**:
- Unreleased contexts: Ensure proper cleanup
- Thread leaks: Stop monitoring threads
- Callback references: Clear callback references

```cpp
// Proper cleanup sequence
void cleanup() {
    wrapper.stopRouteDetection();
    wrapper.stopRouteEnforcement();
    wrapper.cleanupEnhancedRouting();
    wrapper.setEventCallback(nullptr);  // Clear callbacks
    wrapper.setViolationCallback(nullptr);
    wrapper.disconnect();
}
```

### Debug Commands

#### System Route Inspection
```bash
# Show current routing table
ip route show

# Show routes for specific interface
ip route show dev wg0

# Monitor route changes in real-time
ip monitor route
```

#### Interface Status
```bash
# Show interface status
ip link show

# Show interface addresses
ip addr show

# Show interface statistics
ip -s link show
```

#### WireGuard Specific
```bash
# Show WireGuard status
wg show

# Show WireGuard interface details
wg show wg0

# Monitor WireGuard handshake
watch -n 1 'wg show wg0 latest-handshakes'
```

#### OpenVPN Specific
```bash
# Show OpenVPN process status
ps aux | grep openvpn

# Show OpenVPN log
tail -f /var/log/openvpn.log

# Show tunnel interface status
ip addr show tun0
```

### Log Analysis

#### Enable Debug Logging
```cpp
// Enable verbose logging
manager.setVerbose(true);

// Set up comprehensive event logging
wrapper.setEventCallback([](const VPNEvent& event) {
    json logEntry;
    logEntry["timestamp"] = event.timestamp;
    logEntry["type"] = event.type;
    logEntry["message"] = event.message;
    logEntry["state"] = static_cast<int>(event.state);
    logEntry["data"] = event.data;
    
    // Write to log file
    std::ofstream logFile("vpn-routing.log", std::ios::app);
    logFile << logEntry.dump() << std::endl;
});
```

#### Analyze Route Events
```cpp
// Track route rule lifecycle
wrapper.setRouteEventCallback([](const std::string& event_type,
                                 const RouteRule& rule,
                                 const std::string& error_msg) {
    std::cout << "[" << event_type << "] Rule: " << rule.id 
              << " (" << rule.name << ")" << std::endl;
    
    if (!error_msg.empty()) {
        std::cout << "  Error: " << error_msg << std::endl;
    }
    
    // Log to file for analysis
    std::ofstream routeLog("route-events.log", std::ios::app);
    routeLog << event_type << "," << rule.id << "," << error_msg << std::endl;
});
```

---

## Conclusion

This guide provides a comprehensive overview of the routing collection and control methods available in the ur-vpn-extended stack. Both the WireGuard and OpenVPN libraries offer powerful routing capabilities with:

- **Automatic Route Detection**: Real-time discovery of VPN routes
- **Custom Rule Management**: Flexible rule creation and management
- **Advanced Conditions**: Time-based, IP-based, and application-based routing
- **Policy Enforcement**: Multiple enforcement modes and violation handling
- **Performance Monitoring**: Detailed statistics and performance metrics
- **Unified Interface**: Consistent API across both VPN technologies

By following the best practices and troubleshooting guidelines outlined in this document, you can effectively implement robust routing solutions for your VPN infrastructure.

For additional information, refer to the individual library documentation and test examples provided in the repository.
