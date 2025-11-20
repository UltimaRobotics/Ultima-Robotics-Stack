
#include "interface_controller.h"
#include "command_logger.h"
#include "smart_routing.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cctype>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>

InterfaceController::InterfaceController()
    : m_dns_modified(false)
{
}

InterfaceController::~InterfaceController() {
    if (m_dns_modified) {
        restoreDNS();
    }
}

bool InterfaceController::configureInterface(const InterfaceConfig& config) {
    if (!validateConfiguration(config)) {
        std::cerr << "Invalid interface configuration" << std::endl;
        return false;
    }
    
    m_current_interface = config.interface_name;
    
    // Bring interface down first
    if (!bringInterfaceDown(config.interface_name)) {
        std::cerr << "Failed to bring interface down" << std::endl;
        return false;
    }
    
    // Enforce raw IP requirement after interface is down but before bringing it up
    if (!enforceRawIPRequirement(config.interface_name)) {
        std::cerr << "Failed to enforce raw IP requirement for interface " << config.interface_name << std::endl;
        return false;
    }
    
    // Configure IP based on type
    bool ip_configured = false;
    if (config.use_dhcp) {
        ip_configured = startDHCP(config.interface_name);
    } else {
        ip_configured = setStaticIP(config.interface_name, config.ip_address, 
                                   config.subnet_mask, config.gateway);
    }
    
    if (!ip_configured) {
        std::cerr << "Failed to configure IP for interface " << config.interface_name << std::endl;
        return false;
    }
    
    // Set MTU if specified
    if (config.mtu > 0) {
        std::string mtu_cmd = "ip link set dev " + config.interface_name + 
                             " mtu " + std::to_string(config.mtu);
        if (!executeCommandSuccess(mtu_cmd)) {
            std::cerr << "Failed to set MTU" << std::endl;
        }
    }
    
    // Bring interface up
    if (!bringInterfaceUp(config.interface_name)) {
        std::cerr << "Failed to bring interface up" << std::endl;
        return false;
    }
    
    // Configure DNS
    if (!config.dns_primary.empty()) {
        setDNS(config.dns_primary, config.dns_secondary);
    }
    
    // Add default route
    if (!config.gateway.empty()) {
        addDefaultRoute(config.gateway, config.interface_name);
    }
    
    std::cout << "Interface " << config.interface_name << " configured successfully" << std::endl;
    return true;
}

bool InterfaceController::bringInterfaceUp(const std::string& interface_name) {
    std::string cmd = "ip link set dev " + interface_name + " up";
    bool result = executeCommandSuccess(cmd);
    
    if (result) {
        std::cout << "Interface " << interface_name << " brought up" << std::endl;
    } else {
        std::cerr << "Failed to bring up interface " << interface_name << std::endl;
    }
    
    return result;
}

bool InterfaceController::bringInterfaceDown(const std::string& interface_name) {
    std::string cmd = "ip link set dev " + interface_name + " down";
    bool result = executeCommandSuccess(cmd);
    
    if (result) {
        std::cout << "Interface " << interface_name << " brought down" << std::endl;
    }
    
    return result;
}

bool InterfaceController::resetInterface(const std::string& interface_name) {
    // Flush IP addresses
    std::string flush_cmd = "ip addr flush dev " + interface_name;
    executeCommandSuccess(flush_cmd);
    
    // Bring down and up
    bringInterfaceDown(interface_name);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return bringInterfaceUp(interface_name);
}

bool InterfaceController::setStaticIP(const std::string& interface_name,
                                     const std::string& ip_address,
                                     const std::string& subnet_mask,
                                     const std::string& gateway) {
    // Calculate CIDR notation
    int cidr = 24; // Default
    if (!subnet_mask.empty()) {
        struct in_addr addr;
        if (inet_aton(subnet_mask.c_str(), &addr)) {
            uint32_t mask = ntohl(addr.s_addr);
            cidr = __builtin_popcount(mask);
        }
    }
    
    // Set IP address
    std::string ip_cmd = "ip addr add " + ip_address + "/" + std::to_string(cidr) + 
                        " dev " + interface_name;
    
    if (!executeCommandSuccess(ip_cmd)) {
        std::cerr << "Failed to set IP address" << std::endl;
        return false;
    }
    
    std::cout << "Static IP " << ip_address << " configured on " << interface_name << std::endl;
    return true;
}

bool InterfaceController::startDHCP(const std::string& interface_name) {
    // Kill any existing DHCP client for this interface
    stopDHCP(interface_name);
    
    // Start DHCP client
    std::string dhcp_cmd = "dhclient -v " + interface_name + " &";
    
    if (!executeCommandSuccess(dhcp_cmd)) {
        std::cerr << "Failed to start DHCP client" << std::endl;
        return false;
    }
    
    // Wait for IP assignment
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::string ip = parseInterfaceIP(interface_name);
        if (!ip.empty() && ip != "127.0.0.1") {
            std::cout << "DHCP assigned IP " << ip << " to " << interface_name << std::endl;
            return true;
        }
    }
    
    std::cerr << "DHCP timeout for interface " << interface_name << std::endl;
    return false;
}

