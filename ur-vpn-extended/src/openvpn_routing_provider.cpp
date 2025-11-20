#include "openvpn_routing_provider.hpp"

namespace vpn_manager {

OpenVPNRoutingProvider::OpenVPNRoutingProvider(openvpn::OpenVPNWrapper* wrapper)
    : wrapper_(wrapper), event_callback_(nullptr) {
}

OpenVPNRoutingProvider::~OpenVPNRoutingProvider() {
    cleanup();
}

bool OpenVPNRoutingProvider::initialize(const std::string& interface_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    interface_name_ = interface_name;
    
    wrapper_->setRouteEventCallback(
        [this](const std::string& event_type,
               const openvpn::OpenVPNWrapper::RouteRule& rule,
               const std::string& error_msg) {
            handleOpenVPNEvent(event_type, rule, error_msg);
        }
    );
    
    return true;
}

void OpenVPNRoutingProvider::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = nullptr;
    wrapper_->setRouteEventCallback(nullptr);
}

bool OpenVPNRoutingProvider::addRule(const UnifiedRouteRule& rule) {
    auto ovpn_rule = toOpenVPNRule(rule);
    return wrapper_->addRouteRule(ovpn_rule);
}

bool OpenVPNRoutingProvider::removeRule(const std::string& rule_id) {
    return wrapper_->removeRouteRule(rule_id);
}

bool OpenVPNRoutingProvider::updateRule(const std::string& rule_id, const UnifiedRouteRule& rule) {
    if (!removeRule(rule_id)) {
        return false;
    }
    return addRule(rule);
}

UnifiedRouteRule OpenVPNRoutingProvider::getRule(const std::string& rule_id) const {
    auto ovpn_rule = wrapper_->getRouteRule(rule_id);
    return fromOpenVPNRule(ovpn_rule);
}

std::vector<UnifiedRouteRule> OpenVPNRoutingProvider::getAllRules() const {
    auto ovpn_rules = wrapper_->getRouteRules();
    std::vector<UnifiedRouteRule> rules;
    rules.reserve(ovpn_rules.size());
    
    for (const auto& ovpn_rule : ovpn_rules) {
        rules.push_back(fromOpenVPNRule(ovpn_rule));
    }
    
    return rules;
}

bool OpenVPNRoutingProvider::applyRules() {
    return wrapper_->applyPreConnectionRoutes();
}

bool OpenVPNRoutingProvider::clearRoutes() {
    auto rules = wrapper_->getRouteRules();
    bool success = true;
    
    for (const auto& rule : rules) {
        if (!wrapper_->removeRouteRule(rule.id)) {
            success = false;
        }
    }
    
    return success;
}

int OpenVPNRoutingProvider::detectRoutes() {
    if (!wrapper_->detectPostConnectionRoutes()) {
        return 0;
    }
    
    return static_cast<int>(wrapper_->getRouteRules().size());
}

bool OpenVPNRoutingProvider::startMonitoring(int interval_ms) {
    return true;
}

void OpenVPNRoutingProvider::stopMonitoring() {
}

void OpenVPNRoutingProvider::setEventCallback(RouteEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = callback;
}

std::string OpenVPNRoutingProvider::exportRulesJson() const {
    json rules_array = json::array();
    auto rules = getAllRules();
    
    for (const auto& rule : rules) {
        rules_array.push_back(rule.to_json());
    }
    
    json data;
    data["routing_rules"] = rules_array;
    return data.dump(2);
}

bool OpenVPNRoutingProvider::importRulesJson(const std::string& json_str) {
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

openvpn::OpenVPNWrapper::RouteRule OpenVPNRoutingProvider::toOpenVPNRule(
    const UnifiedRouteRule& rule) const {
    
    openvpn::OpenVPNWrapper::RouteRule ovpn_rule;
    ovpn_rule.id = rule.id;
    ovpn_rule.name = rule.name;
    ovpn_rule.type = rule.type;
    ovpn_rule.destination = rule.destination;
    ovpn_rule.gateway = rule.gateway;
    ovpn_rule.source_type = rule.source_type;
    ovpn_rule.source_value = rule.source_value;
    ovpn_rule.protocol = rule.protocol;
    ovpn_rule.metric = rule.metric;
    ovpn_rule.enabled = rule.enabled;
    ovpn_rule.is_automatic = rule.is_automatic;
    ovpn_rule.description = rule.description;
    
    return ovpn_rule;
}

UnifiedRouteRule OpenVPNRoutingProvider::fromOpenVPNRule(
    const openvpn::OpenVPNWrapper::RouteRule& rule) const {
    
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

void OpenVPNRoutingProvider::handleOpenVPNEvent(
    const std::string& event_type,
    const openvpn::OpenVPNWrapper::RouteRule& rule,
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
    
    auto unified_rule = fromOpenVPNRule(rule);
    callback_copy(unified_event_type, unified_rule, error_msg);
}

}
