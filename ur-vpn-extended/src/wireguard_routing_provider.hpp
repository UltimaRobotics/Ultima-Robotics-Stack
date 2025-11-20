#ifndef WIREGUARD_ROUTING_PROVIDER_HPP
#define WIREGUARD_ROUTING_PROVIDER_HPP

#include "vpn_routing_interface.hpp"
#include "../ur-wg_library/wireguard-wrapper/include/wireguard_wrapper.hpp"
#include <map>
#include <mutex>

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
    std::string interface_name_;
    mutable std::mutex mutex_;
    
    wireguard::WireGuardWrapper::RouteRule toWireGuardRule(const UnifiedRouteRule& rule) const;
    UnifiedRouteRule fromWireGuardRule(const wireguard::WireGuardWrapper::RouteRule& rule) const;
    
    void handleWireGuardEvent(const std::string& event_type,
                             const wireguard::WireGuardWrapper::RouteRule& rule,
                             const std::string& error_msg);
};

}

#endif