bool InterfaceController::stopDHCP(const std::string& interface_name) {
    std::string kill_cmd = "pkill -f 'dhclient.*" + interface_name + "'";
    executeCommandSuccess(kill_cmd);
    
    // Release IP
    std::string release_cmd = "dhclient -r " + interface_name;
    executeCommandSuccess(release_cmd);
    
    return true;
}

bool InterfaceController::renewDHCP(const std::string& interface_name) {
    std::string renew_cmd = "dhclient -r " + interface_name + " && dhclient " + interface_name;
    return executeCommandSuccess(renew_cmd);
}

bool InterfaceController::setDNS(const std::string& primary, const std::string& secondary) {
    // Backup original DNS configuration
    if (!m_dns_modified) {
        std::ifstream resolv_conf("/etc/resolv.conf");
        if (resolv_conf.is_open()) {
            std::string line;
            while (std::getline(resolv_conf, line)) {
                m_backup_dns_config += line + "\n";
            }
            resolv_conf.close();
        }
        m_dns_modified = true;
    }
    
    // Write new DNS configuration
    std::ofstream resolv_conf("/etc/resolv.conf");
    if (!resolv_conf.is_open()) {
        std::cerr << "Failed to open /etc/resolv.conf for writing" << std::endl;
        return false;
    }
    
    resolv_conf << "nameserver " << primary << std::endl;
    if (!secondary.empty()) {
        resolv_conf << "nameserver " << secondary << std::endl;
    }
    resolv_conf.close();
    
    std::cout << "DNS configured: " << primary;
    if (!secondary.empty()) {
        std::cout << ", " << secondary;
    }
    std::cout << std::endl;
    
    return true;
}

bool InterfaceController::restoreDNS() {
    if (!m_dns_modified || m_backup_dns_config.empty()) {
        return true;
    }
    
    std::ofstream resolv_conf("/etc/resolv.conf");
    if (!resolv_conf.is_open()) {
        return false;
    }
    
    resolv_conf << m_backup_dns_config;
    resolv_conf.close();
    
    m_dns_modified = false;
    m_backup_dns_config.clear();
    
    std::cout << "DNS configuration restored" << std::endl;
    return true;
}

bool InterfaceController::addDefaultRoute(const std::string& gateway, const std::string& interface_name) {
    std::string route_cmd = "ip route add default via " + gateway + " dev " + interface_name;
    bool result = executeCommandSuccess(route_cmd);
    
    if (result) {
        std::cout << "Default route added via " << gateway << std::endl;
    }
    
    return result;
}

bool InterfaceController::removeDefaultRoute(const std::string& gateway) {
    std::string route_cmd = "ip route del default via " + gateway;
    return executeCommandSuccess(route_cmd);
}

bool InterfaceController::addRoute(const std::string& destination, const std::string& gateway, 
                                  const std::string& interface_name) {
    std::string route_cmd = "ip route add " + destination + " via " + gateway + " dev " + interface_name;
    return executeCommandSuccess(route_cmd);
}

bool InterfaceController::removeRoute(const std::string& destination, const std::string& gateway) {
    std::string route_cmd = "ip route del " + destination + " via " + gateway;
    return executeCommandSuccess(route_cmd);
}

std::vector<std::string> InterfaceController::findWWANInterfaces() {
    std::vector<std::string> wwan_interfaces;
    
    std::string output = executeCommand("ls -1 /sys/class/net/");
    std::istringstream iss(output);
    std::string interface;
    
    while (std::getline(iss, interface)) {
        // Check if it's a WWAN interface
        if (interface.find("wwan") != std::string::npos ||
            interface.find("wwp") != std::string::npos ||
            interface.find("usb") != std::string::npos) {
            wwan_interfaces.push_back(interface);
        }
    }
    
    return wwan_interfaces;
}

std::string InterfaceController::findInterfaceForDevice(const std::string& device_path) {
    // Extract device identifier
    std::regex device_regex("cdc-wdm(\\d+)");
    std::smatch match;
    
    if (std::regex_search(device_path, match, device_regex)) {
        std::string device_num = match[1].str();
        
        // Common WWAN interface patterns
        std::vector<std::string> patterns = {
            "wwan" + device_num,
            "wwp0s20f0u" + device_num,
            "usb" + device_num
        };
        
        for (const auto& pattern : patterns) {
            std::string check_cmd = "ip link show " + pattern + " 2>/dev/null";
            if (!executeCommand(check_cmd).empty()) {
                return pattern;
            }
        }
    }
    
    // Fallback: find any WWAN interface
    auto wwan_interfaces = findWWANInterfaces();
    return wwan_interfaces.empty() ? "" : wwan_interfaces[0];
}

