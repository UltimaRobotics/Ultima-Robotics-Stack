
# Application Layer Routing Integration Implementation Plan

## Overview

This document outlines the detailed implementation plan for integrating VPN provider-level routing mechanisms directly into the `ur-vpn-extended/src` application layer, replacing the current centralized routing manager with provider-specific routing capabilities.

## Current Architecture

### Existing Routing Manager (To Be Replaced)

Currently, routing is managed centrally in:
- `src/vpn_manager_routing.cpp`
- `src/internal/vpn_manager_routing.hpp`

This implementation:
- Parses route output using system commands (`route -n`)
- Manages routing rules in a centralized JSON file
- Applies routes using `ip route` commands
- Monitors route changes through periodic polling

### Provider-Level Routing Implementations

Both VPN providers now have native routing capabilities:

**WireGuard Provider:**
- `ur-wg_library/ur-wg-provider/src/routing.c`
- `ur-wg_library/ur-wg-provider/include/routing.h`
- `ur-wg_library/ur-wg-provider/include/routing_api.h`

**OpenVPN Provider:**
- `ur-openvpn-library/src/source-port/apis/openvpn_routing.c`
- `ur-openvpn-library/src/source-port/apis/openvpn_routing.h`
- `ur-openvpn-library/src/source-port/apis/openvpn_routing_api.h`

**C Bridge Integration:**
- WireGuard: `ur-wg_library/wireguard-wrapper/src/wireguard_c_bridge.c`
- OpenVPN: `ur-openvpn-library/src/openvpn_c_bridge.c`

**C++ Wrapper Integration:**
- WireGuard: `ur-wg_library/wireguard-wrapper/src/wireguard_wrapper.cpp`
- OpenVPN: `ur-openvpn-library/src/openvpn_wrapper.cpp`

---

## Implementation Plan

### Phase 1: Application Layer Integration

#### 1.1 Create Unified Routing Interface

**File:** `src/vpn_routing_interface.hpp`

```cpp
#ifndef VPN_ROUTING_INTERFACE_HPP
#define VPN_ROUTING_INTERFACE_HPP

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vpn_manager {

// Unified route rule structure for application layer
struct UnifiedRouteRule {
    std::string id;
    std::string name;
    std::string type;              // "automatic", "tunnel", "exclude", "gateway"
    std::string destination;       // CIDR notation
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

// Route event types
enum class RouteEventType {
    ADDED,
    REMOVED,
    MODIFIED,
    DETECTED,
    FAILED,
    STATS_UPDATE
};

// Routing callback function
using RouteEventCallback = std::function<void(
    RouteEventType event_type,
    const UnifiedRouteRule& rule,
    const std::string& error_msg
)>;

// Abstract routing interface
class IVPNRoutingProvider {
public:
    virtual ~IVPNRoutingProvider() = default;
    
    // Lifecycle
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
    
    // Event handling
    virtual void setEventCallback(RouteEventCallback callback) = 0;
    
    // Export/Import
    virtual std::string exportRulesJson() const = 0;
    virtual bool importRulesJson(const std::string& json_str) = 0;
};

} // namespace vpn_manager

#endif
```

#### 1.2 WireGuard Routing Provider Adapter

**File:** `src/wireguard_routing_provider.hpp`

```cpp
#ifndef WIREGUARD_ROUTING_PROVIDER_HPP
#define WIREGUARD_ROUTING_PROVIDER_HPP

#include "vpn_routing_interface.hpp"
#include "../ur-wg_library/wireguard-wrapper/include/wireguard_wrapper.hpp"

namespace vpn_manager {

class WireGuardRoutingProvider : public IVPNRoutingProvider {
public:
    explicit WireGuardRoutingProvider(wireguard::WireGuardWrapper* wrapper);
    ~WireGuardRoutingProvider() override;
    
    bool initialize(const std::string& interface_name) override;
    void cleanup() override;
    
    bool addRule(const UnifiedRouteRule& rule) override;
    bool removeRule(const std::string& rule_id) override;
    bool updateRule(const std::string& rule_id, const UnifiedRouteRule& rule) override;
    UnifiedRouteRule getRule(const std::string& rule_id) const override;
    std::vector<UnifiedRouteRule> getAllRules() const override;
    
    bool applyRules() override;
    bool clearRoutes() override;
    int detectRoutes() override;
    
    bool startMonitoring(int interval_ms) override;
    void stopMonitoring() override;
    
    void setEventCallback(RouteEventCallback callback) override;
    
    std::string exportRulesJson() const override;
    bool importRulesJson(const std::string& json_str) override;

private:
    wireguard::WireGuardWrapper* wrapper_;
    RouteEventCallback event_callback_;
    
    // Convert between unified and WireGuard-specific formats
    wireguard::WireGuardWrapper::RouteRule toWireGuardRule(const UnifiedRouteRule& rule) const;
    UnifiedRouteRule fromWireGuardRule(const wireguard::WireGuardWrapper::RouteRule& rule) const;
    
    // Internal event handler
    void handleWireGuardEvent(const std::string& event_type,
                             const wireguard::WireGuardWrapper::RouteRule& rule,
                             const std::string& error_msg);
};

} // namespace vpn_manager

#endif
```

