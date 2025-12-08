#ifndef URWT_MODELS_WIFI_INTERFACE_HPP
#define URWT_MODELS_WIFI_INTERFACE_HPP

#include <string>
#include <optional>
#include "../common/types.hpp"

namespace urwt {

class WifiInterface {
public:
    explicit WifiInterface(std::string name);
    WifiInterface() = default;

    const std::string& name() const noexcept { return name_; }
    const std::string& type() const noexcept { return type_; }
    InterfaceStatus status() const noexcept { return status_; }
    const MacAddress& mac() const noexcept { return mac_; }
    std::optional<int> frequency() const noexcept { return frequency_; }
    std::optional<int> channel() const noexcept { return channel_; }
    std::optional<std::string> ssid() const noexcept { return ssid_; }
    std::optional<int> signalStrength() const noexcept { return signal_strength_; }
    std::optional<int> txPower() const noexcept { return tx_power_; }

    WifiInterface& setName(std::string name);
    WifiInterface& setType(std::string type);
    WifiInterface& setStatus(InterfaceStatus status);
    WifiInterface& setMac(MacAddress mac);
    WifiInterface& setFrequency(int freq);
    WifiInterface& setChannel(int channel);
    WifiInterface& setSsid(std::string ssid);
    WifiInterface& setSignalStrength(int strength);
    WifiInterface& setTxPower(int power);

    bool isValid() const;
    bool isUp() const { return status_ == InterfaceStatus::Up; }
    bool isConnected() const { return ssid_.has_value(); }

private:
    std::string name_;
    std::string type_{"managed"};
    InterfaceStatus status_{InterfaceStatus::Down};
    MacAddress mac_;
    std::optional<int> frequency_;
    std::optional<int> channel_;
    std::optional<std::string> ssid_;
    std::optional<int> signal_strength_;
    std::optional<int> tx_power_;
};

void to_json(json& j, const WifiInterface& iface);
void from_json(const json& j, WifiInterface& iface);

}

#endif