InterfaceStatus InterfaceController::getInterfaceStatus(const std::string& interface_name) {
    InterfaceStatus status;
    status.name = interface_name;
    status.is_up = false;
    status.has_ip = false;
    status.mtu = 1500;
    status.bytes_sent = 0;
    status.bytes_received = 0;
    
    // Check if interface exists and is up
    std::string link_output = executeCommand("ip link show " + interface_name + " 2>/dev/null");
    if (!link_output.empty()) {
        status.is_up = link_output.find("state UP") != std::string::npos;
        
        // Extract MAC address
        std::regex mac_regex("link/ether ([a-fA-F0-9:]{17})");
        std::smatch mac_match;
        if (std::regex_search(link_output, mac_match, mac_regex)) {
            status.mac_address = mac_match[1].str();
        }
        
        // Extract MTU
        std::regex mtu_regex("mtu (\\d+)");
        std::smatch mtu_match;
        if (std::regex_search(link_output, mtu_match, mtu_regex)) {
            status.mtu = std::stoul(mtu_match[1].str());
        }
    }
    
    // Get IP address
    status.ip_address = parseInterfaceIP(interface_name);
    status.has_ip = !status.ip_address.empty() && status.ip_address != "127.0.0.1";
    
    // Get statistics
    parseInterfaceStats(interface_name, status.bytes_sent, status.bytes_received);
    
    return status;
}

std::vector<InterfaceStatus> InterfaceController::getAllInterfaces() {
    std::vector<InterfaceStatus> interfaces;
    
    std::string output = executeCommand("ip link show");
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        std::regex interface_regex("^\\d+: ([^:@]+)[@:]");
        std::smatch match;
        
        if (std::regex_search(line, match, interface_regex)) {
            std::string interface_name = match[1].str();
            if (interface_name != "lo") { // Skip loopback
                interfaces.push_back(getInterfaceStatus(interface_name));
            }
        }
    }
    
    return interfaces;
}

bool InterfaceController::validateConfiguration(const InterfaceConfig& config) {
    if (config.interface_name.empty()) {
        return false;
    }
    
    if (!config.use_dhcp) {
        if (config.ip_address.empty()) {
            return false;
        }
        
        // Validate IP address format
        struct in_addr addr;
        if (inet_aton(config.ip_address.c_str(), &addr) == 0) {
            return false;
        }
    }
    
    return true;
}

bool InterfaceController::testConnectivity(const std::string& target) {
    std::string ping_cmd = "ping -c 1 -W 5 " + target + " >/dev/null 2>&1";
    return executeCommandSuccess(ping_cmd);
}

bool InterfaceController::pingGateway(const std::string& gateway) {
    return testConnectivity(gateway);
}

std::string InterfaceController::executeCommand(const std::string& command) {
    CommandLogger::logCommand(command);
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        CommandLogger::logCommandResult(command, "", -1);
        return "";
    }
    
    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int exit_code = pclose(pipe);
    CommandLogger::logCommandResult(command, result, WEXITSTATUS(exit_code));
    return result;
}

bool InterfaceController::executeCommandSuccess(const std::string& command) {
    CommandLogger::logCommand(command);
    
    int result = system(command.c_str());
    int exit_code = WEXITSTATUS(result);
    bool success = (exit_code == 0);
    
    std::string result_text = success ? "SUCCESS" : "FAILED";
    CommandLogger::logCommandResult(command, result_text, exit_code);
    
    return success;
}