**File:** `src/wireguard_routing_provider.cpp`

Implementation that wraps the WireGuard wrapper's routing capabilities.

#### 1.3 OpenVPN Routing Provider Adapter

**File:** `src/openvpn_routing_provider.hpp`

```cpp
#ifndef OPENVPN_ROUTING_PROVIDER_HPP
#define OPENVPN_ROUTING_PROVIDER_HPP

#include "vpn_routing_interface.hpp"
#include "../ur-openvpn-library/src/openvpn_wrapper.hpp"

namespace vpn_manager {

class OpenVPNRoutingProvider : public IVPNRoutingProvider {
public:
    explicit OpenVPNRoutingProvider(openvpn::OpenVPNWrapper* wrapper);
    ~OpenVPNRoutingProvider() override;
    
    bool initialize(const std::string& interface_name) override;
    void cleanup() override;
    
    bool addRule(const UnifiedRouteRule& rule) override;
    bool removeRule(const std::string& rule_id) override;
    bool updateRule(const std::string& rule_id, const UnifiedRouteRule& rule) override;
    UnifiedRouteRule getRule(const std::string& rule_id) const override;
    std::vector<UnifiedRouteRule> getAllRules() const override;
    
    bool applyRules() override;
    bool clearRoutes() override;
    int detectRoutes() override;
    
    bool startMonitoring(int interval_ms) override;
    void stopMonitoring() override;
    
    void setEventCallback(RouteEventCallback callback) override;
    
    std::string exportRulesJson() const override;
    bool importRulesJson(const std::string& json_str) override;

private:
    openvpn::OpenVPNWrapper* wrapper_;
    RouteEventCallback event_callback_;
    
    // Convert between unified and OpenVPN-specific formats
    openvpn::OpenVPNWrapper::RouteRule toOpenVPNRule(const UnifiedRouteRule& rule) const;
    UnifiedRouteRule fromOpenVPNRule(const openvpn::OpenVPNWrapper::RouteRule& rule) const;
    
    // Internal event handler
    void handleOpenVPNEvent(const std::string& event_type,
                           const openvpn::OpenVPNWrapper::RouteRule& rule,
                           const std::string& error_msg);
};

} // namespace vpn_manager

#endif
```

**File:** `src/openvpn_routing_provider.cpp`

Implementation that wraps the OpenVPN wrapper's routing capabilities.

---

### Phase 2: VPN Instance Manager Integration

#### 2.1 Modify VPNInstance Structure

**File:** `src/vpn_instance_manager.hpp`

Add routing provider to VPNInstance:

```cpp
struct VPNInstance {
    // ... existing fields ...
    
    // Provider-specific routing
    std::unique_ptr<IVPNRoutingProvider> routing_provider;
    bool routing_initialized;
};
```

#### 2.2 Update Instance Lifecycle Methods

**File:** `src/vpn_manager_lifecycle.cpp`

**Connection Flow:**

```cpp
bool VPNInstanceManager::connectInstance(const std::string& instance_name) {
    // ... existing connection code ...
    
    // Initialize routing provider
    if (!initializeRoutingForInstance(instance_name)) {
        // Log error but continue (routing is optional)
    }
    
    // Apply pre-connection routes
    if (inst.routing_provider) {
        inst.routing_provider->applyRules();
    }
    
    // ... proceed with connection ...
    
    // After successful connection, detect routes
    if (inst.routing_provider && inst.current_state == ConnectionState::CONNECTED) {
        inst.routing_provider->detectRoutes();
    }
}
```

