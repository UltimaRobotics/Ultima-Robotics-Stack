#ifndef URWT_MONITOR_MONITOR_CONFIG_HPP
#define URWT_MONITOR_MONITOR_CONFIG_HPP

#include <chrono>
#include <string>
#include <json.hpp>

namespace urwt {
namespace monitor {

struct MonitorConfig {
    // Scanning
    std::chrono::seconds scan_interval{30};
    std::chrono::seconds scan_timeout{10};
    bool continuous_scanning{true};

    // Connection
    bool auto_connect_enabled{true};
    bool auto_reconnect_enabled{true};
    std::chrono::seconds reconnect_delay{10};
    std::chrono::seconds connection_timeout{30};
    size_t max_connection_attempts{3};

    // Caching
    std::chrono::seconds cache_duration{120};
    bool use_cached_results{true};

    // Event publishing
    bool publish_scan_events{true};
    bool publish_connection_events{true};
    bool publish_state_changes{true};
    std::string event_topic_prefix{"wireless/ur-wireless-mann"};

    // Interface
    std::string interface_name{"wlan0"};

    // Thresholds
    int minimum_signal_strength{-80};  // dBm
    int preferred_signal_strength{-70}; // dBm
};

void to_json(nlohmann::json& j, const MonitorConfig& config);
void from_json(const nlohmann::json& j, MonitorConfig& config);

} // namespace monitor
} // namespace urwt

#endif // URWT_MONITOR_MONITOR_CONFIG_HPP