bool InterfaceController::verifyInterfaceCleanup(const std::string& interface_name) {
    std::cout << "Verifying cleanup for interface: " << interface_name << std::endl;
    
    // Check if interface is down
    bool is_active = isInterfaceActive(interface_name);
    if (is_active) {
        std::cout << "  ✗ Interface is still active" << std::endl;
        return false;
    }
    
    // Check if interface has no IP addresses
    std::string ip = parseInterfaceIP(interface_name);
    if (!ip.empty()) {
        std::cout << "  ✗ Interface still has IP address: " << ip << std::endl;
        return false;
    }
    
    // Check if interface has no routes
    std::string route_check_cmd = "ip route show dev " + interface_name + " 2>/dev/null";
    std::string routes = executeCommand(route_check_cmd);
    if (!routes.empty()) {
        std::cout << "  ✗ Interface still has routes: " << routes << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Interface cleanup verified successfully" << std::endl;
    return true;
}

int InterfaceController::countActiveWWANInterfaces() {
    std::vector<std::string> interfaces = getExistingWWANInterfaces();
    int active_count = 0;
    
    for (const auto& interface : interfaces) {
        if (isInterfaceActive(interface)) {
            active_count++;
        }
    }
    
    return active_count;
}

bool InterfaceController::forceCleanupInterface(const std::string& interface_name) {
    std::cout << "Force cleaning interface: " << interface_name << std::endl;
    
    bool success = true;
    
    // Force stop any DHCP clients
    std::string kill_dhcp = "pkill -9 -f 'dhclient.*" + interface_name + "' 2>/dev/null";
    executeCommand(kill_dhcp);
    
    // Force flush all addresses
    std::string flush_addr = "ip addr flush dev " + interface_name + " 2>/dev/null";
    if (!executeCommandSuccess(flush_addr)) {
        std::cout << "  Warning: Could not flush addresses" << std::endl;
        success = false;
    }
    
    // Force flush all routes
    std::string flush_routes = "ip route flush dev " + interface_name + " 2>/dev/null";
    if (!executeCommandSuccess(flush_routes)) {
        std::cout << "  Warning: Could not flush routes" << std::endl;
        success = false;
    }
    
    // Force bring interface down
    std::string down_cmd = "ip link set dev " + interface_name + " down 2>/dev/null";
    if (!executeCommandSuccess(down_cmd)) {
        std::cout << "  Warning: Could not bring interface down" << std::endl;
        success = false;
    }
    
    // Additional cleanup: remove any default routes through this interface
    std::string remove_default = "ip route del default dev " + interface_name + " 2>/dev/null";
    executeCommand(remove_default);  // Don't check success as it may not exist
    
    return success;
}

std::string InterfaceController::parseInterfaceIP(const std::string& interface_name) {
    std::string cmd = "ip addr show " + interface_name + " 2>/dev/null";
    std::string output = executeCommand(cmd);
    
    std::regex ip_regex("inet ([0-9.]+)/");
    std::smatch match;
    
    if (std::regex_search(output, match, ip_regex)) {
        return match[1].str();
    }
    
    return "";
}

std::string InterfaceController::parseInterfaceMAC(const std::string& interface_name) {
    std::string cmd = "ip link show " + interface_name + " 2>/dev/null";
    std::string output = executeCommand(cmd);
    
    std::regex mac_regex("link/ether ([a-fA-F0-9:]{17})");
    std::smatch match;
    
    if (std::regex_search(output, match, mac_regex)) {
        return match[1].str();
    }
    
    return "";
}

bool InterfaceController::parseInterfaceStats(const std::string& interface_name,
                                             uint64_t& bytes_sent, uint64_t& bytes_received) {
    std::string rx_path = "/sys/class/net/" + interface_name + "/statistics/rx_bytes";
    std::string tx_path = "/sys/class/net/" + interface_name + "/statistics/tx_bytes";
    
    std::ifstream rx_file(rx_path);
    std::ifstream tx_file(tx_path);
    
    if (rx_file.is_open()) {
        rx_file >> bytes_received;
        rx_file.close();
    }
    
    if (tx_file.is_open()) {
        tx_file >> bytes_sent;
        tx_file.close();
    }
    
    return rx_file.good() && tx_file.good();
}

// Smart routing integration methods
bool InterfaceController::applyCellularRouting(const std::string& interface_name, 
                                              const std::string& gateway_ip,
                                              const std::string& local_ip) {
    std::cout << "Applying smart routing for cellular interface: " << interface_name << std::endl;
    
    // Use the global smart routing manager to apply cellular routing
    return g_smart_routing.applyCellularRouting(interface_name, gateway_ip, local_ip);
}

bool InterfaceController::removeCellularRouting(const std::string& interface_name) {
    std::cout << "Removing smart routing for cellular interface: " << interface_name << std::endl;
    
    // Use the global smart routing manager to remove cellular routing
    return g_smart_routing.removeCellularRouting(interface_name);
}

// QMI Raw IP configuration methods
bool InterfaceController::verifyAndSetRawIP(const std::string& interface_name) {
    std::cout << "Verifying and setting raw IP mode for interface: " << interface_name << std::endl;
    
    // Check current raw IP status
    if (getRawIPStatus(interface_name)) {
        std::cout << "Raw IP mode already enabled for " << interface_name << std::endl;
        return true;
    }
    
    std::cout << "Raw IP mode is not enabled, attempting to enable it..." << std::endl;
    
    // Set raw IP mode to enabled with retry logic
    bool success = setRawIPMode(interface_name, true);
    
    if (!success) {
        std::cerr << "CRITICAL: Failed to enable raw IP mode for " << interface_name << std::endl;
        std::cerr << "Connection cannot proceed without raw IP mode enabled" << std::endl;
        return false;
    }
    
    // Final verification after setting
    if (!getRawIPStatus(interface_name)) {
        std::cerr << "CRITICAL: Raw IP mode verification failed after setting" << std::endl;
        return false;
    }
    
    std::cout << "Raw IP mode successfully enabled and verified for " << interface_name << std::endl;
    return true;
}

bool InterfaceController::getRawIPStatus(const std::string& interface_name) {
    std::string raw_ip_path = "/sys/class/net/" + interface_name + "/qmi/raw_ip";
    std::ifstream file(raw_ip_path);
    
    if (!file.is_open()) {
        std::cerr << "Warning: Cannot access raw_ip status at " << raw_ip_path << std::endl;
        return false;
    }
    
    std::string status;
    std::getline(file, status);
    file.close();
    
    // Remove any whitespace
    status.erase(std::remove_if(status.begin(), status.end(), ::isspace), status.end());
    
    bool is_enabled = (status == "Y" || status == "y" || status == "1");
    std::cout << "Raw IP status for " << interface_name << ": " << status 
              << " (enabled: " << (is_enabled ? "yes" : "no") << ")" << std::endl;
    
    return is_enabled;
}

bool InterfaceController::setRawIPMode(const std::string& interface_name, bool enable) {
    std::string raw_ip_path = "/sys/class/net/" + interface_name + "/qmi/raw_ip";
    
    // Check if the path exists
    std::ifstream test_file(raw_ip_path);
    if (!test_file.is_open()) {
        std::cerr << "Error: Raw IP control path not found: " << raw_ip_path << std::endl;
        return false;
    }
    test_file.close();
    
    std::string value = enable ? "Y" : "N";
    std::cout << "Setting raw IP mode to " << value << " for " << interface_name << std::endl;
    
    // First attempt: try to set raw_ip directly
    std::string cmd = "echo '" + value + "' | sudo tee " + raw_ip_path + " > /dev/null 2>&1";
    bool result = executeCommandSuccess(cmd);
    
    if (!result) {
        std::cout << "Direct setting failed, trying with interface down/up cycle..." << std::endl;
        
        // Save current interface state
        bool was_up = isInterfaceActive(interface_name);
        
        // Bring interface down to release resources
        if (was_up) {
            std::cout << "Bringing interface " << interface_name << " down to change raw_ip mode..." << std::endl;
            bringInterfaceDown(interface_name);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Try setting raw_ip again
        result = executeCommandSuccess(cmd);
        
        if (!result) {
            // Final attempt with additional delay
            std::cout << "Retrying with additional delay..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            result = executeCommandSuccess(cmd);
        }
        
        // Restore interface state if it was up
        if (was_up && result) {
            std::cout << "Bringing interface " << interface_name << " back up..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            bringInterfaceUp(interface_name);
        }
    }
    
    if (result) {
        std::cout << "Successfully set raw IP mode for " << interface_name << std::endl;
        
        // Verify the change
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        bool verification = getRawIPStatus(interface_name);
        if (verification == enable) {
            std::cout << "Raw IP mode change verified successfully" << std::endl;
        } else {
            std::cerr << "Warning: Raw IP mode change could not be verified" << std::endl;
        }
    } else {
        std::cerr << "Failed to set raw IP mode for " << interface_name 
                  << " (device may be busy or requires manual intervention)" << std::endl;
    }
    
    return result;
}

// Hot-disconnect cleanup support methods
std::vector<std::string> InterfaceController::getActiveInterfaces() {
    std::vector<std::string> interfaces;
    
    std::string cmd = "ip link show | grep '^[0-9]' | cut -d: -f2 | tr -d ' '";
    std::string output = executeCommand(cmd);
    
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.find("wwan") != std::string::npos) {
            interfaces.push_back(line);
        }
    }
    
    return interfaces;
}

