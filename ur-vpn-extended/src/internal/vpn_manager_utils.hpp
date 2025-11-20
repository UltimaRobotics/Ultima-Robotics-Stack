#ifndef VPN_MANAGER_UTILS_HPP
#define VPN_MANAGER_UTILS_HPP

#include <string>
#include <cstdint>

namespace vpn_manager {

// Forward declaration
enum class VPNType;

class VPNManagerUtils {
public:
    static std::string formatBytes(uint64_t bytes);
    static std::string formatTime(uint64_t seconds);
    static VPNType parseVPNType(const std::string& type_str);
    static std::string vpnTypeToString(VPNType type);
    static std::string executeCommand(const std::string& cmd);
    static std::string getCidrFromNetmask(const std::string& netmask);
    static std::string hashString(const std::string& str);
};

} // namespace vpn_manager

#endif // VPN_MANAGER_UTILS_HPP