**Disconnection Flow:**

```cpp
bool VPNInstanceManager::disconnectInstance(const std::string& instance_name) {
    // Clear routes before disconnection
    if (inst.routing_provider) {
        inst.routing_provider->clearRoutes();
    }
    
    // ... existing disconnection code ...
    
    // Cleanup routing provider
    if (inst.routing_provider) {
        inst.routing_provider->cleanup();
    }
}
```

#### 2.3 Routing Initialization

**File:** `src/vpn_manager_routing.cpp` (refactored)

```cpp
bool VPNInstanceManager::initializeRoutingForInstance(const std::string& instance_name) {
    auto it = instances_.find(instance_name);
    if (it == instances_.end()) {
        return false;
    }
    
    auto& inst = it->second;
    
    // Create appropriate routing provider based on VPN type
    if (inst.type == VPNType::WIREGUARD) {
        auto* wg_wrapper = static_cast<wireguard::WireGuardWrapper*>(inst.vpn_wrapper.get());
        inst.routing_provider = std::make_unique<WireGuardRoutingProvider>(wg_wrapper);
    } else if (inst.type == VPNType::OPENVPN) {
        auto* ovpn_wrapper = static_cast<openvpn::OpenVPNWrapper*>(inst.vpn_wrapper.get());
        inst.routing_provider = std::make_unique<OpenVPNRoutingProvider>(ovpn_wrapper);
    } else {
        return false;
    }
    
    // Set event callback
    inst.routing_provider->setEventCallback(
        [this, instance_name](RouteEventType event_type, 
                             const UnifiedRouteRule& rule,
                             const std::string& error_msg) {
            handleRoutingEvent(instance_name, event_type, rule, error_msg);
        }
    );
    
    // Initialize with interface name
    std::string interface_name = getInterfaceForInstance(instance_name);
    if (!inst.routing_provider->initialize(interface_name)) {
        inst.routing_provider.reset();
        return false;
    }
    
    inst.routing_initialized = true;
    return true;
}
```

#### 2.4 Routing Event Handler

```cpp
void VPNInstanceManager::handleRoutingEvent(
    const std::string& instance_name,
    RouteEventType event_type,
    const UnifiedRouteRule& rule,
    const std::string& error_msg) {
    
    json event_json;
    event_json["type"] = "routing_event";
    event_json["instance"] = instance_name;
    event_json["event_type"] = routeEventTypeToString(event_type);
    event_json["rule"] = rule.to_json();
    
    if (!error_msg.empty()) {
        event_json["error"] = error_msg;
    }
    
    std::cout << event_json.dump() << std::endl;
    
    // Persist changes if automatic route detected
    if (event_type == RouteEventType::DETECTED && rule.is_automatic) {
        saveRoutingRulesToFile(instance_name);
    }
}
```

---

### Phase 3: HTTP API Integration

#### 3.1 New Routing Endpoints

**File:** `src/http_server.cpp`