std::vector<std::string> InterfaceController::getActiveRoutes() {
    std::vector<std::string> routes;
    
    std::string cmd = "ip route show";
    std::string output = executeCommand(cmd);
    
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        // Look for routes involving wwan interfaces
        if (!line.empty() && line.find("wwan") != std::string::npos) {
            routes.push_back(line);
        }
    }
    
    return routes;
}

bool InterfaceController::cleanupInterface(const std::string& interface_name) {
    std::cout << "Cleaning up interface: " << interface_name << std::endl;
    
    bool success = true;
    
    // Stop DHCP if running
    success &= stopDHCP(interface_name);
    
    // Remove any routes using this interface
    std::string route_cmd = "ip route flush dev " + interface_name;
    if (!executeCommandSuccess(route_cmd)) {
        std::cerr << "Warning: Failed to flush routes for " << interface_name << std::endl;
        success = false;
    }
    
    // Flush IP addresses
    std::string flush_cmd = "ip addr flush dev " + interface_name;
    if (!executeCommandSuccess(flush_cmd)) {
        std::cerr << "Warning: Failed to flush addresses for " << interface_name << std::endl;
        success = false;
    }
    
    // Bring interface down
    success &= bringInterfaceDown(interface_name);
    
    if (success) {
        std::cout << "Successfully cleaned up interface: " << interface_name << std::endl;
    } else {
        std::cerr << "Partial cleanup completed for interface: " << interface_name << std::endl;
    }
    
    return success;
}

