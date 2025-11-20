#include "vpn_instance_manager.hpp"
#include "internal/vpn_manager_utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <array>
#include <memory>
#include <map>
#include <functional>

namespace vpn_manager {

VPNType VPNManagerUtils::parseVPNType(const std::string& type_str) {
    std::string lower = type_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "openvpn") return VPNType::OPENVPN;
    if (lower == "wireguard" || lower == "wireguard") return VPNType::WIREGUARD;
    return VPNType::UNKNOWN;
}

std::string VPNManagerUtils::vpnTypeToString(VPNType type) {
    switch (type) {
        case VPNType::OPENVPN: return "openvpn";
        case VPNType::WIREGUARD: return "wireguard";
        default: return "unknown";
    }
}

std::string VPNManagerUtils::formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return ss.str();
}

std::string VPNManagerUtils::formatTime(uint64_t seconds) {
    uint64_t hours = seconds / 3600;
    uint64_t minutes = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;

    std::stringstream ss;
    ss << hours << "h " << minutes << "m " << secs << "s";
    return ss.str();
}

std::string VPNManagerUtils::executeCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    
    if (!pipe) {
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

std::string VPNManagerUtils::getCidrFromNetmask(const std::string& netmask) {
    std::map<std::string, std::string> netmask_map = {
        {"255.255.255.255", "32"},
        {"255.255.255.254", "31"},
        {"255.255.255.252", "30"},
        {"255.255.255.248", "29"},
        {"255.255.255.240", "28"},
        {"255.255.255.224", "27"},
        {"255.255.255.192", "26"},
        {"255.255.255.128", "25"},
        {"255.255.255.0", "24"},
        {"255.255.254.0", "23"},
        {"255.255.252.0", "22"},
        {"255.255.248.0", "21"},
        {"255.255.240.0", "20"},
        {"255.255.224.0", "19"},
        {"255.255.192.0", "18"},
        {"255.255.128.0", "17"},
        {"255.255.0.0", "16"},
        {"255.254.0.0", "15"},
        {"255.252.0.0", "14"},
        {"255.248.0.0", "13"},
        {"255.240.0.0", "12"},
        {"255.224.0.0", "11"},
        {"255.192.0.0", "10"},
        {"255.128.0.0", "9"},
        {"255.0.0.0", "8"},
        {"254.0.0.0", "7"},
        {"252.0.0.0", "6"},
        {"248.0.0.0", "5"},
        {"240.0.0.0", "4"},
        {"224.0.0.0", "3"},
        {"192.0.0.0", "2"},
        {"128.0.0.0", "1"},
        {"0.0.0.0", "0"}
    };
    
    auto it = netmask_map.find(netmask);
    if (it != netmask_map.end()) {
        return it->second;
    }
    
    return "24";
}

std::string VPNManagerUtils::hashString(const std::string& str) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(str));
}

} // namespace vpn_manager
