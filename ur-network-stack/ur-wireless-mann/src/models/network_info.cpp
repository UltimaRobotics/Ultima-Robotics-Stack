#include "urwt/models/network_info.hpp"
#include <algorithm>

namespace urwt {

NetworkInfo::NetworkInfo(MacAddress bssid, std::string ssid)
    : bssid_(std::move(bssid))
    , ssid_(std::move(ssid))
    , timestamp_(std::chrono::system_clock::now()) {}

NetworkInfo& NetworkInfo::setBssid(MacAddress bssid) {
    bssid_ = std::move(bssid);
    return *this;
}

NetworkInfo& NetworkInfo::setSsid(std::string ssid) {
    ssid_ = std::move(ssid);
    return *this;
}

NetworkInfo& NetworkInfo::setFrequency(int freq) {
    frequency_ = freq;
    deriveChannel();
    return *this;
}

NetworkInfo& NetworkInfo::setChannel(int channel) {
    channel_ = channel;
    return *this;
}

NetworkInfo& NetworkInfo::setSignalStrength(int strength) {
    signal_strength_ = strength;
    calculateQuality();
    return *this;
}

NetworkInfo& NetworkInfo::setSecurity(SecurityType sec) {
    security_ = sec;
    return *this;
}

NetworkInfo& NetworkInfo::addCapability(std::string cap) {
    capabilities_.push_back(std::move(cap));
    return *this;
}

void NetworkInfo::calculateQuality() {
    if (signal_strength_ >= -50) {
        quality_ = 100;
    } else if (signal_strength_ >= -60) {
        quality_ = 80;
    } else if (signal_strength_ >= -70) {
        quality_ = 60;
    } else if (signal_strength_ >= -80) {
        quality_ = 40;
    } else if (signal_strength_ >= -90) {
        quality_ = 20;
    } else {
        quality_ = 10;
    }
}

void NetworkInfo::deriveChannel() {
    if (frequency_ >= 2412 && frequency_ <= 2484) {
        channel_ = (frequency_ - 2412) / 5 + 1;
    } else if (frequency_ >= 5170 && frequency_ <= 5825) {
        channel_ = (frequency_ - 5170) / 5 + 34;
    }
}

bool NetworkInfo::operator==(const NetworkInfo& other) const {
    return bssid_ == other.bssid_;
}

bool NetworkInfo::operator<(const NetworkInfo& other) const {
    return signal_strength_ > other.signal_strength_;
}

void to_json(json& j, const NetworkInfo& info) {
    j = json{
        {"bssid", info.bssid()},
        {"ssid", info.ssid()},
        {"frequency", info.frequency()},
        {"channel", info.channel()},
        {"signal_strength", info.signalStrength()},
        {"quality", info.quality()},
        {"security", info.security()},
        {"capabilities", info.capabilities()}
    };
}

void from_json(const json& j, NetworkInfo& info) {
    info.setBssid(j.at("bssid").get<MacAddress>())
        .setSsid(j.at("ssid").get<std::string>())
        .setFrequency(j.at("frequency").get<int>())
        .setSignalStrength(j.at("signal_strength").get<int>())
        .setSecurity(j.at("security").get<SecurityType>());
        
    if (j.contains("capabilities")) {
        for (const auto& cap : j.at("capabilities")) {
            info.addCapability(cap.get<std::string>());
        }
    }
}

}
