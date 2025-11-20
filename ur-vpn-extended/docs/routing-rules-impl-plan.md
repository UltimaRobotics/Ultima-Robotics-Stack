
# Routing Rules Implementation Plan: Provider-Level Architecture

## Overview

This document outlines the implementation plan for moving automated routing and custom routing rules from the application-layer VPN manager to the VPN provider libraries (WireGuard and OpenVPN). This architectural change will enable each VPN instance to manage its own routing tables directly at the provider level, with the application layer only consuming routing information through the existing bridges and wrappers.

## Architecture Goals

1. **Provider-Level Route Management**: Each VPN provider (WireGuard/OpenVPN) manages its own routing tables
2. **Automatic Route Detection**: Providers automatically detect and report routes established during connection
3. **Custom Route Application**: Providers accept and apply user-defined routing rules before/during connection
4. **Unified Interface**: Consistent routing API across both WireGuard and OpenVPN providers
5. **Bridge Integration**: Routing capabilities exposed through existing C bridges
6. **Wrapper Integration**: C++ wrappers provide high-level routing control

## Design Principles

- **Separation of Concerns**: Routing logic lives in provider libraries, application layer only coordinates
- **Real-time Updates**: Route changes are detected and reported via callbacks
- **Idempotent Operations**: Route add/remove operations are safe to repeat
- **Atomic Changes**: Route modifications are applied atomically where possible
- **Rollback Support**: Failed route operations can be rolled back

---

## Component 1: WireGuard Provider Routing System

### 1.1 Data Structures (`ur-wg-provider/include/routing.h`)

```c
#ifndef WIREGUARD_ROUTING_H
#define WIREGUARD_ROUTING_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>

#define MAX_ROUTE_RULES 256
#define MAX_ROUTE_DESCRIPTION 512

/* Route Types */
typedef enum {
    WG_ROUTE_TYPE_AUTOMATIC = 0,    // Automatically detected/applied by WireGuard
    WG_ROUTE_TYPE_CUSTOM_TUNNEL,    // User-defined: route through VPN
    WG_ROUTE_TYPE_CUSTOM_EXCLUDE,   // User-defined: exclude from VPN
    WG_ROUTE_TYPE_CUSTOM_GATEWAY    // User-defined: specific gateway
} wg_route_type_t;

/* Route Source Filter Types */
typedef enum {
    WG_ROUTE_SRC_ANY = 0,
    WG_ROUTE_SRC_IP_ADDRESS,
    WG_ROUTE_SRC_IP_RANGE,
    WG_ROUTE_SRC_SUBNET,
    WG_ROUTE_SRC_INTERFACE
} wg_route_src_type_t;

/* Route Protocol Filter */
typedef enum {
    WG_ROUTE_PROTO_BOTH = 0,
    WG_ROUTE_PROTO_TCP,
    WG_ROUTE_PROTO_UDP,
    WG_ROUTE_PROTO_ICMP
} wg_route_protocol_t;

/* Route State */
typedef enum {
    WG_ROUTE_STATE_PENDING = 0,
    WG_ROUTE_STATE_APPLIED,
    WG_ROUTE_STATE_FAILED,
    WG_ROUTE_STATE_REMOVED
} wg_route_state_t;

/* IPv4/IPv6 Union for Addresses */
typedef union {
    struct in_addr ipv4;
    struct in6_addr ipv6;
} wg_ip_addr_t;

/* Route Rule Structure */
typedef struct {
    char id[64];                        // Unique identifier
    char name[256];                     // Human-readable name
    
    /* Route Type and Classification */
    wg_route_type_t type;
    bool is_automatic;                  // True if detected automatically
    bool user_modified;                 // True if user changed auto-detected rule
    
    /* Source Filtering */
    wg_route_src_type_t src_type;
    wg_ip_addr_t src_addr;
    uint8_t src_prefix_len;             // For subnet/range
    char src_interface[32];             // For interface-based routing
    
    /* Destination */
    wg_ip_addr_t dest_addr;
    uint8_t dest_prefix_len;
    bool is_ipv6;
    
    /* Gateway and Routing */
    wg_ip_addr_t gateway;
    bool has_gateway;
    uint32_t metric;                    // Route priority/metric
    uint32_t table_id;                  // Routing table ID (default: main=254)
    
    /* Protocol Filtering */
    wg_route_protocol_t protocol;
    uint16_t port_start;                // 0 = any
    uint16_t port_end;                  // 0 = any
    
    /* State and Control */
    wg_route_state_t state;
    bool enabled;
    bool log_traffic;                   // Enable traffic logging for this route
    
    /* Metadata */
    char description[MAX_ROUTE_DESCRIPTION];
    time_t created_time;
    time_t modified_time;
    time_t applied_time;
    
    /* Statistics (if route is applied) */
    uint64_t packets_routed;
    uint64_t bytes_routed;
    time_t last_used;
} wg_route_rule_t;

/* Route Event Types */
typedef enum {
    WG_ROUTE_EVENT_ADDED = 0,
    WG_ROUTE_EVENT_REMOVED,
    WG_ROUTE_EVENT_MODIFIED,
    WG_ROUTE_EVENT_DETECTED,            // Automatic route detected
    WG_ROUTE_EVENT_FAILED,
    WG_ROUTE_EVENT_STATS_UPDATE
} wg_route_event_type_t;

/* Route Event Callback */
typedef void (*wg_route_event_callback_t)(
    wg_route_event_type_t event_type,
    const wg_route_rule_t *rule,
    const char *error_message,
    void *user_data
);

/* Routing Context (opaque) */
typedef struct wg_routing_ctx wg_routing_ctx_t;

#endif /* WIREGUARD_ROUTING_H */
```

