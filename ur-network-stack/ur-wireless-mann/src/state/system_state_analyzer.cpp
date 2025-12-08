#include "urwt/state/system_state_analyzer.hpp"
#include "urwt/utils/process_executor.hpp"
#include "urwt/managers/interface_detector.hpp"
#include <sstream>
#include <regex>
#include <algorithm>

namespace urwt {
namespace state {

SystemStateAnalyzer::SystemStateAnalyzer(std::shared_ptr<WirelessToolsAPI> api)
    : api_(api) {
}

SystemStateAnalyzer::~SystemStateAnalyzer() = default;

Result<SystemWirelessState, std::string> SystemStateAnalyzer::analyzeCurrentState() {
    auto interfaces_result = api_->listInterfaces();
    if (!interfaces_result.isOk() || interfaces_result.value().empty()) {
        return Result<SystemWirelessState, std::string>::error(
            "No wireless interfaces found");
    }
    
    return analyzeInterface(interfaces_result.value()[0].name());
}

Result<SystemWirelessState, std::string> SystemStateAnalyzer::analyzeInterface(
    const std::string& iface) {
    
    SystemWirelessState state;
    state.interface_name = iface;
    state.last_updated = std::chrono::system_clock::now();
    
    auto hw_state = detectHardwareState();
    if (hw_state.isOk()) {
        state.hardware_state = hw_state.value();
    }
    
    auto iface_state = detectInterfaceState(iface);
    if (iface_state.isOk()) {
        state.interface_state = iface_state.value();
    }
    
    auto mode = detectCurrentMode(iface);
    if (mode.isOk()) {
        state.current_mode = mode.value();
    }
    
    auto conn_state = detectConnectionState(iface);
    if (conn_state.isOk()) {
        state.connection_state = conn_state.value();
    }
    
    if (state.current_mode == config::WirelessMode::STA) {
        auto connection = getCurrentConnection(iface);
        if (connection.isOk() && connection.value().isConnected()) {
            state.connection = connection.value();
        }
    } else if (state.current_mode == config::WirelessMode::AP) {
        auto ap_state = getAPState(iface);
        if (ap_state.isOk() && ap_state.value().active) {
            state.ap_state = ap_state.value();
        }
    }
    
    return Result<SystemWirelessState, std::string>::ok(state);
}

Result<WirelessHardwareState, std::string> SystemStateAnalyzer::detectHardwareState() {
    auto rf_killed = isRfKilled();
    if (rf_killed.isOk() && rf_killed.value()) {
        return Result<WirelessHardwareState, std::string>::ok(
            WirelessHardwareState::RfKilled);
    }
    
    auto interfaces_result = api_->listInterfaces();
    if (!interfaces_result.isOk() || interfaces_result.value().empty()) {
        return Result<WirelessHardwareState, std::string>::ok(
            WirelessHardwareState::NotPresent);
    }
    
    return Result<WirelessHardwareState, std::string>::ok(
        WirelessHardwareState::Enabled);
}

Result<InterfaceState, std::string> SystemStateAnalyzer::detectInterfaceState(
    const std::string& iface) {
    
    auto cmd_result = executeCommand("ip link show " + iface + " 2>/dev/null");
    if (!cmd_result.isOk()) {
        return Result<InterfaceState, std::string>::ok(InterfaceState::Unknown);
    }
    
    std::string output = cmd_result.value();
    
    if (output.find("state DOWN") != std::string::npos) {
        return Result<InterfaceState, std::string>::ok(InterfaceState::Down);
    }
    
    if (output.find("state UP") == std::string::npos) {
        return Result<InterfaceState, std::string>::ok(InterfaceState::Down);
    }
    
    auto is_ap = isInterfaceInAPMode(iface);
    if (is_ap.isOk() && is_ap.value()) {
        return Result<InterfaceState, std::string>::ok(InterfaceState::APMode);
    }
    
    auto connection = getCurrentConnection(iface);
    if (connection.isOk() && connection.value().isConnected()) {
        return Result<InterfaceState, std::string>::ok(InterfaceState::Connected);
    }
    
    return Result<InterfaceState, std::string>::ok(InterfaceState::Up);
}

Result<ConnectionState, std::string> SystemStateAnalyzer::detectConnectionState(
    const std::string& iface) {
    
    auto connection = getCurrentConnection(iface);
    if (!connection.isOk()) {
        return Result<ConnectionState, std::string>::ok(ConnectionState::Disconnected);
    }
    
    if (connection.value().isConnected()) {
        return Result<ConnectionState, std::string>::ok(ConnectionState::Connected);
    }
    
    return Result<ConnectionState, std::string>::ok(ConnectionState::Disconnected);
}

Result<config::WirelessMode, std::string> SystemStateAnalyzer::detectCurrentMode(
    const std::string& iface) {
    
    auto is_ap = isInterfaceInAPMode(iface);
    if (is_ap.isOk() && is_ap.value()) {
        return Result<config::WirelessMode, std::string>::ok(config::WirelessMode::AP);
    }
    
    auto cmd_result = executeCommand("iw dev " + iface + " info 2>/dev/null");
    if (!cmd_result.isOk()) {
        return Result<config::WirelessMode, std::string>::ok(
            config::WirelessMode::Unknown);
    }
    
    std::string output = cmd_result.value();
    if (output.find("type managed") != std::string::npos ||
        output.find("type station") != std::string::npos) {
        return Result<config::WirelessMode, std::string>::ok(config::WirelessMode::STA);
    }
    
    if (output.find("type AP") != std::string::npos) {
        return Result<config::WirelessMode, std::string>::ok(config::WirelessMode::AP);
    }
    
    return Result<config::WirelessMode, std::string>::ok(config::WirelessMode::Unknown);
}

Result<CurrentConnection, std::string> SystemStateAnalyzer::getCurrentConnection(
    const std::string& iface) {
    
    auto cmd_result = executeCommand("iw dev " + iface + " link 2>/dev/null");
    if (!cmd_result.isOk()) {
        return Result<CurrentConnection, std::string>::error(
            "Failed to get connection info");
    }
    
    auto connection_opt = parseConnectionInfo(cmd_result.value());
    if (!connection_opt.has_value()) {
        return Result<CurrentConnection, std::string>::error("Not connected");
    }
    
    CurrentConnection conn = connection_opt.value();
    
    auto ip_result = executeCommand(
        "ip -4 addr show " + iface + " 2>/dev/null | grep inet | awk '{print $2}' | cut -d/ -f1");
    if (ip_result.isOk()) {
        std::string ip = ip_result.value();
        ip.erase(std::remove(ip.begin(), ip.end(), '\n'), ip.end());
        if (!ip.empty()) {
            conn.ip_address = ip;
        }
    }
    
    return Result<CurrentConnection, std::string>::ok(conn);
}

Result<APModeState, std::string> SystemStateAnalyzer::getAPState(
    const std::string& iface) {
    
    auto cmd_result = executeCommand(
        "pidof hostapd >/dev/null 2>&1 && echo 'running' || echo 'stopped'");
    if (!cmd_result.isOk() || cmd_result.value().find("stopped") != std::string::npos) {
        APModeState state;
        state.active = false;
        return Result<APModeState, std::string>::ok(state);
    }
    
    APModeState state;
    state.active = true;
    
    auto info_result = executeCommand("iw dev " + iface + " info 2>/dev/null");
    if (info_result.isOk()) {
        auto ap_state_opt = parseAPStatus(info_result.value());
        if (ap_state_opt.has_value()) {
            state = ap_state_opt.value();
            state.active = true;
        }
    }
    
    auto clients = getConnectedAPClients(iface);
    if (clients.isOk()) {
        state.client_mac_addresses = clients.value();
        state.connected_clients = clients.value().size();
    }
    
    return Result<APModeState, std::string>::ok(state);
}

StateTransitionPlan SystemStateAnalyzer::compareStates(
    const SystemWirelessState& current,
    const config::WirelessConfig& desired) {
    
    StateTransitionPlan plan;
    plan.from_state = current;
    plan.to_config = desired;
    
    if (!desired.enabled && current.hardware_state == WirelessHardwareState::Enabled) {
        TransitionStep step(
            TransitionAction::DisableHardware,
            "Disable wireless hardware",
            1, true);
        plan.steps.push_back(step);
    }
    
    if (desired.enabled && current.hardware_state != WirelessHardwareState::Enabled) {
        TransitionStep step(
            TransitionAction::EnableHardware,
            "Enable wireless hardware",
            1, true);
        plan.steps.push_back(step);
    }
    
    if (current.current_mode != desired.mode && desired.enabled) {
        if (current.current_mode == config::WirelessMode::AP) {
            plan.steps.push_back(TransitionStep(
                TransitionAction::StopAPMode,
                "Stop AP mode",
                2, true));
        }
        
        if (current.connection.has_value()) {
            plan.steps.push_back(TransitionStep(
                TransitionAction::DisconnectNetwork,
                "Disconnect from current network",
                2, false));
        }
        
        if (desired.mode == config::WirelessMode::STA) {
            plan.steps.push_back(TransitionStep(
                TransitionAction::SwitchToSTAMode,
                "Switch to STA mode",
                3, true));
        } else if (desired.mode == config::WirelessMode::AP) {
            plan.steps.push_back(TransitionStep(
                TransitionAction::SwitchToAPMode,
                "Switch to AP mode",
                3, true));
        }
    }
    
    std::sort(plan.steps.begin(), plan.steps.end());
    plan.estimated_duration_seconds = plan.steps.size() * 5;
    
    return plan;
}

bool SystemStateAnalyzer::requiresStateChange(
    const SystemWirelessState& current,
    const config::WirelessConfig& desired) {
    
    if (current.hardware_state != WirelessHardwareState::Enabled && desired.enabled) {
        return true;
    }
    
    if (current.hardware_state == WirelessHardwareState::Enabled && !desired.enabled) {
        return true;
    }
    
    if (current.current_mode != desired.mode && desired.enabled) {
        return true;
    }
    
    return false;
}

Result<bool, std::string> SystemStateAnalyzer::validateStateTransition(
    const SystemWirelessState& from,
    const SystemWirelessState& to) {
    
    if (from.hardware_state == WirelessHardwareState::NotPresent) {
        return Result<bool, std::string>::error(
            "Cannot transition: no wireless hardware present");
    }
    
    if (from.hardware_state == WirelessHardwareState::RfKilled &&
        to.hardware_state == WirelessHardwareState::Enabled) {
        return Result<bool, std::string>::error(
            "Cannot enable: hardware is RF-killed");
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<std::string, std::string> SystemStateAnalyzer::executeCommand(
    const std::string& cmd) {
    
    ProcessExecutor executor;
    auto result = executor.executeShell(cmd, std::chrono::milliseconds(5000));
    
    if (!result.isOk()) {
        return Result<std::string, std::string>::error(result.error());
    }
    
    if (result.value().exit_code != 0) {
        return Result<std::string, std::string>::error(
            "Command failed with exit code: " + std::to_string(result.value().exit_code));
    }
    
    return Result<std::string, std::string>::ok(result.value().stdout_output);
}

Result<bool, std::string> SystemStateAnalyzer::isRfKilled() {
    auto cmd_result = executeCommand("rfkill list wifi 2>/dev/null");
    if (!cmd_result.isOk()) {
        return Result<bool, std::string>::ok(false);
    }
    
    std::string output = cmd_result.value();
    return Result<bool, std::string>::ok(
        output.find("Soft blocked: yes") != std::string::npos ||
        output.find("Hard blocked: yes") != std::string::npos);
}

Result<bool, std::string> SystemStateAnalyzer::isInterfaceInAPMode(
    const std::string& iface) {
    
    auto cmd_result = executeCommand("iw dev " + iface + " info 2>/dev/null");
    if (!cmd_result.isOk()) {
        return Result<bool, std::string>::ok(false);
    }
    
    std::string output = cmd_result.value();
    return Result<bool, std::string>::ok(output.find("type AP") != std::string::npos);
}

Result<std::vector<std::string>, std::string> SystemStateAnalyzer::getConnectedAPClients(
    const std::string& iface) {
    
    auto cmd_result = executeCommand("iw dev " + iface + " station dump 2>/dev/null");
    if (!cmd_result.isOk()) {
        return Result<std::vector<std::string>, std::string>::ok(
            std::vector<std::string>());
    }
    
    std::vector<std::string> clients;
    std::string output = cmd_result.value();
    std::istringstream iss(output);
    std::string line;
    
    std::regex station_regex(R"(^Station\s+([0-9a-fA-F:]+))");
    
    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, station_regex)) {
            clients.push_back(match[1].str());
        }
    }
    
