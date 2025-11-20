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

} // namespace vpn_manager
