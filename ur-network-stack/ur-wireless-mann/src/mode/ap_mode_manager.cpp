#include "urwt/mode/ap_mode_manager.hpp"
#include "urwt/utils/process_executor.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <regex>

namespace urwt {
namespace mode {

APModeManager::APModeManager(std::shared_ptr<WirelessToolsAPI> api)
    : api_(api)
    , hostapd_config_path_("/tmp/hostapd.conf")
    , dnsmasq_config_path_("/tmp/dnsmasq.conf")
    , hostapd_pid_file_("/tmp/hostapd.pid")
    , dnsmasq_pid_file_("/tmp/dnsmasq.pid") {
}

APModeManager::~APModeManager() {
    stopAP();
}

Result<bool, std::string> APModeManager::startAP(const APModeConfig& config) {
    interface_ = config.interface;
    
    auto stop_result = stopAP();
    if (!stop_result.isOk()) {
    }
    
    auto down_result = executeCommand("ip link set " + config.interface + " down 2>&1");
    if (!down_result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to bring interface down: " + down_result.error());
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto mode_result = executeCommand(
        "iw dev " + config.interface + " set type __ap 2>&1");
    if (!mode_result.isOk()) {
    }
    
    auto up_result = executeCommand("ip link set " + config.interface + " up 2>&1");
    if (!up_result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to bring interface up: " + up_result.error());
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto addr_result = executeCommand(
        "ip addr add " + config.ip_address + "/24 dev " + config.interface + " 2>&1");
    if (!addr_result.isOk()) {
    }
    
    auto hostapd_config = generateHostapdConfig(config);
    if (!hostapd_config.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to generate hostapd config: " + hostapd_config.error());
    }
    
    std::ofstream hostapd_file(hostapd_config_path_);
    if (!hostapd_file.is_open()) {
        return Result<bool, std::string>::error(
            "Failed to write hostapd config file");
    }
    hostapd_file << hostapd_config.value();
    hostapd_file.close();
    
    auto dnsmasq_config = generateDnsmasqConfig(config);
    if (!dnsmasq_config.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to generate dnsmasq config: " + dnsmasq_config.error());
    }
    
    std::ofstream dnsmasq_file(dnsmasq_config_path_);
    if (!dnsmasq_file.is_open()) {
        return Result<bool, std::string>::error(
            "Failed to write dnsmasq config file");
    }
    dnsmasq_file << dnsmasq_config.value();
    dnsmasq_file.close();
    
    auto forwarding_result = configureIPForwarding();
    if (!forwarding_result.isOk()) {
    }
    
    auto nat_result = configureNAT(config.interface);
    if (!nat_result.isOk()) {
    }
    
    auto hostapd_start = startHostapd(hostapd_config_path_);
    if (!hostapd_start.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to start hostapd: " + hostapd_start.error());
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auto dnsmasq_start = startDnsmasq(dnsmasq_config_path_);
    if (!dnsmasq_start.isOk()) {
        stopHostapd();
        return Result<bool, std::string>::error(
            "Failed to start dnsmasq: " + dnsmasq_start.error());
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> APModeManager::stopAP() {
    stopDnsmasq();
    stopHostapd();
    
    if (!interface_.empty()) {
        executeCommand("ip addr flush dev " + interface_ + " 2>/dev/null");
        executeCommand("ip link set " + interface_ + " down 2>/dev/null");
    }
    
    return Result<bool, std::string>::ok(true);
}

bool APModeManager::isAPRunning() const {
    ProcessExecutor executor;
    auto result = executor.executeShell(
        "pidof hostapd >/dev/null 2>&1 && echo 'running' || echo 'stopped'",
        std::chrono::milliseconds(2000));
    
    if (!result.isOk()) {
        return false;
    }
    
    return result.value().stdout_output.find("running") != std::string::npos;
}

Result<bool, std::string> APModeManager::updateAPConfig(const APModeConfig& config) {
    if (!isAPRunning()) {
        return Result<bool, std::string>::error("AP is not running");
    }
    
    return startAP(config);
}

Result<std::string, std::string> APModeManager::generateHostapdConfig(
    const APModeConfig& config) {
    
    std::stringstream ss;
    ss << "interface=" << config.interface << "\n";
    ss << "driver=nl80211\n";
    ss << "ssid=" << config.ssid << "\n";
    ss << "hw_mode=" << config.hw_mode << "\n";
    ss << "channel=" << config.channel << "\n";
    ss << "wmm_enabled=" << (config.wmm_enabled ? "1" : "0") << "\n";
    ss << "country_code=" << config.country_code << "\n";
    ss << "ieee80211n=" << (config.ieee80211n ? "1" : "0") << "\n";
    ss << "ignore_broadcast_ssid=" << (config.hidden ? "1" : "0") << "\n";
    
    if (config.security == config::SecurityType::WPA2 || config.security == config::SecurityType::WPA3) {
        ss << "auth_algs=1\n";
        ss << "wpa=2\n";
        ss << "wpa_key_mgmt=WPA-PSK\n";
        ss << "wpa_pairwise=CCMP\n";
        ss << "rsn_pairwise=CCMP\n";
        
        if (!config.password.empty()) {
            ss << "wpa_passphrase=" << config.password << "\n";
        }
    }
    
    return Result<std::string, std::string>::ok(ss.str());
}

Result<std::string, std::string> APModeManager::generateDnsmasqConfig(
    const APModeConfig& config) {
    
    std::stringstream ss;
    ss << "interface=" << config.interface << "\n";
    ss << "bind-interfaces\n";
    ss << "dhcp-range=" << config.dhcp_range_start << "," 
       << config.dhcp_range_end << "," << config.dhcp_lease_time << "\n";
    ss << "dhcp-option=3," << config.ip_address << "\n";
    ss << "dhcp-option=6," << config.ip_address << "\n";
    ss << "server=8.8.8.8\n";
    ss << "log-queries\n";
    ss << "log-dhcp\n";
    
    return Result<std::string, std::string>::ok(ss.str());
}

std::vector<APClient> APModeManager::getConnectedClients() const {
    if (interface_.empty()) {
        return std::vector<APClient>();
    }
    
    auto cmd_result = executeCommand("iw dev " + interface_ + " station dump 2>/dev/null");
    if (!cmd_result.isOk()) {
        return std::vector<APClient>();
    }
    
    auto clients_result = parseConnectedClients(cmd_result.value());
    if (!clients_result.isOk()) {
        return std::vector<APClient>();
    }
    
    return clients_result.value();
}

size_t APModeManager::getClientCount() const {
    return getConnectedClients().size();
}

Result<bool, std::string> APModeManager::disconnectClient(const std::string& mac) {
    if (interface_.empty()) {
        return Result<bool, std::string>::error("No active AP interface");
    }
    
    auto result = executeCommand(
        "iw dev " + interface_ + " station del " + mac + " 2>&1");
    
    if (!result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to disconnect client: " + result.error());
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<APModeState, std::string> APModeManager::getAPStatus() const {
    APModeState state;
    
    if (!isAPRunning()) {
        state.active = false;
        return Result<APModeState, std::string>::ok(state);
    }
    
    state.active = true;
    
    auto clients = getConnectedClients();
    state.connected_clients = clients.size();
    for (const auto& client : clients) {
        state.client_mac_addresses.push_back(client.mac_address);
    }
    
    return Result<APModeState, std::string>::ok(state);
}

Result<bool, std::string> APModeManager::configureIPForwarding() {
    auto result = executeCommand("echo 1 > /proc/sys/net/ipv4/ip_forward 2>&1");
    
    if (!result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to enable IP forwarding: " + result.error());
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> APModeManager::configureNAT(const std::string& interface) {
    executeCommand("iptables -t nat -F 2>/dev/null");
    executeCommand("iptables -F 2>/dev/null");
    
    auto nat_result = executeCommand(
        "iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE 2>&1");
    if (!nat_result.isOk()) {
    }
    
    auto forward_result = executeCommand(
        "iptables -A FORWARD -i " + interface + " -o eth0 -j ACCEPT 2>&1");
    if (!forward_result.isOk()) {
    }
    
    auto forward_back_result = executeCommand(
        "iptables -A FORWARD -i eth0 -o " + interface + 
        " -m state --state RELATED,ESTABLISHED -j ACCEPT 2>&1");
    if (!forward_back_result.isOk()) {
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> APModeManager::startHostapd(const std::string& config_path) {
    auto result = executeCommand("hostapd -B -P " + hostapd_pid_file_ + 
                                 " " + config_path + " 2>&1");
    
    if (!result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to start hostapd: " + result.error());
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> APModeManager::startDnsmasq(const std::string& config_path) {
    auto result = executeCommand("dnsmasq -C " + config_path + 
                                 " --pid-file=" + dnsmasq_pid_file_ + " 2>&1");
    
    if (!result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to start dnsmasq: " + result.error());
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> APModeManager::stopHostapd() {
    executeCommand("killall hostapd 2>/dev/null");
    executeCommand("rm -f " + hostapd_pid_file_ + " 2>/dev/null");
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> APModeManager::stopDnsmasq() {
    executeCommand("killall dnsmasq 2>/dev/null");
    executeCommand("rm -f " + dnsmasq_pid_file_ + " 2>/dev/null");
    return Result<bool, std::string>::ok(true);
}

Result<std::vector<APClient>, std::string> APModeManager::parseConnectedClients(
    const std::string& output) const {
    
    std::vector<APClient> clients;
    std::istringstream iss(output);
    std::string line;
    
    std::regex station_regex(R"(^Station\s+([0-9a-fA-F:]+))");
    
    APClient current_client;
    bool has_current = false;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        
        if (std::regex_search(line, match, station_regex)) {
            if (has_current) {
                clients.push_back(current_client);
            }
            current_client = APClient();
            current_client.mac_address = match[1].str();
            has_current = true;
        }
    }
    
    if (has_current) {
        clients.push_back(current_client);
    }
    
    return Result<std::vector<APClient>, std::string>::ok(clients);
}

Result<std::string, std::string> APModeManager::executeCommand(const std::string& cmd) const {
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