```cpp
// Get routing rules for a specific instance
server.Get("/vpn/instances/:name/routes", [this](const Request& req, Response& res) {
    std::string instance_name = req.path_params.at("name");
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end()) {
        res.status = 404;
        res.set_content(json({{"error", "Instance not found"}}).dump(), "application/json");
        return;
    }
    
    if (!inst_it->second.routing_provider) {
        res.status = 400;
        res.set_content(json({{"error", "Routing not initialized"}}).dump(), "application/json");
        return;
    }
    
    auto rules = inst_it->second.routing_provider->getAllRules();
    json rules_json = json::array();
    for (const auto& rule : rules) {
        rules_json.push_back(rule.to_json());
    }
    
    res.set_content(rules_json.dump(), "application/json");
});

// Add routing rule to instance
server.Post("/vpn/instances/:name/routes", [this](const Request& req, Response& res) {
    std::string instance_name = req.path_params.at("name");
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
        res.status = 404;
        res.set_content(json({{"error", "Instance not found or routing not initialized"}}).dump(), 
                       "application/json");
        return;
    }
    
    try {
        json rule_json = json::parse(req.body);
        UnifiedRouteRule rule = UnifiedRouteRule::from_json(rule_json);
        
        if (inst_it->second.routing_provider->addRule(rule)) {
            res.set_content(json({{"status", "success"}}).dump(), "application/json");
        } else {
            res.status = 400;
            res.set_content(json({{"error", "Failed to add rule"}}).dump(), "application/json");
        }
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json({{"error", e.what()}}).dump(), "application/json");
    }
});

// Delete routing rule
server.Delete("/vpn/instances/:name/routes/:rule_id", [this](const Request& req, Response& res) {
    std::string instance_name = req.path_params.at("name");
    std::string rule_id = req.path_params.at("rule_id");
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
        res.status = 404;
        res.set_content(json({{"error", "Instance not found"}}).dump(), "application/json");
        return;
    }
    
    if (inst_it->second.routing_provider->removeRule(rule_id)) {
        res.set_content(json({{"status", "success"}}).dump(), "application/json");
    } else {
        res.status = 404;
        res.set_content(json({{"error", "Rule not found"}}).dump(), "application/json");
    }
});

// Apply routes (pre-connection)
server.Post("/vpn/instances/:name/routes/apply", [this](const Request& req, Response& res) {
    std::string instance_name = req.path_params.at("name");
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
        res.status = 404;
        res.set_content(json({{"error", "Instance not found"}}).dump(), "application/json");
        return;
    }
    
    if (inst_it->second.routing_provider->applyRules()) {
        res.set_content(json({{"status", "success"}}).dump(), "application/json");
    } else {
        res.status = 500;
        res.set_content(json({{"error", "Failed to apply rules"}}).dump(), "application/json");
    }
});

// Detect routes (post-connection)
server.Post("/vpn/instances/:name/routes/detect", [this](const Request& req, Response& res) {
    std::string instance_name = req.path_params.at("name");
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
        res.status = 404;
        res.set_content(json({{"error", "Instance not found"}}).dump(), "application/json");
        return;
    }
    
    int detected = inst_it->second.routing_provider->detectRoutes();
    res.set_content(json({{"detected_routes", detected}}).dump(), "application/json");
});
```

---

### Phase 4: Data Persistence

#### 4.1 Per-Instance Routing Files

Instead of a single centralized routing file, store routing rules per instance:

**Structure:**
```
config/
  vpn-configs/
    instance1-routes.json
    instance2-routes.json
```

#### 4.2 Load/Save Methods

**File:** `src/vpn_manager_routing.cpp`

```cpp
bool VPNInstanceManager::loadRoutingRulesForInstance(const std::string& instance_name) {
    std::string filename = routing_config_dir_ + "/" + instance_name + "-routes.json";
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
        return false;
    }
    
    std::ifstream file(filename);
    if (!file.good()) {
        // No existing routes file - this is OK
        return true;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    return inst_it->second.routing_provider->importRulesJson(content);
}

bool VPNInstanceManager::saveRoutingRulesForInstance(const std::string& instance_name) {
    std::string filename = routing_config_dir_ + "/" + instance_name + "-routes.json";
    
    auto inst_it = instances_.find(instance_name);
    if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
        return false;
    }
    
    std::string json_content = inst_it->second.routing_provider->exportRulesJson();
    
    std::ofstream file(filename);
    if (!file.good()) {
        return false;
    }
    
    file << json_content;
    file.close();
    
    return true;
}
```

---

### Phase 5: Migration Strategy

#### 5.1 Backward Compatibility

During migration, support both old and new routing systems:

1. **Detection:** Check if centralized routing rules exist
2. **Migration:** Convert old rules to per-instance format
3. **Cleanup:** Remove old centralized routing file after successful migration

**File:** `src/vpn_manager_migration.cpp`