### 1.2 Core Routing API (`ur-wg-provider/include/routing_api.h`)

```c
#ifndef WIREGUARD_ROUTING_API_H
#define WIREGUARD_ROUTING_API_H

#include "routing.h"

/* Initialize routing context for a WireGuard interface */
wg_routing_ctx_t* wg_routing_init(const char *interface_name);

/* Cleanup routing context */
void wg_routing_cleanup(wg_routing_ctx_t *ctx);

/* Set event callback for route changes */
void wg_routing_set_callback(wg_routing_ctx_t *ctx, 
                             wg_route_event_callback_t callback,
                             void *user_data);

/* Add a custom route rule (not yet applied) */
int wg_routing_add_rule(wg_routing_ctx_t *ctx, const wg_route_rule_t *rule);

/* Remove a route rule by ID */
int wg_routing_remove_rule(wg_routing_ctx_t *ctx, const char *rule_id);

/* Update an existing route rule */
int wg_routing_update_rule(wg_routing_ctx_t *ctx, 
                           const char *rule_id,
                           const wg_route_rule_t *updated_rule);

/* Get a specific route rule */
int wg_routing_get_rule(wg_routing_ctx_t *ctx, 
                        const char *rule_id,
                        wg_route_rule_t *out_rule);

/* Get all route rules */
int wg_routing_get_all_rules(wg_routing_ctx_t *ctx,
                             wg_route_rule_t **out_rules,
                             size_t *out_count);

/* Apply all enabled route rules to the system */
int wg_routing_apply_rules(wg_routing_ctx_t *ctx);

/* Remove all applied routes from the system */
int wg_routing_clear_routes(wg_routing_ctx_t *ctx);

/* Detect current routes for the interface and create automatic rules */
int wg_routing_detect_routes(wg_routing_ctx_t *ctx);

/* Start automatic route monitoring (detects changes in real-time) */
int wg_routing_start_monitoring(wg_routing_ctx_t *ctx, int interval_ms);

/* Stop route monitoring */
void wg_routing_stop_monitoring(wg_routing_ctx_t *ctx);

/* Export rules to JSON string (caller must free) */
char* wg_routing_export_json(wg_routing_ctx_t *ctx);

/* Import rules from JSON string */
int wg_routing_import_json(wg_routing_ctx_t *ctx, const char *json_str);

/* Get routing statistics for a specific rule */
int wg_routing_get_rule_stats(wg_routing_ctx_t *ctx,
                              const char *rule_id,
                              uint64_t *packets,
                              uint64_t *bytes);

#endif /* WIREGUARD_ROUTING_API_H */
```

### 1.3 Implementation Details

#### File: `ur-wg-provider/src/routing.c`

**Key Functions to Implement:**

1. **Route Detection** (`detect_system_routes()`)
   - Parse output from `ip route show` for the WireGuard interface
   - Parse output from `ip -6 route show` for IPv6
   - Extract: destination, gateway, metric, source constraints
   - Create automatic route rules for each detected route

