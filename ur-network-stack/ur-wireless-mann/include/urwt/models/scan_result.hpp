#ifndef URWT_MODELS_SCAN_RESULT_HPP
#define URWT_MODELS_SCAN_RESULT_HPP

#include <vector>
#include <chrono>
#include <optional>
#include "wifi_interface.hpp"
#include "network_info.hpp"

namespace urwt {

class ScanResult {
public:
    ScanResult(WifiInterface interface, 
               std::vector<NetworkInfo> networks,
               std::chrono::milliseconds duration);
    ScanResult() = default;

    const WifiInterface& interface() const { return interface_; }
    const std::vector<NetworkInfo>& networks() const { return networks_; }
    std::size_t count() const { return networks_.size(); }
    std::chrono::milliseconds duration() const { return duration_; }
    const std::chrono::system_clock::time_point& timestamp() const { 
        return timestamp_; 
    }

    std::optional<NetworkInfo> findBySSID(const std::string& ssid) const;
    std::optional<NetworkInfo> findByBSSID(const MacAddress& bssid) const;
    std::vector<NetworkInfo> filterBySignal(int minStrength) const;
    std::vector<NetworkInfo> filterBySecurity(SecurityType type) const;

private:
    WifiInterface interface_;
    std::vector<NetworkInfo> networks_;
    std::chrono::milliseconds duration_{0};
    std::chrono::system_clock::time_point timestamp_;
};

void to_json(json& j, const ScanResult& result);
void from_json(const json& j, ScanResult& result);

}

#endif
