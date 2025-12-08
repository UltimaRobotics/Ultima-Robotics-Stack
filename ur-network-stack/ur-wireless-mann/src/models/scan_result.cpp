#include "urwt/models/scan_result.hpp"
#include <algorithm>

namespace urwt {

ScanResult::ScanResult(WifiInterface interface, 
                       std::vector<NetworkInfo> networks,
                       std::chrono::milliseconds duration)
    : interface_(std::move(interface))
    , networks_(std::move(networks))
    , duration_(duration)
    , timestamp_(std::chrono::system_clock::now()) {}

std::optional<NetworkInfo> ScanResult::findBySSID(const std::string& ssid) const {
    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&ssid](const NetworkInfo& info) {
            return info.ssid() == ssid;
        });
    
    if (it != networks_.end()) {
        return *it;
    }
    return std::nullopt;
}

std::optional<NetworkInfo> ScanResult::findByBSSID(const MacAddress& bssid) const {
    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&bssid](const NetworkInfo& info) {
            return info.bssid() == bssid;
        });
    
    if (it != networks_.end()) {
        return *it;
    }
    return std::nullopt;
}

std::vector<NetworkInfo> ScanResult::filterBySignal(int minStrength) const {
    std::vector<NetworkInfo> result;
    std::copy_if(networks_.begin(), networks_.end(), std::back_inserter(result),
        [minStrength](const NetworkInfo& info) {
            return info.signalStrength() >= minStrength;
        });
    return result;
}

std::vector<NetworkInfo> ScanResult::filterBySecurity(SecurityType type) const {
    std::vector<NetworkInfo> result;
    std::copy_if(networks_.begin(), networks_.end(), std::back_inserter(result),
        [type](const NetworkInfo& info) {
            return info.security() == type;
        });
    return result;
}

void to_json(json& j, const ScanResult& result) {
    auto timestamp_t = std::chrono::system_clock::to_time_t(result.timestamp());

    j = json{
        {"interface", result.interface()},
        {"scan_time", timestamp_t},
        {"scan_duration_ms", result.duration().count()},
        {"results_count", result.count()},
        {"scan_results", result.networks()}
    };
}

void from_json(const json& j, ScanResult& result) {
}

}