2. **Route Application** (`apply_route_rule()`)
   - For TUNNEL routes: `ip route add <dest> dev <wg-iface> metric <metric>`
   - For EXCLUDE routes: `ip route add <dest> via <default-gw> metric <metric>`
   - For GATEWAY routes: `ip route add <dest> via <gateway> dev <wg-iface>`
   - Handle IPv6 with `ip -6 route`
   - Use netlink API for more efficient operations

3. **Route Removal** (`remove_route_rule()`)
   - Execute `ip route del <dest>` for IPv4
   - Execute `ip -6 route del <dest>` for IPv6
   - Handle cleanup of policy routing if used

4. **Route Monitoring** (`route_monitor_thread()`)
   - Use netlink socket to listen for RTMGRP_IPV4_ROUTE and RTMGRP_IPV6_ROUTE
   - Detect when routes are added/removed externally
   - Update internal route table
   - Fire callbacks for route changes

5. **JSON Import/Export** (`routing_to_json()`, `json_to_routing()`)
   - Serialize wg_route_rule_t array to JSON
   - Deserialize JSON to wg_route_rule_t array
   - Use cJSON library for parsing

#### File: `ur-wg-provider/src/routing_netlink.c`

**Netlink-based Route Operations:**

1. **Netlink Route Add** (`netlink_add_route()`)
   - Open netlink socket
   - Construct RTM_NEWROUTE message
   - Add RTA_DST, RTA_GATEWAY, RTA_OIF, RTA_PRIORITY attributes
   - Send and wait for acknowledgment

2. **Netlink Route Delete** (`netlink_del_route()`)
   - Construct RTM_DELROUTE message
   - Send deletion request

3. **Netlink Route Monitor** (`netlink_route_monitor()`)
   - Subscribe to RTMGRP_IPV4_ROUTE and RTMGRP_IPV6_ROUTE
   - Parse incoming RTM_NEWROUTE and RTM_DELROUTE messages
   - Extract route attributes
   - Fire callbacks when routes for our interface change

---

## Component 2: OpenVPN Provider Routing System

### 2.1 Data Structures (`ur-openvpn-library/src/source-port/apis/openvpn_routing.h`)

```c
#ifndef OPENVPN_ROUTING_H
#define OPENVPN_ROUTING_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>

#define OVPN_MAX_ROUTE_RULES 256
#define OVPN_MAX_ROUTE_DESC 512

/* Reuse similar enums and structures as WireGuard */
typedef enum {
    OVPN_ROUTE_TYPE_AUTOMATIC = 0,
    OVPN_ROUTE_TYPE_CUSTOM_TUNNEL,
    OVPN_ROUTE_TYPE_CUSTOM_EXCLUDE,
    OVPN_ROUTE_TYPE_CUSTOM_GATEWAY
} ovpn_route_type_t;

typedef enum {
    OVPN_ROUTE_SRC_ANY = 0,
    OVPN_ROUTE_SRC_IP_ADDRESS,
    OVPN_ROUTE_SRC_IP_RANGE,
    OVPN_ROUTE_SRC_SUBNET,
    OVPN_ROUTE_SRC_INTERFACE
} ovpn_route_src_type_t;

typedef enum {
    OVPN_ROUTE_PROTO_BOTH = 0,
    OVPN_ROUTE_PROTO_TCP,
    OVPN_ROUTE_PROTO_UDP,
    OVPN_ROUTE_PROTO_ICMP
} ovpn_route_protocol_t;

typedef enum {
    OVPN_ROUTE_STATE_PENDING = 0,
    OVPN_ROUTE_STATE_APPLIED,
    OVPN_ROUTE_STATE_FAILED,
    OVPN_ROUTE_STATE_REMOVED
} ovpn_route_state_t;

typedef union {
    struct in_addr ipv4;
    struct in6_addr ipv6;
} ovpn_ip_addr_t;

/* OpenVPN Route Rule Structure */
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

typedef enum {
    OVPN_ROUTE_EVENT_ADDED = 0,
    OVPN_ROUTE_EVENT_REMOVED,
    OVPN_ROUTE_EVENT_MODIFIED,
    OVPN_ROUTE_EVENT_DETECTED,
    OVPN_ROUTE_EVENT_FAILED,
    OVPN_ROUTE_EVENT_STATS_UPDATE
} ovpn_route_event_type_t;

typedef void (*ovpn_route_event_callback_t)(
    ovpn_route_event_type_t event_type,
    const ovpn_route_rule_t *rule,
    const char *error_message,
    void *user_data
);

typedef struct ovpn_routing_ctx ovpn_routing_ctx_t;

#endif /* OPENVPN_ROUTING_H */
```

