#include "wireguard_routing_provider.hpp"

namespace vpn_manager {

WireGuardRoutingProvider::WireGuardRoutingProvider(wireguard::WireGuardWrapper* wrapper)
    : wrapper_(wrapper), event_callback_(nullptr) {
}

WireGuardRoutingProvider::~WireGuardRoutingProvider() {
    cleanup();
}

bool WireGuardRoutingProvider::initialize(const std::string& interface_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    interface_name_ = interface_name;
    
    wrapper_->setRouteEventCallback(
        [this](const std::string& event_type,
               const wireguard::WireGuardWrapper::RouteRule& rule,
               const std::string& error_msg) {
            handleWireGuardEvent(event_type, rule, error_msg);
        }
    );
    
    return true;
}

void WireGuardRoutingProvider::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = nullptr;
    wrapper_->setRouteEventCallback(nullptr);
}

bool WireGuardRoutingProvider::addRule(const UnifiedRouteRule& rule) {
    auto wg_rule = toWireGuardRule(rule);
    return wrapper_->addRouteRule(wg_rule);
}

bool WireGuardRoutingProvider::removeRule(const std::string& rule_id) {
    return wrapper_->removeRouteRule(rule_id);
}

bool WireGuardRoutingProvider::updateRule(const std::string& rule_id, const UnifiedRouteRule& rule) {
    if (!removeRule(rule_id)) {
        return false;
    }
    return addRule(rule);
}

UnifiedRouteRule WireGuardRoutingProvider::getRule(const std::string& rule_id) const {
    auto wg_rule = wrapper_->getRouteRule(rule_id);
    return fromWireGuardRule(wg_rule);
}

std::vector<UnifiedRouteRule> WireGuardRoutingProvider::getAllRules() const {
    auto wg_rules = wrapper_->getRouteRules();
    std::vector<UnifiedRouteRule> rules;
    rules.reserve(wg_rules.size());
    
    for (const auto& wg_rule : wg_rules) {
        rules.push_back(fromWireGuardRule(wg_rule));
    }
    
    return rules;
}

bool WireGuardRoutingProvider::applyRules() {
    return wrapper_->applyPreConnectionRoutes();
}

bool WireGuardRoutingProvider::clearRoutes() {
    auto rules = wrapper_->getRouteRules();
    bool success = true;
    
    for (const auto& rule : rules) {
        if (!wrapper_->removeRouteRule(rule.id)) {
            success = false;
        }
    }
    
    return success;
}

int WireGuardRoutingProvider::detectRoutes() {
    if (!wrapper_->detectPostConnectionRoutes()) {
        return 0;
    }
    
    return static_cast<int>(wrapper_->getRouteRules().size());
}

bool WireGuardRoutingProvider::startMonitoring(int interval_ms) {
    return true;
}

void WireGuardRoutingProvider::stopMonitoring() {
}

void WireGuardRoutingProvider::setEventCallback(RouteEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = callback;
}

std::string WireGuardRoutingProvider::exportRulesJson() const {
    json rules_array = json::array();
    auto rules = getAllRules();
    
    for (const auto& rule : rules) {
        rules_array.push_back(rule.to_json());
    }
    
    json data;
    data["routing_rules"] = rules_array;
    return data.dump(2);
}

bool WireGuardRoutingProvider::importRulesJson(const std::string& json_str) {
    try {
        json data = json::parse(json_str);
        
        if (!data.contains("routing_rules") || !data["routing_rules"].is_array()) {
            return false;
        }
        
        for (const auto& rule_json : data["routing_rules"]) {
            UnifiedRouteRule rule = UnifiedRouteRule::from_json(rule_json);
            addRule(rule);
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

wireguard::WireGuardWrapper::RouteRule WireGuardRoutingProvider::toWireGuardRule(
    const UnifiedRouteRule& rule) const {
    
    wireguard::WireGuardWrapper::RouteRule wg_rule;
    wg_rule.id = rule.id;
    wg_rule.name = rule.name;
    wg_rule.type = rule.type;
    wg_rule.destination = rule.destination;
    wg_rule.gateway = rule.gateway;
    wg_rule.source_type = rule.source_type;
    wg_rule.source_value = rule.source_value;
    wg_rule.protocol = rule.protocol;
    wg_rule.metric = rule.metric;
    wg_rule.enabled = rule.enabled;
    wg_rule.is_automatic = rule.is_automatic;
    wg_rule.description = rule.description;
    
    return wg_rule;
}

UnifiedRouteRule WireGuardRoutingProvider::fromWireGuardRule(
    const wireguard::WireGuardWrapper::RouteRule& rule) const {
    
    UnifiedRouteRule unified_rule;
    unified_rule.id = rule.id;
    unified_rule.name = rule.name;
    unified_rule.type = rule.type;
    unified_rule.destination = rule.destination;
    unified_rule.gateway = rule.gateway;
    unified_rule.source_type = rule.source_type;
    unified_rule.source_value = rule.source_value;
    unified_rule.protocol = rule.protocol;
    unified_rule.metric = rule.metric;
    unified_rule.enabled = rule.enabled;
    unified_rule.is_automatic = rule.is_automatic;
    unified_rule.description = rule.description;
    
    return unified_rule;
}

void WireGuardRoutingProvider::handleWireGuardEvent(
    const std::string& event_type,
    const wireguard::WireGuardWrapper::RouteRule& rule,
    const std::string& error_msg) {
    
    RouteEventCallback callback_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!event_callback_) {
            return;
        }
        callback_copy = event_callback_;
    }
    
    RouteEventType unified_event_type;
    if (event_type == "added") {
        unified_event_type = RouteEventType::ADDED;
    } else if (event_type == "removed") {
        unified_event_type = RouteEventType::REMOVED;
    } else if (event_type == "modified") {
        unified_event_type = RouteEventType::MODIFIED;
    } else if (event_type == "detected") {
        unified_event_type = RouteEventType::DETECTED;
    } else if (event_type == "failed") {
        unified_event_type = RouteEventType::FAILED;
    } else {
        unified_event_type = RouteEventType::STATS_UPDATE;
    }
    
    auto unified_rule = fromWireGuardRule(rule);
    callback_copy(unified_event_type, unified_rule, error_msg);
}

}