```cpp
bool VPNInstanceManager::migrateRoutingRules() {
    std::string old_file = config_dir_ + "/routing-rules.json";
    
    std::ifstream file(old_file);
    if (!file.good()) {
        // No old routing file, no migration needed
        return true;
    }
    
    json old_data;
    file >> old_data;
    file.close();
    
    if (!old_data.contains("routing_rules")) {
        return false;
    }
    
    // Group rules by instance
    std::map<std::string, std::vector<UnifiedRouteRule>> instance_rules;
    
    for (const auto& rule_json : old_data["routing_rules"]) {
        std::string instance_name = rule_json.value("vpn_instance", "");
        if (instance_name.empty()) continue;
        
        UnifiedRouteRule rule = UnifiedRouteRule::from_json(rule_json);
        instance_rules[instance_name].push_back(rule);
    }
    
    // Import rules to each instance's routing provider
    for (const auto& [instance_name, rules] : instance_rules) {
        auto inst_it = instances_.find(instance_name);
        if (inst_it == instances_.end() || !inst_it->second.routing_provider) {
            continue;
        }
        
        for (const auto& rule : rules) {
            inst_it->second.routing_provider->addRule(rule);
        }
        
        saveRoutingRulesForInstance(instance_name);
    }
    
    // Backup old file
    std::rename(old_file.c_str(), (old_file + ".backup").c_str());
    
    return true;
}
```

---

### Phase 6: Testing Plan

#### 6.1 Unit Tests

**Test Files:**
- `tests/test_wireguard_routing_provider.cpp`
- `tests/test_openvpn_routing_provider.cpp`
- `tests/test_routing_migration.cpp`

**Test Cases:**
1. Provider initialization
2. Rule CRUD operations
3. Format conversion (Unified ↔ Provider-specific)
4. Event handling
5. Route detection
6. Route application
7. Migration from old system

#### 6.2 Integration Tests

**Scenarios:**
1. Connect instance → Detect routes → Verify persistence
2. Add custom rule → Apply → Verify system routes
3. Disconnect → Verify route cleanup
4. Multiple instances with different routing rules

#### 6.3 API Tests

**Test Files:**
- `tests/test_routing_api.py`

**Endpoints to Test:**
- GET `/vpn/instances/:name/routes`
- POST `/vpn/instances/:name/routes`
- DELETE `/vpn/instances/:name/routes/:rule_id`
- POST `/vpn/instances/:name/routes/apply`
- POST `/vpn/instances/:name/routes/detect`

---

### Phase 7: Documentation Updates

#### 7.1 API Documentation

Update API documentation to reflect new routing endpoints:
- `docs/api-reference.md`

#### 7.2 User Guide

Update user guide with new routing workflow:
- `docs/user-guide.md`

#### 7.3 Architecture Documentation

Update architecture diagrams:
- `docs/vpn-instance-manager-architecture.md`

---

## Implementation Timeline

### Week 1: Foundation
- Create unified routing interface
- Implement WireGuard routing provider adapter
- Implement OpenVPN routing provider adapter

### Week 2: Integration
- Modify VPNInstance structure
- Update lifecycle methods
- Implement routing initialization

### Week 3: API & Persistence
- Add HTTP API endpoints
- Implement per-instance persistence
- Create migration utility

### Week 4: Testing & Documentation
- Write unit tests
- Write integration tests
- Update documentation

### Week 5: Deployment & Cleanup
- Deploy to production
- Monitor for issues
- Remove old routing manager code

---

## Benefits of This Approach

1. **Provider-Native Routing:** Uses each VPN provider's native routing capabilities
2. **Better Performance:** Direct integration eliminates parsing overhead
3. **Real-Time Updates:** Event-driven architecture provides instant feedback
4. **Scalability:** Each instance manages its own routes independently
5. **Maintainability:** Clear separation of concerns
6. **Flexibility:** Easy to add new VPN types with routing support

---

## Rollback Plan

If issues arise during deployment:

1. **Keep Old Code:** Don't delete old routing manager initially
2. **Feature Flag:** Add configuration option to use old vs new routing
3. **Data Backup:** Backup routing rules before migration
4. **Quick Revert:** Ability to switch back to centralized routing if needed

---

## Future Enhancements

1. **Advanced Route Policies:** QoS, traffic shaping, bandwidth limits
2. **Route Analytics:** Track bandwidth usage per route
3. **Route Optimization:** Automatic metric adjustment based on performance
4. **Multi-Path Routing:** Route different traffic through different instances
5. **Conflict Detection:** Detect and resolve overlapping routes

---

## Conclusion

This implementation plan provides a clear path to integrate provider-level routing into the application layer, leveraging the existing WireGuard and OpenVPN routing implementations through their C bridges and C++ wrappers. The result will be a more robust, maintainable, and performant routing system.