### 2.2 Core Routing API (`ur-openvpn-library/src/source-port/apis/openvpn_routing_api.h`)

```c
#ifndef OPENVPN_ROUTING_API_H
#define OPENVPN_ROUTING_API_H

#include "openvpn_routing.h"

/* Initialize routing context for an OpenVPN session */
ovpn_routing_ctx_t* ovpn_routing_init(const char *interface_name);

/* Cleanup routing context */
void ovpn_routing_cleanup(ovpn_routing_ctx_t *ctx);

/* Set event callback */
void ovpn_routing_set_callback(ovpn_routing_ctx_t *ctx,
                               ovpn_route_event_callback_t callback,
                               void *user_data);

/* Add custom route rule */
int ovpn_routing_add_rule(ovpn_routing_ctx_t *ctx, const ovpn_route_rule_t *rule);

/* Remove route rule */
int ovpn_routing_remove_rule(ovpn_routing_ctx_t *ctx, const char *rule_id);

/* Update route rule */
int ovpn_routing_update_rule(ovpn_routing_ctx_t *ctx,
                             const char *rule_id,
                             const ovpn_route_rule_t *updated_rule);

/* Get specific rule */
int ovpn_routing_get_rule(ovpn_routing_ctx_t *ctx,
                          const char *rule_id,
                          ovpn_route_rule_t *out_rule);

/* Get all rules */
int ovpn_routing_get_all_rules(ovpn_routing_ctx_t *ctx,
                               ovpn_route_rule_t **out_rules,
                               size_t *out_count);

/* Apply all enabled rules */
int ovpn_routing_apply_rules(ovpn_routing_ctx_t *ctx);

/* Clear all applied routes */
int ovpn_routing_clear_routes(ovpn_routing_ctx_t *ctx);

/* Detect current routes */
int ovpn_routing_detect_routes(ovpn_routing_ctx_t *ctx);

/* Start route monitoring */
int ovpn_routing_start_monitoring(ovpn_routing_ctx_t *ctx, int interval_ms);

/* Stop route monitoring */
void ovpn_routing_stop_monitoring(ovpn_routing_ctx_t *ctx);

/* Export to JSON */
char* ovpn_routing_export_json(ovpn_routing_ctx_t *ctx);

/* Import from JSON */
int ovpn_routing_import_json(ovpn_routing_ctx_t *ctx, const char *json_str);

/* Get rule statistics */
int ovpn_routing_get_rule_stats(ovpn_routing_ctx_t *ctx,
                                const char *rule_id,
                                uint64_t *packets,
                                uint64_t *bytes);

/* Hook into OpenVPN route callbacks */
int ovpn_routing_hook_openvpn(ovpn_routing_ctx_t *ctx, struct context *openvpn_ctx);

#endif /* OPENVPN_ROUTING_API_H */
```

### 2.3 Implementation Details

#### File: `ur-openvpn-library/src/source-port/apis/openvpn_routing.c`

**Key Functions:**

1. **Integration with OpenVPN Route Callbacks** (`ovpn_routing_hook_openvpn()`)
   - Hook into OpenVPN's route.c callbacks
   - Intercept `add_route()` and `delete_route()` calls
   - Capture routes being added by OpenVPN server push
   - Create automatic route rules for pushed routes

2. **Route Detection** (`detect_openvpn_routes()`)
   - Parse `ip route show` output filtering for tun/tap interface
   - Special handling for OpenVPN's split-tunnel routes (0.0.0.0/1 and 128.0.0.0/1)
   - Detect redirect-gateway configurations
   - Extract DNS push routes

3. **Custom Route Pre-Application** (`apply_custom_routes_before_connect()`)
   - Apply user-defined routes before OpenVPN connection
   - Set up policy routing tables if needed
   - Configure iptables rules for protocol-specific routing

4. **Route Conflict Resolution** (`resolve_route_conflicts()`)
   - Detect conflicting routes (user vs. automatic)
   - Priority: user_modified > custom > automatic
   - Merge or replace routes based on priority

5. **OpenVPN Script Integration** (`generate_route_script()`)
   - Generate up/down scripts for OpenVPN to execute
   - Scripts apply custom routes that OpenVPN doesn't handle
   - Pass to OpenVPN via `--up` and `--down` options

---

## Component 3: Bridge Integration

### 3.1 WireGuard Bridge Extensions (`ur-wg_library/wireguard-wrapper/include/wireguard_c_bridge.h`)

