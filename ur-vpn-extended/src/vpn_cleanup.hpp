
#ifndef VPN_CLEANUP_HPP
#define VPN_CLEANUP_HPP

#include <string>
#include <vector>

namespace vpn_manager {

class VPNCleanup {
public:
    // Detect and clean all leftover VPN interfaces
    static bool cleanupAll(bool verbose = false);
    
    // Clean specific interface type
    static bool cleanupWireGuard(bool verbose = false);
    static bool cleanupOpenVPN(bool verbose = false);
    
private:
    // Helper methods
    static std::vector<std::string> detectWireGuardInterfaces();
    static std::vector<std::string> detectOpenVPNInterfaces();
    static bool removeInterface(const std::string& interface_name, const std::string& type, bool verbose);
    static bool executeCommand(const std::string& command, bool verbose);
};

} // namespace vpn_manager

#endif // VPN_CLEANUP_HPP