bool InterfaceController::cleanupAllRoutes() {
    std::cout << "Cleaning up all cellular/WWAN routes..." << std::endl;
    
    std::vector<std::string> routes = getActiveRoutes();
    bool success = true;
    
    for (const auto& route : routes) {
        // Parse route and remove it
        std::cout << "Removing route: " << route << std::endl;
        
        // Extract key information from route to construct removal command
        std::istringstream iss(route);
        std::string destination, via, gateway, dev, interface;
        
        // Parse the route string (format: destination via gateway dev interface)
        iss >> destination;
        if (iss >> via && via == "via") {
            iss >> gateway >> dev >> interface;
            
            std::string remove_cmd = "ip route del " + destination + " via " + gateway + " dev " + interface;
            if (!executeCommandSuccess(remove_cmd)) {
                std::cerr << "Warning: Failed to remove route: " << route << std::endl;
                success = false;
            }
        }
    }
    
    return success;
}

// Interface name management methods
std::vector<std::string> InterfaceController::getExistingWWANInterfaces() {
    std::vector<std::string> existing_interfaces;
    
    // Get all network interfaces
    std::string cmd = "ls /sys/class/net/";
    std::string output = executeCommand(cmd);
    
    std::istringstream iss(output);
    std::string interface;
    while (iss >> interface) {
        // Check if it's a WWAN interface (starts with 'wwan')
        if (interface.find("wwan") == 0) {
            // Verify it's actually a network interface
            std::string interface_path = "/sys/class/net/" + interface;
            std::string test_cmd = "test -d " + interface_path;
            if (executeCommandSuccess(test_cmd)) {
                existing_interfaces.push_back(interface);
            }
        }
    }
    
    // Sort the interfaces for consistent ordering
    std::sort(existing_interfaces.begin(), existing_interfaces.end());
    
    std::cout << "Found " << existing_interfaces.size() << " existing WWAN interfaces" << std::endl;
    for (const auto& iface : existing_interfaces) {
        std::cout << "  - " << iface << std::endl;
    }
    
    return existing_interfaces;
}

std::string InterfaceController::generateUniqueWWANName(const std::string& base_name) {
    std::vector<int> used_numbers = getUsedWWANNumbers();
    
    // Find the smallest available number
    int next_number = 0;
    while (std::find(used_numbers.begin(), used_numbers.end(), next_number) != used_numbers.end()) {
        next_number++;
    }
    
    std::string unique_name = base_name + std::to_string(next_number);
    
    std::cout << "Generated unique WWAN interface name: " << unique_name << std::endl;
    return unique_name;
}

bool InterfaceController::isInterfaceNameTaken(const std::string& interface_name) {
    std::string interface_path = "/sys/class/net/" + interface_name;
    std::string test_cmd = "test -e " + interface_path;
    
    bool exists = executeCommandSuccess(test_cmd);
    if (exists) {
        std::cout << "Interface name '" << interface_name << "' is already taken" << std::endl;
    }
    
    return exists;
}

std::vector<int> InterfaceController::getUsedWWANNumbers() {
    std::vector<int> used_numbers;
    std::vector<std::string> existing_interfaces = getExistingWWANInterfaces();
    
    for (const auto& interface_name : existing_interfaces) {
        // Extract number from interface name (e.g., "wwan0" -> 0, "wwan123" -> 123)
        if (interface_name.find("wwan") == 0) {
            std::string number_str = interface_name.substr(4);  // Skip "wwan"
            if (!number_str.empty() && std::all_of(number_str.begin(), number_str.end(), ::isdigit)) {
                int number = std::stoi(number_str);
                used_numbers.push_back(number);
            }
        }
    }
    
    std::sort(used_numbers.begin(), used_numbers.end());
    return used_numbers;
}

// Smart cleanup and interface availability methods
bool InterfaceController::isInterfaceActive(const std::string& interface_name) {
    // Check if interface exists and has UP flag
    std::string status_cmd = "ip link show " + interface_name + " 2>/dev/null";
    std::string output = executeCommand(status_cmd);
    
    if (output.empty() || output.find("does not exist") != std::string::npos) {
        return false;
    }
    
    // Check for UP flag in the output
    bool is_up = (output.find("state UP") != std::string::npos || 
                  output.find("UP,") != std::string::npos);
    
    // Also check for RUNNING flag which indicates actual activity
    bool is_running = (output.find("RUNNING") != std::string::npos);
    
    return is_up && is_running;
}