Add to existing bridge header:

```c
/* Routing context handle */
typedef void* wireguard_routing_ctx_t;

/* Initialize routing for this bridge context */
wireguard_routing_ctx_t wireguard_bridge_routing_init(wireguard_bridge_ctx_t *ctx);

/* Cleanup routing */
void wireguard_bridge_routing_cleanup(wireguard_routing_ctx_t routing_ctx);

/* Add custom route rule (JSON format) */
int wireguard_bridge_routing_add_rule_json(wireguard_routing_ctx_t routing_ctx,
                                           const char *rule_json);

/* Remove route rule */
int wireguard_bridge_routing_remove_rule(wireguard_routing_ctx_t routing_ctx,
                                         const char *rule_id);

/* Get all routes as JSON array */
char* wireguard_bridge_routing_get_all_json(wireguard_routing_ctx_t routing_ctx);

/* Apply routes before connection */
int wireguard_bridge_routing_apply_pre_connect(wireguard_routing_ctx_t routing_ctx);

/* Detect routes after connection */
int wireguard_bridge_routing_detect_post_connect(wireguard_routing_ctx_t routing_ctx);

/* Set routing event callback */
typedef void (*wireguard_bridge_route_callback_t)(
    const char *event_type,
    const char *rule_json,
    const char *error_msg,
    void *user_data
);

void wireguard_bridge_routing_set_callback(wireguard_routing_ctx_t routing_ctx,
                                           wireguard_bridge_route_callback_t callback,
                                           void *user_data);
```

### 3.2 OpenVPN Bridge Extensions (`ur-openvpn-library/src/openvpn_c_bridge.h`)

Add to existing bridge header:

```c
/* Routing context handle */
typedef void* openvpn_routing_ctx_t;

/* Initialize routing for this bridge context */
openvpn_routing_ctx_t openvpn_bridge_routing_init(openvpn_bridge_ctx_t *ctx);

/* Cleanup routing */
void openvpn_bridge_routing_cleanup(openvpn_routing_ctx_t routing_ctx);

/* Add custom route rule (JSON format) */
int openvpn_bridge_routing_add_rule_json(openvpn_routing_ctx_t routing_ctx,
                                         const char *rule_json);

/* Remove route rule */
int openvpn_bridge_routing_remove_rule(openvpn_routing_ctx_t routing_ctx,
                                       const char *rule_id);

/* Get all routes as JSON array */
char* openvpn_bridge_routing_get_all_json(openvpn_routing_ctx_t routing_ctx);

/* Apply routes before connection */
int openvpn_bridge_routing_apply_pre_connect(openvpn_routing_ctx_t routing_ctx);

/* Detect routes after connection */
int openvpn_bridge_routing_detect_post_connect(openvpn_routing_ctx_t routing_ctx);

/* Set routing event callback */
typedef void (*openvpn_bridge_route_callback_t)(
    const char *event_type,
    const char *rule_json,
    const char *error_msg,
    void *user_data
);

void openvpn_bridge_routing_set_callback(openvpn_routing_ctx_t routing_ctx,
                                         openvpn_bridge_route_callback_t callback,
                                         void *user_data);
```

---

## Component 4: C++ Wrapper Integration

### 4.1 WireGuard Wrapper Extensions (`ur-wg_library/wireguard-wrapper/include/wireguard_wrapper.hpp`)

Add to WireGuardWrapper class:

```cpp
class WireGuardWrapper {
public:
    // ... existing methods ...
    
    /* Routing Management */
    struct RouteRule {
        std::string id;
        std::string name;
        std::string type;              // "automatic", "tunnel", "exclude", "gateway"
        std::string destination;       // CIDR notation
        std::string gateway;           // Optional
        std::string source_type;       // "any", "ip", "subnet", "interface"
        std::string source_value;      // Depends on source_type
        std::string protocol;          // "both", "tcp", "udp", "icmp"
        uint32_t metric;
        bool enabled;
        bool is_automatic;
        std::string description;
        json to_json() const;
        static RouteRule from_json(const json& j);
    };
    
    // Add custom route rule
    bool addRouteRule(const RouteRule& rule);
    
    // Remove route rule by ID
    bool removeRouteRule(const std::string& rule_id);
    
    // Get all route rules
    std::vector<RouteRule> getRouteRules() const;
    
    // Get specific route rule
    RouteRule getRouteRule(const std::string& rule_id) const;
    
    // Apply pre-connection routes (call before connect())
    bool applyPreConnectionRoutes();
    
    // Detect post-connection routes (call after connect())
    bool detectPostConnectionRoutes();
    
    // Set route event callback
    using RouteEventCallback = std::function<void(
        const std::string& event_type,
        const RouteRule& rule,
        const std::string& error_msg
    )>;
    void setRouteEventCallback(RouteEventCallback callback);
    
private:
    wireguard_routing_ctx_t routing_ctx_;
    RouteEventCallback route_event_callback_;
    
    static void route_callback_wrapper(
        const char* event_type,
        const char* rule_json,
        const char* error_msg,
        void* user_data
    );
};
```

