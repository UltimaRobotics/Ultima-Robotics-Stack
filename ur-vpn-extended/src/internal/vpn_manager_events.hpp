#ifndef VPN_MANAGER_EVENTS_HPP
#define VPN_MANAGER_EVENTS_HPP

#include "../vpn_instance_manager.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vpn_manager {

class VPNInstanceManager;

namespace EventManager {
    void emitEvent(VPNInstanceManager* manager,
                   const std::string& instance_name,
                   const std::string& event_type,
                   const std::string& message,
                   const json& data = json::object());
    
    void setGlobalEventCallback(VPNInstanceManager* manager, EventCallback callback);
}

} // namespace vpn_manager

#endif // VPN_MANAGER_EVENTS_HPP
