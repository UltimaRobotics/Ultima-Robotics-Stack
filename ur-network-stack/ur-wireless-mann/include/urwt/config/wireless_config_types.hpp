#ifndef URWT_CONFIG_WIRELESS_CONFIG_TYPES_HPP
#define URWT_CONFIG_WIRELESS_CONFIG_TYPES_HPP

#include <string>
#include <vector>
#include <optional>
#include <json.hpp>

namespace urwt {
namespace config {

enum class WirelessMode {
    STA,
    AP,
    Unknown
};

enum class SecurityType {
    Open,
    WEP,
    WPA,
    WPA2,
    WPA3,
    WPA2Enterprise,
    Unknown
};

struct NetworkProfile {
    std::string ssid;
    SecurityType security{SecurityType::WPA2};
    std::string password;
    int priority{5};
    bool auto_connect{true};
    std::optional<std::string> bssid;
    bool hidden{false};
    
    std::optional<std::string> identity;
    std::optional<std::string> ca_cert;
    std::optional<std::string> client_cert;
    std::optional<std::string> private_key;
    
    NetworkProfile() = default;
    NetworkProfile(const std::string& ssid_, SecurityType sec = SecurityType::WPA2)
        : ssid(ssid_), security(sec) {}
    
    bool operator<(const NetworkProfile& other) const {
        return priority > other.priority;
    }
    
    bool operator==(const NetworkProfile& other) const {
        return ssid == other.ssid;
    }
};

struct StaticIPConfig {
    std::optional<std::string> ip_address;
    std::optional<std::string> netmask;
    std::optional<std::string> gateway;
    std::vector<std::string> dns_servers;
    
    StaticIPConfig() = default;
    
    bool isConfigured() const {
        return ip_address.has_value();
    }
};

struct STAModeConfig {
    std::vector<NetworkProfile> saved_networks;
    std::string interface;  // No default - must be set from config or detection
    bool dhcp_enabled{true};
    StaticIPConfig static_ip_config;
    bool power_save{false};
    std::string regulatory_domain{"US"};
    
    STAModeConfig() = default;
};

struct APModeConfig {
    std::string ssid{"UR-Wireless-AP"};
    SecurityType security{SecurityType::WPA2};
    std::string password;
    int channel{6};
    std::string interface;  // No default - must be set from config or detection
    std::string ip_address{"192.168.4.1"};
    std::string netmask{"255.255.255.0"};
    std::string dhcp_range_start{"192.168.4.100"};
    std::string dhcp_range_end{"192.168.4.200"};
    std::string dhcp_lease_time{"12h"};
    int max_clients{10};
    bool hidden{false};
    std::string country_code{"US"};
    std::string hw_mode{"g"};
    bool ieee80211n{true};
    bool wmm_enabled{true};
    
    APModeConfig() = default;
};

struct AutomationConfig {
    bool enabled{true};
    bool auto_connect{true};
    bool auto_reconnect{true};
    int reconnect_delay_seconds{10};
    int connection_timeout_seconds{30};
    int scan_interval_seconds{30};
    int max_connection_attempts{3};
    
    AutomationConfig() = default;
};

struct MonitoringConfig {
    bool enabled{true};
    int scan_interval_seconds{30};
    int scan_timeout_seconds{10};
    int cache_duration_seconds{120};
    bool publish_events{true};
    std::string event_topic{"wireless/ur-wireless-mann/events"};
    
    MonitoringConfig() = default;
};

struct PersistenceConfig {
    std::string config_file{"/etc/ur-wireless-mann/wireless-config.json"};
    std::string saved_networks_file{"/etc/ur-wireless-mann/saved-networks.json"};
    std::string state_file{"/var/lib/ur-wireless-mann/state.json"};
    bool backup_enabled{true};
    int backup_count{5};
    
    PersistenceConfig() = default;
};

struct SecurityConfig {
    bool encryption_enabled{true};
    std::string encryption_key_file{"/etc/ur-wireless-mann/encryption.key"};
    bool require_encryption_for_passwords{true};
    
    SecurityConfig() = default;
};

struct WirelessConfig {
    bool enabled{true};
    WirelessMode mode{WirelessMode::STA};
    AutomationConfig automation;
    STAModeConfig sta_mode;
    APModeConfig ap_mode;
    MonitoringConfig monitoring;
    PersistenceConfig persistence;
    SecurityConfig security;
    
    WirelessConfig() = default;
};

void to_json(nlohmann::json& j, const NetworkProfile& profile);
void from_json(const nlohmann::json& j, NetworkProfile& profile);
void to_json(nlohmann::json& j, const StaticIPConfig& config);
void from_json(const nlohmann::json& j, StaticIPConfig& config);
void to_json(nlohmann::json& j, const STAModeConfig& config);
void from_json(const nlohmann::json& j, STAModeConfig& config);
void to_json(nlohmann::json& j, const APModeConfig& config);
void from_json(const nlohmann::json& j, APModeConfig& config);
void to_json(nlohmann::json& j, const AutomationConfig& config);
void from_json(const nlohmann::json& j, AutomationConfig& config);
void to_json(nlohmann::json& j, const MonitoringConfig& config);
void from_json(const nlohmann::json& j, MonitoringConfig& config);
void to_json(nlohmann::json& j, const PersistenceConfig& config);
void from_json(const nlohmann::json& j, PersistenceConfig& config);
void to_json(nlohmann::json& j, const SecurityConfig& config);
void from_json(const nlohmann::json& j, SecurityConfig& config);
void to_json(nlohmann::json& j, const WirelessConfig& config);
void from_json(const nlohmann::json& j, WirelessConfig& config);

std::string wirelessModeToString(WirelessMode mode);
WirelessMode stringToWirelessMode(const std::string& str);
std::string securityTypeToString(SecurityType type);
SecurityType stringToSecurityType(const std::string& str);

}
}

#endif