### 4.2 OpenVPN Wrapper Extensions (`ur-openvpn-library/src/openvpn_wrapper.hpp`)

Add to OpenVPNWrapper class:

```cpp
class OpenVPNWrapper {
public:
    // ... existing methods ...
    
    /* Routing Management - Same interface as WireGuard */
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
    
    bool addRouteRule(const RouteRule& rule);
    bool removeRouteRule(const std::string& rule_id);
    std::vector<RouteRule> getRouteRules() const;
    RouteRule getRouteRule(const std::string& rule_id) const;
    bool applyPreConnectionRoutes();
    bool detectPostConnectionRoutes();
    
    using RouteEventCallback = std::function<void(
        const std::string& event_type,
        const RouteRule& rule,
        const std::string& error_msg
    )>;
    void setRouteEventCallback(RouteEventCallback callback);
    
private:
    openvpn_routing_ctx_t routing_ctx_;
    RouteEventCallback route_event_callback_;
    
    static void route_callback_wrapper(
        const char* event_type,
        const char* rule_json,
        const char* error_msg,
        void* user_data
    );
};
```

---

## Component 5: Application Layer Integration

### 5.1 VPN Instance Manager Changes

The VPN Instance Manager will be simplified to only:

1. **Store routing preferences** per instance in configuration files
2. **Forward routing commands** to the appropriate wrapper
3. **Aggregate routing information** from all instances for display
4. **Persist routing rules** to disk (delegated to providers)

### 5.2 Modified VPN Instance Manager API

```cpp
// In vpn_instance_manager.hpp

class VPNInstanceManager {
public:
    // ... existing methods ...
    
    /* Routing Management - Delegated to Providers */
    
    // Add route rule to a specific instance
    bool addInstanceRouteRule(const std::string& instance_name,
                             const json& rule_json);
    
    // Remove route rule from instance
    bool removeInstanceRouteRule(const std::string& instance_name,
                                const std::string& rule_id);
    
    // Get all routes for an instance
    json getInstanceRoutes(const std::string& instance_name);
    
    // Get all routes from all instances (aggregated)
    json getAllRoutes();
    
    // Load routing rules from config file into instance
    bool loadInstanceRoutingConfig(const std::string& instance_name,
                                   const std::string& config_file);
    
    // Save routing rules from instance to config file
    bool saveInstanceRoutingConfig(const std::string& instance_name,
                                   const std::string& config_file);
    
private:
    // Routing is now handled by wrappers, not directly by manager
    // Remove: routing_rules_, routing_mutex_, routing-specific methods
    
    // Helper to get routing context from wrapper
    void* getRoutingContextForInstance(const std::string& instance_name);
};
```

### 5.3 Connection Workflow Integration

**Modified Connection Sequence:**

```
1. User calls: connect(instance_name)
2. Manager loads routing config for instance
3. Manager calls wrapper->applyPreConnectionRoutes()
4. Manager calls wrapper->connect()
5. On connection success, wrapper automatically calls detectPostConnectionRoutes()
6. Detected routes trigger route event callbacks
7. Manager receives route events and updates UI/logs
```

**Modified Disconnection Sequence:**

```
1. User calls: disconnect(instance_name)
2. Manager calls wrapper->disconnect()
3. Wrapper automatically removes all applied routes
4. Route removal events are fired
5. Manager receives events and updates state
```

---

## Component 6: Configuration and Persistence

### 6.1 Routing Configuration File Format

Each VPN instance has its own routing configuration file: `config/vpn-configs/<instance-name>-routes.json`