    return Result<std::vector<std::string>, std::string>::ok(clients);
}

std::optional<CurrentConnection> SystemStateAnalyzer::parseConnectionInfo(
    const std::string& output) {
    
    if (output.find("Not connected") != std::string::npos) {
        return std::nullopt;
    }
    
    CurrentConnection conn;
    
    std::regex ssid_regex(R"(SSID:\s+(.+))");
    std::regex bssid_regex(R"(Connected to\s+([0-9a-fA-F:]+))");
    std::regex freq_regex(R"(freq:\s+(\d+))");
    std::regex signal_regex(R"(signal:\s+(-?\d+)\s+dBm)");
    
    std::smatch match;
    
    if (std::regex_search(output, match, ssid_regex)) {
        conn.ssid = match[1].str();
        conn.ssid.erase(std::remove(conn.ssid.begin(), conn.ssid.end(), '\n'), 
                       conn.ssid.end());
    }
    
    if (std::regex_search(output, match, bssid_regex)) {
        conn.bssid = match[1].str();
    }
    
    if (std::regex_search(output, match, freq_regex)) {
        conn.frequency = std::stoi(match[1].str());
        conn.channel = (conn.frequency - 2407) / 5;
        if (conn.frequency >= 5000) {
            conn.channel = (conn.frequency - 5000) / 5;
        }
    }
    
    if (std::regex_search(output, match, signal_regex)) {
        conn.signal_strength = std::stoi(match[1].str());
    }
    
    if (conn.ssid.empty()) {
        return std::nullopt;
    }
    
    conn.connected_at = std::chrono::system_clock::now();
    return conn;
}

std::optional<APModeState> SystemStateAnalyzer::parseAPStatus(const std::string& output) {
    APModeState state;
    
    std::regex ssid_regex(R"(ssid\s+(.+))");
    std::regex channel_regex(R"(channel\s+(\d+))");
    
    std::smatch match;
    
    if (std::regex_search(output, match, ssid_regex)) {
        state.ssid = match[1].str();
        state.ssid.erase(std::remove(state.ssid.begin(), state.ssid.end(), '\n'), 
                        state.ssid.end());
    }
    
    if (std::regex_search(output, match, channel_regex)) {
        state.channel = std::stoi(match[1].str());
    }
    
    if (state.ssid.empty()) {
        return std::nullopt;
    }
    
    state.started_at = std::chrono::system_clock::now();
    return state;
}

}
}
