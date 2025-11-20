#ifndef VPN_ROUTING_INTERFACE_HPP
#define VPN_ROUTING_INTERFACE_HPP

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vpn_manager {

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
    
    json to_json() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["type"] = type;
        j["destination"] = destination;
        j["gateway"] = gateway;
        j["source_type"] = source_type;
        j["source_value"] = source_value;
        j["protocol"] = protocol;
        j["metric"] = metric;
        j["enabled"] = enabled;
        j["is_automatic"] = is_automatic;
        j["description"] = description;
        return j;
    }
    
    static UnifiedRouteRule from_json(const json& j) {
        UnifiedRouteRule rule;
        rule.id = j.value("id", "");
        rule.name = j.value("name", "");
        rule.type = j.value("type", "automatic");
        rule.destination = j.value("destination", "");
        rule.gateway = j.value("gateway", "");
        rule.source_type = j.value("source_type", "Any");
        rule.source_value = j.value("source_value", "");
        rule.protocol = j.value("protocol", "both");
        rule.metric = j.value("metric", 100);
        rule.enabled = j.value("enabled", true);
        rule.is_automatic = j.value("is_automatic", false);
        rule.description = j.value("description", "");
        return rule;
    }
};

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

class IVPNRoutingProvider {
public:
    virtual ~IVPNRoutingProvider() = default;
    
    virtual bool initialize(const std::string& interface_name) = 0;
    virtual void cleanup() = 0;
    
    virtual bool addRule(const UnifiedRouteRule& rule) = 0;
    virtual bool removeRule(const std::string& rule_id) = 0;
    virtual bool updateRule(const std::string& rule_id, const UnifiedRouteRule& rule) = 0;
    virtual UnifiedRouteRule getRule(const std::string& rule_id) const = 0;
    virtual std::vector<UnifiedRouteRule> getAllRules() const = 0;
    
    virtual bool applyRules() = 0;
    virtual bool clearRoutes() = 0;
    virtual int detectRoutes() = 0;
    
    virtual bool startMonitoring(int interval_ms) = 0;
    virtual void stopMonitoring() = 0;
    
    virtual void setEventCallback(RouteEventCallback callback) = 0;
    
    virtual std::string exportRulesJson() const = 0;
    virtual bool importRulesJson(const std::string& json_str) = 0;
};

inline std::string routeEventTypeToString(RouteEventType event_type) {
    switch (event_type) {
        case RouteEventType::ADDED: return "added";
        case RouteEventType::REMOVED: return "removed";
        case RouteEventType::MODIFIED: return "modified";
        case RouteEventType::DETECTED: return "detected";
        case RouteEventType::FAILED: return "failed";
        case RouteEventType::STATS_UPDATE: return "stats_update";
        default: return "unknown";
    }
}

}

#endif
