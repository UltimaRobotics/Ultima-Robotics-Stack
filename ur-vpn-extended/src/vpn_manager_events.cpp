#include "internal/vpn_manager_events.hpp"
#include "vpn_instance_manager.hpp"
#include <iostream>

namespace vpn_manager {

void VPNInstanceManager::setGlobalEventCallback(EventCallback callback) {
    global_event_callback_ = callback;
}

void VPNInstanceManager::emitEvent(const std::string& instance_name,
                                     const std::string& event_type,
                                     const std::string& message,
                                     const json& data) {
    AggregatedEvent event;
    event.instance_name = instance_name;
    event.event_type = event_type;
    event.message = message;
    event.data = data;
    event.timestamp = time(nullptr);

    if (global_event_callback_) {
        global_event_callback_(event);
    }
}

void VPNInstanceManager::setVerbose(bool verbose) {
    verbose_ = verbose;
}

bool VPNInstanceManager::isVerbose() const {
    return verbose_;
}

// Stats logging control methods
void VPNInstanceManager::setStatsLoggingEnabled(bool enabled) {
    stats_logging_enabled_ = enabled;
}

bool VPNInstanceManager::isStatsLoggingEnabled() const {
    return stats_logging_enabled_;
}

void VPNInstanceManager::setOpenVPNStatsLogging(bool enabled) {
    openvpn_stats_logging_ = enabled;
}

bool VPNInstanceManager::isOpenVPNStatsLoggingEnabled() const {
    return openvpn_stats_logging_;
}

void VPNInstanceManager::setWireGuardStatsLogging(bool enabled) {
    wireguard_stats_logging_ = enabled;
}

bool VPNInstanceManager::isWireGuardStatsLoggingEnabled() const {
    return wireguard_stats_logging_;
}

} // namespace vpn_manager