bool InterfaceController::isInterfaceConnected(const std::string& interface_name) {
    // Check if interface is active and has a valid IP address
    if (!isInterfaceActive(interface_name)) {
        return false;
    }
    
    return hasValidIPAddress(interface_name);
}

bool InterfaceController::hasValidIPAddress(const std::string& interface_name) {
    std::string ip_cmd = "ip addr show " + interface_name + " 2>/dev/null";
    std::string output = executeCommand(ip_cmd);
    
    if (output.empty()) {
        return false;
    }
    
    // Look for inet address (excluding loopback)
    std::regex ip_pattern("inet\\s+([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})");
    std::smatch match;
    
    if (std::regex_search(output, match, ip_pattern)) {
        std::string ip = match[1].str();
        // Exclude invalid/placeholder IPs
        if (ip != "127.0.0.1" && ip != "0.0.0.0" && !ip.empty()) {
            std::cout << "Interface " << interface_name << " has valid IP: " << ip << std::endl;
            return true;
        }
    }
    
    return false;
}

bool InterfaceController::isInterfaceRunning(const std::string& interface_name) {
    std::string cmd = "cat /sys/class/net/" + interface_name + "/operstate 2>/dev/null";
    std::string state = executeCommand(cmd);
    
    // Remove newlines and whitespace
    state.erase(std::remove_if(state.begin(), state.end(), ::isspace), state.end());
    
    return (state == "up" || state == "unknown");
}

std::string InterfaceController::findFirstAvailableInterface(const std::string& base_name) {
    std::vector<std::string> existing_interfaces = getExistingWWANInterfaces();
    
    // Check existing interfaces for availability
    for (const auto& interface : existing_interfaces) {
        if (interface.find(base_name) == 0) {
            // Check if this interface is truly active and connected
            if (isInterfaceConnected(interface)) {
                std::cout << "Found active connected interface: " << interface << std::endl;
                return interface;  // Reuse this interface
            } else if (!isInterfaceActive(interface)) {
                std::cout << "Found inactive interface that can be reused: " << interface << std::endl;
                return interface;  // Reuse this inactive interface
            }
        }
    }
    
    // If no suitable existing interface, generate a new unique name
    return generateUniqueWWANName(base_name);
}

bool InterfaceController::performSmartCleanup() {
    std::cout << "Performing smart cleanup of WWAN interfaces..." << std::endl;
    
    bool success = true;
    std::vector<std::string> inactive_interfaces = getInactiveWWANInterfaces();
    
    for (const auto& interface : inactive_interfaces) {
        std::cout << "Cleaning up inactive interface: " << interface << std::endl;
        
        // Remove any routes associated with this interface
        std::string route_cmd = "ip route show dev " + interface + " 2>/dev/null";
        std::string routes = executeCommand(route_cmd);
        
        if (!routes.empty()) {
            std::cout << "Removing routes for interface " << interface << std::endl;
            std::string cleanup_routes_cmd = "ip route flush dev " + interface + " 2>/dev/null";
            executeCommand(cleanup_routes_cmd);
        }
        
        // Bring the interface down
        if (!bringInterfaceDown(interface)) {
            std::cerr << "Warning: Failed to bring down interface " << interface << std::endl;
            success = false;
        }
        
        // Enforce raw IP requirement after interface is down but before reset
        if (!enforceRawIPRequirement(interface)) {
            std::cerr << "Warning: Failed to enforce raw IP requirement during cleanup for " << interface << std::endl;
            // Don't fail cleanup for raw IP issues, just warn
        }
        
        // Reset interface configuration
        if (!resetInterface(interface)) {
            std::cerr << "Warning: Failed to reset interface " << interface << std::endl;
            success = false;
        }
    }
    
    std::cout << "Smart cleanup completed. Cleaned " << inactive_interfaces.size() << " interfaces" << std::endl;
    return success;
}

bool InterfaceController::cleanupInactiveInterfaces() {
    return performSmartCleanup();
}

std::vector<std::string> InterfaceController::getInactiveWWANInterfaces() {
    std::vector<std::string> inactive_interfaces;
    std::vector<std::string> all_interfaces = getExistingWWANInterfaces();
    
    for (const auto& interface : all_interfaces) {
        // Interface is inactive if it exists but is not connected
        if (!isInterfaceConnected(interface)) {
            std::cout << "Interface " << interface << " is inactive" << std::endl;
            inactive_interfaces.push_back(interface);
        } else {
            std::cout << "Interface " << interface << " is active and connected" << std::endl;
        }
    }
    
    return inactive_interfaces;
}