```json
{
  "instance_name": "wg0-home",
  "routing_rules": [
    {
      "id": "rule-001",
      "name": "Route all traffic through VPN",
      "type": "tunnel",
      "destination": "0.0.0.0/0",
      "source_type": "any",
      "protocol": "both",
      "metric": 100,
      "enabled": true,
      "is_automatic": false,
      "description": "User-defined default route"
    },
    {
      "id": "auto-001",
      "name": "Auto-detected LAN route",
      "type": "automatic",
      "destination": "192.168.1.0/24",
      "source_type": "any",
      "protocol": "both",
      "metric": 50,
      "enabled": true,
      "is_automatic": true,
      "user_modified": false,
      "description": "Automatically detected route"
    }
  ],
  "last_modified": "2025-01-15T10:30:00Z"
}
```

### 6.2 Master Configuration Integration

The master `config.json` references routing config files:

```json
{
  "vpn_profiles": [
    {
      "name": "wg0-home",
      "type": "wireguard",
      "config_file": "/path/to/wg0.conf",
      "routing_config": "/path/to/wg0-home-routes.json",
      "auto_apply_routes": true,
      "auto_detect_routes": true
    }
  ]
}
```

---

## Component 7: Testing Strategy

### 7.1 Unit Tests

**WireGuard Routing Tests** (`ur-wg-provider/tests/test_routing.c`)

1. Test route rule creation and validation
2. Test route application (mock netlink)
3. Test route detection from system
4. Test JSON import/export
5. Test route conflict resolution
6. Test route removal and cleanup

**OpenVPN Routing Tests** (`ur-openvpn-library/tests/test_routing.c`)

1. Similar test coverage as WireGuard
2. Additional: Test OpenVPN hook integration
3. Test script generation for complex routes
4. Test split-tunnel route handling

### 7.2 Integration Tests

**Bridge Integration Tests**

1. Test routing through C bridges
2. Test callback mechanisms
3. Test JSON serialization across bridge boundary

**Wrapper Integration Tests**

1. Test C++ wrapper routing methods
2. Test route event callbacks
3. Test pre/post connection route operations

**Application Layer Tests**

1. Test VPNInstanceManager routing delegation
2. Test multi-instance routing coordination
3. Test configuration persistence
4. Test route aggregation and querying

### 7.3 System Tests

1. **Full Connection Test**: Connect VPN, verify routes applied
2. **Route Persistence Test**: Disconnect/reconnect, verify routes restored
3. **Conflict Test**: Apply conflicting routes, verify resolution
4. **Multi-Instance Test**: Run 2 VPNs simultaneously with different routes
5. **Cleanup Test**: Verify all routes removed on shutdown

---

## Component 8: Migration Plan

### Phase 1: Foundation (Week 1-2)

1. Implement WireGuard routing structures and core API
2. Implement OpenVPN routing structures and core API
3. Add basic route detection functionality
4. Add basic route application/removal

### Phase 2: Bridge Integration (Week 2-3)

1. Extend WireGuard C bridge with routing functions
2. Extend OpenVPN C bridge with routing functions
3. Implement JSON serialization for routes
4. Add route event callbacks

### Phase 3: Wrapper Integration (Week 3-4)

1. Add routing methods to WireGuardWrapper
2. Add routing methods to OpenVPNWrapper
3. Implement C++ route event handling
4. Add pre/post connection route hooks

### Phase 4: Application Integration (Week 4-5)

1. Modify VPNInstanceManager to delegate routing
2. Remove old routing manager code
3. Update connection/disconnection workflows
4. Implement configuration persistence

### Phase 5: Testing and Refinement (Week 5-6)

1. Write and run all unit tests
2. Perform integration testing
3. System-level testing
4. Bug fixes and optimization

### Phase 6: Documentation and Deployment (Week 6)

1. Complete API documentation
2. Write user guide for routing features
3. Update main README
4. Prepare for deployment

---

## Component 9: API Usage Examples

### 9.1 WireGuard Example (C)

```c
#include "routing_api.h"

void route_event_handler(wg_route_event_type_t type,
                        const wg_route_rule_t *rule,
                        const char *error,
                        void *user_data) {
    printf("Route event: %d for rule %s\n", type, rule->name);
}

int main() {
    // Initialize routing
    wg_routing_ctx_t *routing = wg_routing_init("wg0");
    wg_routing_set_callback(routing, route_event_handler, NULL);
    
    // Add custom route
    wg_route_rule_t rule = {0};
    strncpy(rule.id, "rule-001", sizeof(rule.id));
    strncpy(rule.name, "Route to LAN", sizeof(rule.name));
    rule.type = WG_ROUTE_TYPE_CUSTOM_TUNNEL;
    inet_pton(AF_INET, "192.168.1.0", &rule.dest_addr.ipv4);
    rule.dest_prefix_len = 24;
    rule.metric = 100;
    rule.enabled = true;
    
    wg_routing_add_rule(routing, &rule);
    wg_routing_apply_rules(routing);
    
    // Detect automatic routes
    wg_routing_detect_routes(routing);
    
    // Export to JSON
    char *json = wg_routing_export_json(routing);
    printf("Routes: %s\n", json);
    free(json);
    
    // Cleanup
    wg_routing_cleanup(routing);
    return 0;
}
```

