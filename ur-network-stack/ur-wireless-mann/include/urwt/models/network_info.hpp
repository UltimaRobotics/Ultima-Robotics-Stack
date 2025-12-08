#ifndef URWT_MODELS_NETWORK_INFO_HPP
#define URWT_MODELS_NETWORK_INFO_HPP

#include <string>
#include <vector>
#include <chrono>
#include "../common/types.hpp"

namespace urwt {

class NetworkInfo {
public:
    NetworkInfo(MacAddress bssid, std::string ssid);
    NetworkInfo() = default;

    const MacAddress& bssid() const noexcept { return bssid_; }
    const std::string& ssid() const noexcept { return ssid_; }
    int frequency() const noexcept { return frequency_; }
    int channel() const noexcept { return channel_; }
    int signalStrength() const noexcept { return signal_strength_; }
    int quality() const noexcept { return quality_; }
    SecurityType security() const noexcept { return security_; }
    const std::vector<std::string>& capabilities() const noexcept { 
        return capabilities_; 
    }

    NetworkInfo& setBssid(MacAddress bssid);
    NetworkInfo& setSsid(std::string ssid);
    NetworkInfo& setFrequency(int freq);
    NetworkInfo& setChannel(int channel);
    NetworkInfo& setSignalStrength(int strength);
    NetworkInfo& setSecurity(SecurityType sec);
    NetworkInfo& addCapability(std::string cap);

    bool operator==(const NetworkInfo& other) const;
    bool operator<(const NetworkInfo& other) const;

private:
    MacAddress bssid_;
    std::string ssid_;
    int frequency_{0};
    int channel_{0};
    int signal_strength_{-100};
    int quality_{0};
    SecurityType security_{SecurityType::Unknown};
    std::vector<std::string> capabilities_;
    std::chrono::system_clock::time_point timestamp_;

    void calculateQuality();
    void deriveChannel();
};

void to_json(json& j, const NetworkInfo& info);
void from_json(const json& j, NetworkInfo& info);

}

#endif