// Production-ready interface creation methods
bool InterfaceController::ensureInterfaceExists(const std::string& interface_name, const std::string& device_path) {
    std::cout << "Ensuring interface " << interface_name << " exists for device " << device_path << std::endl;
    
    // Check if interface already exists
    if (isInterfaceNameTaken(interface_name)) {
        std::cout << "Interface " << interface_name << " already exists" << std::endl;
        return true;
    }
    
    // Try to create the interface
    if (!createWWANInterface(interface_name, device_path)) {
        std::cerr << "Failed to create interface " << interface_name << std::endl;
        return false;
    }
    
    // Wait for interface to be created
    if (!waitForInterfaceCreation(interface_name, 10)) {
        std::cerr << "Interface " << interface_name << " was not created within timeout" << std::endl;
        return false;
    }
    
    std::cout << "Interface " << interface_name << " created successfully" << std::endl;
    return true;
}

bool InterfaceController::createWWANInterface(const std::string& interface_name, const std::string& device_path) {
    std::cout << "Creating WWAN interface " << interface_name << " for device " << device_path << std::endl;
    
    // Method 1: Try qmicli to create interface
    std::string qmi_cmd = "qmicli -d " + device_path + " --wda-set-data-format=802-3 --wda-set-interface-mode=ethernet 2>/dev/null";
    std::string qmi_result = executeCommand(qmi_cmd);
    
    // Method 2: Try to trigger interface creation via WDS
    std::string wds_cmd = "qmicli -d " + device_path + " --wds-noop 2>/dev/null";
    executeCommand(wds_cmd);
    
    // Method 3: Use ip link to create interface if supported
    std::string link_cmd = "ip link add " + interface_name + " type dummy 2>/dev/null && ip link delete " + interface_name + " 2>/dev/null";
    executeCommand(link_cmd);
    
    // Method 4: Check if interface appeared naturally
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return true;  // Return true as creation might be asynchronous
}

bool InterfaceController::waitForInterfaceCreation(const std::string& interface_name, int timeout_seconds) {
    std::cout << "Waiting for interface " << interface_name << " to be created (timeout: " << timeout_seconds << "s)" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::seconds(timeout_seconds);
    
    while (std::chrono::steady_clock::now() - start_time < timeout_duration) {
        if (isInterfaceNameTaken(interface_name)) {
            std::cout << "Interface " << interface_name << " detected" << std::endl;
            return true;
        }
        
        // Check if any new WWAN interface appeared
        std::vector<std::string> current_interfaces = getExistingWWANInterfaces();
        for (const auto& iface : current_interfaces) {
            if (iface == interface_name) {
                std::cout << "Interface " << interface_name << " found" << std::endl;
                return true;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    std::cout << "Interface " << interface_name << " creation timeout" << std::endl;
    return false;
}

bool InterfaceController::bindInterfaceToDevice(const std::string& interface_name, const std::string& device_path) {
    std::cout << "Binding interface " << interface_name << " to device " << device_path << std::endl;
    
    // This is typically handled automatically by the kernel/qmi_wwan driver
    // but we can verify the binding
    
    std::string check_cmd = "ls -la /sys/class/net/" + interface_name + "/device 2>/dev/null";
    std::string result = executeCommand(check_cmd);
    
    if (!result.empty()) {
        std::cout << "Interface " << interface_name << " is bound to a device" << std::endl;
        return true;
    }
    
    // Try to find the interface's associated device
    std::string device_check = "find /sys/class/net/" + interface_name + "/ -name 'device' 2>/dev/null";
    std::string device_link = executeCommand(device_check);
    
    return !device_link.empty();
}

bool InterfaceController::enforceRawIPRequirement(const std::string& interface_name) {
    std::cout << "Enforcing raw IP requirement for interface: " << interface_name << std::endl;
    
    // Check if interface supports QMI raw IP
    std::string raw_ip_path = "/sys/class/net/" + interface_name + "/qmi/raw_ip";
    std::ifstream test_file(raw_ip_path);
    if (!test_file.is_open()) {
        std::cout << "Interface " << interface_name << " does not support QMI raw IP mode" << std::endl;
        return true; // Not a QMI interface, allow connection
    }
    test_file.close();
    
    // For QMI interfaces, raw IP mode is mandatory
    std::cout << "QMI interface detected, raw IP mode is required" << std::endl;
    
    // Attempt to verify and set raw IP mode
    int max_attempts = 3;
    for (int attempt = 1; attempt <= max_attempts; attempt++) {
        std::cout << "Raw IP enforcement attempt " << attempt << "/" << max_attempts << std::endl;
        
        if (verifyAndSetRawIP(interface_name)) {
            std::cout << "Raw IP requirement satisfied for " << interface_name << std::endl;
            return true;
        }
        
        if (attempt < max_attempts) {
            std::cout << "Waiting before retry..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    
    std::cerr << "CRITICAL: Cannot satisfy raw IP requirement for interface " << interface_name << std::endl;
    std::cerr << "Connection cannot proceed. Manual intervention may be required." << std::endl;
    return false;
}
