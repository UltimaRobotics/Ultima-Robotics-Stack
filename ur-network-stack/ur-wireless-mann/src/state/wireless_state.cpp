#include "urwt/state/wireless_state.hpp"
#include <sstream>
#include <iomanip>

namespace urwt {
namespace state {

std::string hardwareStateToString(WirelessHardwareState state) {
    switch (state) {
        case WirelessHardwareState::Unknown: return "Unknown";
        case WirelessHardwareState::Disabled: return "Disabled";
        case WirelessHardwareState::Enabled: return "Enabled";
        case WirelessHardwareState::RfKilled: return "RfKilled";
        case WirelessHardwareState::NotPresent: return "NotPresent";
        default: return "Unknown";
    }
}

std::string interfaceStateToString(InterfaceState state) {
    switch (state) {
        case InterfaceState::Unknown: return "Unknown";
        case InterfaceState::Down: return "Down";
        case InterfaceState::Up: return "Up";
        case InterfaceState::Connecting: return "Connecting";
        case InterfaceState::Connected: return "Connected";
        case InterfaceState::Disconnecting: return "Disconnecting";
        case InterfaceState::APMode: return "APMode";
        default: return "Unknown";
    }
}

std::string connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected: return "Disconnected";
        case ConnectionState::Authenticating: return "Authenticating";
        case ConnectionState::Associating: return "Associating";
        case ConnectionState::FourWayHandshake: return "FourWayHandshake";
        case ConnectionState::Connected: return "Connected";
        case ConnectionState::Failed: return "Failed";
        default: return "Disconnected";
    }
}

void to_json(nlohmann::json& j, const CurrentConnection& conn) {
    j = nlohmann::json{
        {"ssid", conn.ssid},
        {"bssid", conn.bssid},
        {"signal_strength", conn.signal_strength},
        {"frequency", conn.frequency},
        {"channel", conn.channel},
        {"ip_address", conn.ip_address},
        {"uptime_seconds", conn.uptime.count()}
    };
    
    auto time_t_val = std::chrono::system_clock::to_time_t(conn.connected_at);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
    j["connected_at"] = ss.str();
}

void from_json(const nlohmann::json& j, CurrentConnection& conn) {
    j.at("ssid").get_to(conn.ssid);
    j.at("bssid").get_to(conn.bssid);
    j.at("signal_strength").get_to(conn.signal_strength);
    j.at("frequency").get_to(conn.frequency);
    j.at("channel").get_to(conn.channel);
    j.at("ip_address").get_to(conn.ip_address);
    
    if (j.contains("uptime_seconds")) {
        conn.uptime = std::chrono::seconds(j.at("uptime_seconds").get<int64_t>());
    }
}

void to_json(nlohmann::json& j, const APModeState& state) {
    j = nlohmann::json{
        {"active", state.active},
        {"ssid", state.ssid},
        {"channel", state.channel},
        {"connected_clients", state.connected_clients},
        {"client_mac_addresses", state.client_mac_addresses}
    };
    
    auto time_t_val = std::chrono::system_clock::to_time_t(state.started_at);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
    j["started_at"] = ss.str();
}

void from_json(const nlohmann::json& j, APModeState& state) {
    j.at("active").get_to(state.active);
    j.at("ssid").get_to(state.ssid);
    j.at("channel").get_to(state.channel);
    j.at("connected_clients").get_to(state.connected_clients);
    j.at("client_mac_addresses").get_to(state.client_mac_addresses);
}

void to_json(nlohmann::json& j, const SystemWirelessState& state) {
    j = nlohmann::json{
        {"hardware_state", hardwareStateToString(state.hardware_state)},
        {"interface_state", interfaceStateToString(state.interface_state)},
        {"connection_state", connectionStateToString(state.connection_state)},
        {"current_mode", config::wirelessModeToString(state.current_mode)},
        {"interface_name", state.interface_name},
        {"is_automation_running", state.is_automation_running},
        {"is_monitoring_active", state.is_monitoring_active}
    };
    
    if (state.connection.has_value()) {
        j["connection"] = state.connection.value();
    } else {
        j["connection"] = nullptr;
    }
    
    if (state.ap_state.has_value()) {
        j["ap_state"] = state.ap_state.value();
    } else {
        j["ap_state"] = nullptr;
    }
    
    auto time_t_val = std::chrono::system_clock::to_time_t(state.last_updated);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%SZ");
    j["last_updated"] = ss.str();
}

void from_json(const nlohmann::json& j, SystemWirelessState& state) {
    state.interface_name = j.at("interface_name").get<std::string>();
    state.is_automation_running = j.at("is_automation_running").get<bool>();
    state.is_monitoring_active = j.at("is_monitoring_active").get<bool>();
    
    if (j.contains("connection") && !j.at("connection").is_null()) {
        state.connection = j.at("connection").get<CurrentConnection>();
    }
    
    if (j.contains("ap_state") && !j.at("ap_state").is_null()) {
        state.ap_state = j.at("ap_state").get<APModeState>();
    }
}

}
}
