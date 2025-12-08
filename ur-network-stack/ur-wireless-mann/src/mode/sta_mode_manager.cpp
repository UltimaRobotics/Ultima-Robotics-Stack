#include "urwt/mode/sta_mode_manager.hpp"
#include "urwt/utils/process_executor.hpp"
#include <thread>
#include <chrono>
#include <sstream>

namespace urwt {
namespace mode {

STAModeManager::STAModeManager(std::shared_ptr<WirelessToolsAPI> api)
    : api_(api) {
}

STAModeManager::~STAModeManager() = default;

Result<bool, std::string> STAModeManager::enableSTAMode(const STAModeConfig& config) {
    std::cout << "[STAModeManager] enableSTAMode called for interface: " << config.interface << std::endl;
    
    // Clean up any existing connections first (like C implementation)
    std::cout << "[STAModeManager] Cleaning up existing connections..." << std::endl;
    executeCommand("killall udhcpc 2>/dev/null || true");
    executeCommand("killall dhclient 2>/dev/null || true");
    executeCommand("killall wpa_supplicant 2>/dev/null || true");
    executeCommand("ip addr flush dev " + config.interface + " 2>/dev/null");
    executeCommand("iw dev " + config.interface + " disconnect 2>/dev/null");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "[STAModeManager] Bringing interface up..." << std::endl;
    auto iface_result = executeCommand(
        "ip link set " + config.interface + " up 2>&1");
    if (!iface_result.isOk()) {
        std::cerr << "[STAModeManager] Failed to bring interface up: " << iface_result.error() << std::endl;
        return Result<bool, std::string>::error(
            "Failed to bring interface up: " + iface_result.error());
    }
    std::cout << "[STAModeManager] Interface brought up successfully" << std::endl;
    
    std::cout << "[STAModeManager] Waiting 500ms for interface to stabilize..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "[STAModeManager] Setting interface to managed mode..." << std::endl;
    auto mode_result = executeCommand(
        "iw dev " + config.interface + " set type managed 2>&1");
    if (!mode_result.isOk()) {
        std::cerr << "[STAModeManager] Warning: Failed to set managed mode: " << mode_result.error() << std::endl;
    } else {
        std::cout << "[STAModeManager] Managed mode set successfully" << std::endl;
    }
    
    std::cout << "[STAModeManager] Configuring interface..." << std::endl;
    auto configure_result = configureInterface(config);
    if (!configure_result.isOk()) {
        std::cerr << "[STAModeManager] Failed to configure interface: " << configure_result.error() << std::endl;
        return configure_result;
    }
    std::cout << "[STAModeManager] Interface configured successfully" << std::endl;
    
    if (!config.regulatory_domain.empty()) {
        std::cout << "[STAModeManager] Setting regulatory domain to: " << config.regulatory_domain << std::endl;
        auto reg_result = setRegulatoryDomain(config.regulatory_domain);
        if (reg_result.isOk()) {
            std::cout << "[STAModeManager] Regulatory domain set successfully" << std::endl;
        } else {
            std::cerr << "[STAModeManager] Warning: Failed to set regulatory domain: " << reg_result.error() << std::endl;
        }
    }
    
    std::cout << "[STAModeManager] Setting power save to: " << (config.power_save ? "enabled" : "disabled") << std::endl;
    auto ps_result = setPowerSave(config.power_save, config.interface);
    if (ps_result.isOk()) {
        std::cout << "[STAModeManager] Power save configured successfully" << std::endl;
    } else {
        std::cerr << "[STAModeManager] Warning: Failed to set power save: " << ps_result.error() << std::endl;
    }
    
    std::cout << "[STAModeManager] STA mode enabled successfully" << std::endl;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> STAModeManager::disableSTAMode(const std::string& interface) {
    std::cout << "[STAModeManager] Disabling STA mode on " << interface << std::endl;
    
    // Kill all DHCP clients
    std::cout << "[STAModeManager] Killing DHCP clients..." << std::endl;
    executeCommand("killall dhclient 2>/dev/null");
    executeCommand("killall udhcpc 2>/dev/null");
    
    // Kill wpa_supplicant
    std::cout << "[STAModeManager] Killing wpa_supplicant..." << std::endl;
    executeCommand("killall wpa_supplicant 2>/dev/null");
    
    // Disconnect using NetworkManager if available
    executeCommand("nmcli device disconnect " + interface + " 2>/dev/null");
    
    // Disconnect using iw
    std::cout << "[STAModeManager] Disconnecting interface..." << std::endl;
    executeCommand("iw dev " + interface + " disconnect 2>/dev/null");
    
    std::cout << "[STAModeManager] Waiting 500ms for cleanup..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Flush IP addresses
    std::cout << "[STAModeManager] Flushing IP addresses..." << std::endl;
    executeCommand("ip addr flush dev " + interface + " 2>/dev/null");
    
    // Bring interface down
    std::cout << "[STAModeManager] Bringing interface down..." << std::endl;
    auto down_result = executeCommand("ip link set " + interface + " down 2>&1");
    if (!down_result.isOk()) {
        std::cerr << "[STAModeManager] Warning: Failed to bring interface down: " << down_result.error() << std::endl;
        // Don't fail - interface may already be down
    }
    
    std::cout << "[STAModeManager] STA mode disabled successfully" << std::endl;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> STAModeManager::configureInterface(const STAModeConfig& config) {
    std::cout << "[STAModeManager] Configuring interface: DHCP=" << (config.dhcp_enabled ? "enabled" : "disabled") << std::endl;
    
    if (config.dhcp_enabled) {
        std::cout << "[STAModeManager] Enabling DHCP..." << std::endl;
        return enableDHCP(config.interface);
    } else if (config.static_ip_config.isConfigured()) {
        std::cout << "[STAModeManager] Configuring static IP..." << std::endl;
        return setStaticIP(config.static_ip_config, config.interface);
    }
    
    std::cout << "[STAModeManager] No IP configuration needed" << std::endl;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> STAModeManager::setStaticIP(
    const StaticIPConfig& ip_config,
    const std::string& interface) {
    
    if (!ip_config.ip_address.has_value()) {
        return Result<bool, std::string>::error("No IP address specified");
    }
    
    executeCommand("ip addr flush dev " + interface + " 2>/dev/null");
    
    std::string netmask = ip_config.netmask.value_or("255.255.255.0");
    int cidr = 24;
    if (netmask == "255.255.255.0") cidr = 24;
    else if (netmask == "255.255.0.0") cidr = 16;
    else if (netmask == "255.0.0.0") cidr = 8;
    
    auto ip_result = executeCommand(
        "ip addr add " + ip_config.ip_address.value() + "/" + std::to_string(cidr) +
        " dev " + interface + " 2>&1");
    
    if (!ip_result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to set IP address: " + ip_result.error());
    }
    
    if (ip_config.gateway.has_value()) {
        executeCommand("ip route del default 2>/dev/null");
        auto gw_result = executeCommand(
            "ip route add default via " + ip_config.gateway.value() +
            " dev " + interface + " 2>&1");
        
        if (!gw_result.isOk()) {
            return Result<bool, std::string>::error(
                "Failed to set gateway: " + gw_result.error());
        }
    }
    
    if (!ip_config.dns_servers.empty()) {
        std::stringstream dns_content;
        for (const auto& dns : ip_config.dns_servers) {
            dns_content << "nameserver " << dns << "\n";
        }
        
        executeCommand(
            "echo '" + dns_content.str() + "' > /etc/resolv.conf 2>/dev/null");
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> STAModeManager::enableDHCP(const std::string& interface) {
    std::cout << "[STAModeManager] Killing existing DHCP clients..." << std::endl;
    executeCommand("killall dhclient 2>/dev/null");
    executeCommand("killall udhcpc 2>/dev/null");
    
    std::cout << "[STAModeManager] Waiting 500ms for cleanup..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Flush IP addresses from interface
    std::cout << "[STAModeManager] Flushing IP addresses from " << interface << std::endl;
    executeCommand("ip addr flush dev " + interface + " 2>/dev/null");
    
    std::cout << "[STAModeManager] Waiting 200ms after flush..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Check if interface has carrier before attempting DHCP
    std::cout << "[STAModeManager] Checking interface carrier status..." << std::endl;
    auto carrier_check = executeCommand("cat /sys/class/net/" + interface + "/carrier 2>/dev/null || echo '0'");
    if (carrier_check.isOk() && carrier_check.value().find("1") == std::string::npos) {
        std::cout << "[STAModeManager] Warning: Interface has no carrier, DHCP may not succeed" << std::endl;
    }
    
    // Try udhcpc first (like C implementation), then fall back to dhclient
    std::cout << "[STAModeManager] Starting DHCP client on " << interface << std::endl;
    auto udhcpc_result = executeCommand("timeout 8 udhcpc -i " + interface + " -n 2>&1 &");
    
    if (!udhcpc_result.isOk()) {
        std::cout << "[STAModeManager] udhcpc not available, trying dhclient..." << std::endl;
        // Use dhclient as fallback with -nw (no wait) and timeout
        auto dhcp_result = executeCommand("timeout 10 dhclient -nw -timeout 10 " + interface + " 2>&1");
        
        if (!dhcp_result.isOk()) {
            std::cerr << "[STAModeManager] DHCP client failed to start: " << dhcp_result.error() << std::endl;
            std::cerr << "[STAModeManager] Continuing without DHCP (interface may need manual configuration or network connection)" << std::endl;
        } else {
            std::cout << "[STAModeManager] dhclient started (acquiring lease in background)" << std::endl;
        }
    } else {
        std::cout << "[STAModeManager] udhcpc started (acquiring lease in background)" << std::endl;
    }
    
    // Always return success - DHCP is optional
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> STAModeManager::setPowerSave(
    bool enabled, 
    const std::string& interface) {
    
    std::string value = enabled ? "on" : "off";
    auto result = executeCommand(
        "iw dev " + interface + " set power_save " + value + " 2>&1");
    
    if (!result.isOk()) {
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> STAModeManager::setRegulatoryDomain(const std::string& domain) {
    auto result = executeCommand("iw reg set " + domain + " 2>&1");
    
    if (!result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to set regulatory domain: " + result.error());
    }
    
    return Result<bool, std::string>::ok(true);
}

bool STAModeManager::isSTAModeActive(const std::string& interface) const {
    ProcessExecutor executor;
    auto result = executor.executeShell(
        "iw dev " + interface + " info 2>/dev/null",
        std::chrono::milliseconds(3000));
    
    if (!result.isOk()) {
        return false;
    }
    
    std::string output = result.value().stdout_output;
    return output.find("type managed") != std::string::npos ||
           output.find("type station") != std::string::npos;
}

Result<std::string, std::string> STAModeManager::executeCommand(const std::string& cmd) {
    ProcessExecutor executor;
    auto result = executor.executeShell(cmd, std::chrono::milliseconds(5000));
    
    if (!result.isOk()) {
        return Result<std::string, std::string>::error(result.error());
    }
    
    if (result.value().exit_code != 0) {
        std::string error_msg = "Command failed with exit code " + 
                               std::to_string(result.value().exit_code);
        if (!result.value().stderr_output.empty()) {
            error_msg += ": " + result.value().stderr_output;
        }
        return Result<std::string, std::string>::error(error_msg);
    }
    
    return Result<std::string, std::string>::ok(result.value().stdout_output);
}

}
}