### 9.2 OpenVPN Example (C++)

```cpp
#include "openvpn_wrapper.hpp"

int main() {
    OpenVPNWrapper vpn;
    
    // Set route event callback
    vpn.setRouteEventCallback([](const std::string& event,
                                  const OpenVPNWrapper::RouteRule& rule,
                                  const std::string& error) {
        std::cout << "Route " << event << ": " << rule.name << std::endl;
    });
    
    // Add custom route before connection
    OpenVPNWrapper::RouteRule rule;
    rule.id = "custom-001";
    rule.name = "Exclude local subnet";
    rule.type = "exclude";
    rule.destination = "10.0.0.0/8";
    rule.metric = 50;
    rule.enabled = true;
    
    vpn.addRouteRule(rule);
    
    // Apply routes and connect
    vpn.initializeFromFile("config.ovpn");
    vpn.applyPreConnectionRoutes();
    vpn.connect();
    
    // Routes are automatically detected after connection
    // Wait for connection
    while (!vpn.isConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Get all routes
    auto routes = vpn.getRouteRules();
    for (const auto& r : routes) {
        std::cout << r.to_json().dump(2) << std::endl;
    }
    
    return 0;
}
```

### 9.3 VPN Instance Manager Example (C++)

```cpp
#include "vpn_instance_manager.hpp"

int main() {
    VPNInstanceManager manager;
    
    // Load instance configuration
    manager.loadInstanceConfig("config.json");
    
    // Add custom route to instance
    json route_rule = {
        {"id", "rule-001"},
        {"name", "Corporate network"},
        {"type", "tunnel"},
        {"destination", "172.16.0.0/12"},
        {"metric", 200},
        {"enabled", true}
    };
    
    manager.addInstanceRouteRule("vpn-corp", route_rule.dump());
    
    // Save routing configuration
    manager.saveInstanceRoutingConfig("vpn-corp",
                                      "config/vpn-corp-routes.json");
    
    // Connect (routes applied automatically)
    manager.connect("vpn-corp");
    
    // Query routes from all instances
    json all_routes = manager.getAllRoutes();
    std::cout << all_routes.dump(2) << std::endl;
    
    return 0;
}
```

---

## Component 10: Performance Considerations

### 10.1 Route Detection Performance

- Use netlink for efficient route monitoring instead of polling `ip route`
- Cache parsed routes to avoid repeated parsing
- Debounce route detection (wait 500ms after last change before processing)

### 10.2 Route Application Performance

- Batch route operations when possible
- Use netlink for direct kernel communication
- Avoid spawning shell processes for each route change

### 10.3 Memory Management

- Use fixed-size arrays where possible (MAX_ROUTE_RULES)
- Free JSON strings after use
- Clear route caches on disconnect

---

## Component 11: Security Considerations

### 11.1 Route Validation

- Validate CIDR notation before applying routes
- Prevent route injection attacks via JSON parsing
- Sanitize user-provided route descriptions

### 11.2 Privilege Management

- Route operations require root/CAP_NET_ADMIN
- Validate permissions before route application
- Log all route changes for audit

### 11.3 Conflict Prevention

- Prevent routes that could break VPN connection
- Warn about routes that conflict with system routes
- Implement safe rollback on route application failure

---

## Summary

This implementation plan moves routing logic from the application layer into the VPN provider libraries (WireGuard and OpenVPN), creating a cleaner architecture where:

1. **Providers own routing**: Each VPN type manages its own routes
2. **Automatic detection**: Routes are detected and tracked in real-time
3. **Custom rules supported**: Users can define complex routing rules
4. **Unified interface**: Both WireGuard and OpenVPN expose identical routing APIs
5. **Application delegates**: VPN Instance Manager simply coordinates and persists

The migration can be done incrementally over 6 weeks, with each phase building on the previous one. The final result will be more maintainable, testable, and aligned with the principle that each component should own its domain-specific logic.
