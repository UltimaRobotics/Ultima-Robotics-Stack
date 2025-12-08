#ifndef URWT_STATE_WIRELESS_STATE_HPP
#define URWT_STATE_WIRELESS_STATE_HPP

#include <string>
#include <optional>
#include <chrono>
#include <vector>
#include <json.hpp>
#include "urwt/config/wireless_config_types.hpp"

namespace urwt {
namespace state {

using namespace config;

enum class WirelessHardwareState {
    Unknown,
    Disabled,
    Enabled,
    RfKilled,
    NotPresent
};

enum class InterfaceState {
    Unknown,
    Down,
    Up,
    Connecting,
    Connected,
    Disconnecting,
    APMode
};

enum class ConnectionState {
    Disconnected,
    Authenticating,
    Associating,
    FourWayHandshake,
    Connected,
    Failed
};

struct CurrentConnection {
    std::string ssid;
    std::string bssid;
    int signal_strength{-100};
    int frequency{0};
    int channel{0};
    std::string ip_address;
    std::chrono::system_clock::time_point connected_at;
    std::chrono::seconds uptime{0};

    CurrentConnection() = default;

    bool isConnected() const {
        return !ssid.empty();
    }
};

struct APModeState {
    bool active{false};
    std::string ssid;
    int channel{0};
    int connected_clients{0};
    std::vector<std::string> client_mac_addresses;
    std::chrono::system_clock::time_point started_at;

    APModeState() = default;
};

struct SystemWirelessState {
    WirelessHardwareState hardware_state{WirelessHardwareState::Unknown};
    InterfaceState interface_state{InterfaceState::Unknown};
    ConnectionState connection_state{ConnectionState::Disconnected};
    WirelessMode current_mode{WirelessMode::Unknown};

    std::string interface_name;
    std::optional<CurrentConnection> connection;
    std::optional<APModeState> ap_state;

    bool is_automation_running{false};
    bool is_monitoring_active{false};

    std::chrono::system_clock::time_point last_updated;

    SystemWirelessState() : last_updated(std::chrono::system_clock::now()) {}

    bool isWirelessAvailable() const {
        return hardware_state == WirelessHardwareState::Enabled;
    }

    bool isInterfaceReady() const {
        return interface_state != InterfaceState::Down &&
               interface_state != InterfaceState::Unknown;
    }

    bool isInNeutralState() const {
        return interface_state == InterfaceState::Down ||
               (interface_state == InterfaceState::Up && 
                !connection.has_value() && 
                !ap_state.has_value());
    }
};

void to_json(nlohmann::json& j, const CurrentConnection& conn);
void from_json(const nlohmann::json& j, CurrentConnection& conn);
void to_json(nlohmann::json& j, const APModeState& state);
void from_json(const nlohmann::json& j, APModeState& state);
void to_json(nlohmann::json& j, const SystemWirelessState& state);
void from_json(const nlohmann::json& j, SystemWirelessState& state);

std::string hardwareStateToString(WirelessHardwareState state);
std::string interfaceStateToString(InterfaceState state);
std::string connectionStateToString(ConnectionState state);

}
}

#endif
