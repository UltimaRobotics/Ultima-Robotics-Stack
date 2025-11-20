
#include "vpn_cleanup.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <unistd.h>

using json = nlohmann::json;

namespace vpn_manager {

bool VPNCleanup::cleanupAll(bool verbose) {
    json log;
    log["type"] = "cleanup";
    log["message"] = "Starting auto-cleanup of leftover VPN resources";
    std::cout << log.dump() << std::endl;
    
    bool wg_success = cleanupWireGuard(verbose);
    bool ovpn_success = cleanupOpenVPN(verbose);
    
    json complete_log;
    complete_log["type"] = "cleanup";
    complete_log["message"] = "Auto-cleanup completed";
    complete_log["wireguard_cleanup"] = wg_success;
    complete_log["openvpn_cleanup"] = ovpn_success;
    std::cout << complete_log.dump() << std::endl;
    
    return wg_success && ovpn_success;
}

std::vector<std::string> VPNCleanup::detectWireGuardInterfaces() {
    std::vector<std::string> interfaces;
    
    // Use 'ip link show type wireguard' to detect WireGuard interfaces
    FILE* pipe = popen("ip link show type wireguard 2>/dev/null", "r");
    if (!pipe) {
        return interfaces;
    }
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        
        // Parse interface name from line like "5: wg0: <POINTOPOINT,NOARP,UP,LOWER_UP>"
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            size_t second_colon = line.find(':', colon_pos + 1);
            if (second_colon != std::string::npos) {
                std::string iface = line.substr(colon_pos + 1, second_colon - colon_pos - 1);
                // Trim whitespace
                size_t start = iface.find_first_not_of(" \t\n\r");
                size_t end = iface.find_last_not_of(" \t\n\r");
                if (start != std::string::npos && end != std::string::npos) {
                    iface = iface.substr(start, end - start + 1);
                    if (iface.find("wg") == 0) {  // Interface starts with "wg"
                        interfaces.push_back(iface);
                    }
                }
            }
        }
    }
    
    pclose(pipe);
    return interfaces;
}

std::vector<std::string> VPNCleanup::detectOpenVPNInterfaces() {
    std::vector<std::string> interfaces;
    
    // Use 'ip link show' to detect tun/tap interfaces
    FILE* pipe = popen("ip link show 2>/dev/null | grep -E 'tun[0-9]+:|tap[0-9]+:'", "r");
    if (!pipe) {
        return interfaces;
    }
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            size_t second_colon = line.find(':', colon_pos + 1);
            if (second_colon != std::string::npos) {
                std::string iface = line.substr(colon_pos + 1, second_colon - colon_pos - 1);
                // Trim whitespace
                size_t start = iface.find_first_not_of(" \t\n\r");
                size_t end = iface.find_last_not_of(" \t\n\r");
                if (start != std::string::npos && end != std::string::npos) {
                    iface = iface.substr(start, end - start + 1);
                    interfaces.push_back(iface);
                }
            }
        }
    }
    
    pclose(pipe);
    return interfaces;
}

bool VPNCleanup::executeCommand(const std::string& command, bool verbose) {
    if (verbose) {
        json cmd_log;
        cmd_log["type"] = "cleanup_verbose";
        cmd_log["command"] = command;
        std::cout << cmd_log.dump() << std::endl;
    }
    
    int result = system(command.c_str());
    
    if (verbose) {
        json result_log;
        result_log["type"] = "cleanup_verbose";
        result_log["command"] = command;
        result_log["result_code"] = result;
        std::cout << result_log.dump() << std::endl;
    }
    
    return result == 0;
}

bool VPNCleanup::removeInterface(const std::string& interface_name, const std::string& type, bool verbose) {
    json log;
    log["type"] = "cleanup";
    log["step"] = "REMOVING_INTERFACE";
    log["interface"] = interface_name;
    log["interface_type"] = type;
    std::cout << log.dump() << std::endl;
    
    bool success = true;
    
    // Step 1: Flush routes
    std::string route_cmd = "ip route flush dev " + interface_name + " 2>/dev/null || true";
    executeCommand(route_cmd, verbose);
    
    // Step 2: Remove DNS if it was set for this interface
    if (type == "wireguard") {
        std::string dns_cmd = "resolvconf -d " + interface_name + " 2>/dev/null || true";
        executeCommand(dns_cmd, verbose);
    }
    
    // Step 3: Bring interface down
    std::string down_cmd = "ip link set " + interface_name + " down 2>/dev/null || true";
    executeCommand(down_cmd, verbose);
    
    // Step 4: Delete interface
    std::string del_cmd = "ip link del " + interface_name + " 2>/dev/null || true";
    success = executeCommand(del_cmd, verbose);
    
    // Step 5: Verify interface is gone
    std::string verify_cmd = "ip link show " + interface_name + " 2>/dev/null";
    FILE* verify_pipe = popen(verify_cmd.c_str(), "r");
    bool still_exists = false;
    if (verify_pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), verify_pipe) != nullptr) {
            still_exists = true;
        }
        pclose(verify_pipe);
    }
    
    json result_log;
    result_log["type"] = "cleanup";
    result_log["step"] = "INTERFACE_REMOVED";
    result_log["interface"] = interface_name;
    result_log["interface_type"] = type;
    result_log["success"] = !still_exists;
    std::cout << result_log.dump() << std::endl;
    
    return !still_exists;
}

bool VPNCleanup::cleanupWireGuard(bool verbose) {
    json log;
    log["type"] = "cleanup";
    log["message"] = "Detecting leftover WireGuard interfaces";
    std::cout << log.dump() << std::endl;
    
    auto interfaces = detectWireGuardInterfaces();
    
    if (interfaces.empty()) {
        json no_interfaces_log;
        no_interfaces_log["type"] = "cleanup";
        no_interfaces_log["message"] = "No leftover WireGuard interfaces detected";
        std::cout << no_interfaces_log.dump() << std::endl;
        return true;
    }
    
    json found_log;
    found_log["type"] = "cleanup";
    found_log["message"] = "Found leftover WireGuard interfaces";
    found_log["count"] = interfaces.size();
    found_log["interfaces"] = interfaces;
    std::cout << found_log.dump() << std::endl;
    
    bool all_success = true;
    for (const auto& iface : interfaces) {
        bool removed = removeInterface(iface, "wireguard", verbose);
        if (!removed) {
            all_success = false;
        }
        
        // Small delay between cleanup operations
        usleep(200000); // 200ms
    }
    
    return all_success;
}

bool VPNCleanup::cleanupOpenVPN(bool verbose) {
    json log;
    log["type"] = "cleanup";
    log["message"] = "Detecting leftover OpenVPN interfaces";
    std::cout << log.dump() << std::endl;
    
    auto interfaces = detectOpenVPNInterfaces();
    
    if (interfaces.empty()) {
        json no_interfaces_log;
        no_interfaces_log["type"] = "cleanup";
        no_interfaces_log["message"] = "No leftover OpenVPN interfaces detected";
        std::cout << no_interfaces_log.dump() << std::endl;
        return true;
    }
    
    json found_log;
    found_log["type"] = "cleanup";
    found_log["message"] = "Found leftover OpenVPN interfaces";
    found_log["count"] = interfaces.size();
    found_log["interfaces"] = interfaces;
    std::cout << found_log.dump() << std::endl;
    
    bool all_success = true;
    for (const auto& iface : interfaces) {
        bool removed = removeInterface(iface, "openvpn", verbose);
        if (!removed) {
            all_success = false;
        }
        
        // Small delay between cleanup operations
        usleep(200000); // 200ms
    }
    
    return all_success;
}

} // namespace vpn_manager
