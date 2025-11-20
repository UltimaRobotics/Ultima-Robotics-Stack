#include "vpn_instance_manager.hpp"
#include "internal/vpn_manager_utils.hpp"
#include <iostream>

namespace vpn_manager {

json VPNInstanceManager::getInstanceStatus(const std::string& instance_name) {
    std::lock_guard<std::mutex> lock(instances_mutex_);

    auto it = instances_.find(instance_name);
    if (it == instances_.end()) {
        return {{"error", "Instance not found"}};
    }

    json status;
    status["name"] = it->second.name;
    status["type"] = VPNManagerUtils::vpnTypeToString(it->second.type);
    status["enabled"] = it->second.enabled;
    status["state"] = static_cast<int>(it->second.current_state);
    status["auto_connect"] = it->second.auto_connect;
    status["server"] = it->second.server;
    status["port"] = it->second.port;
    status["status"] = it->second.status;
    status["connection_stats"] = it->second.connection_stats;

    if (it->second.start_time > 0) {
        status["uptime"] = time(nullptr) - it->second.start_time;
    }

    return status;
}

json VPNInstanceManager::getAllInstancesStatus() {
    std::lock_guard<std::mutex> lock(instances_mutex_);

    json all_status = json::array();

    for (const auto& [name, inst] : instances_) {
        json status;
        status["name"] = inst.name;
        status["type"] = VPNManagerUtils::vpnTypeToString(inst.type);
        status["enabled"] = inst.enabled;
        status["state"] = static_cast<int>(inst.current_state);
        status["server"] = inst.server;
        status["port"] = inst.port;
        status["status"] = inst.status;
        status["connection_stats"] = inst.connection_stats;

        if (inst.start_time > 0) {
            status["uptime"] = time(nullptr) - inst.start_time;
        }

        all_status.push_back(status);
    }

    return all_status;
}

json VPNInstanceManager::getAggregatedStats() {
    json stats;
    stats["total_instances"] = instances_.size();
    stats["timestamp"] = time(nullptr);
    return stats;
}

std::string VPNInstanceManager::formatBytes(uint64_t bytes) {
    return VPNManagerUtils::formatBytes(bytes);
}

std::string VPNInstanceManager::formatTime(uint64_t seconds) {
    return VPNManagerUtils::formatTime(seconds);
}

VPNType VPNInstanceManager::parseVPNType(const std::string& type_str) {
    return VPNManagerUtils::parseVPNType(type_str);
}

std::string VPNInstanceManager::vpnTypeToString(VPNType type) {
    return VPNManagerUtils::vpnTypeToString(type);
}

std::string VPNInstanceManager::executeCommand(const std::string& cmd) {
    return VPNManagerUtils::executeCommand(cmd);
}

std::string VPNInstanceManager::getCidrFromNetmask(const std::string& netmask) {
    return VPNManagerUtils::getCidrFromNetmask(netmask);
}

} // namespace vpn_manager
