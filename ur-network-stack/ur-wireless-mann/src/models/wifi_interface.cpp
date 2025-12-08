#include "urwt/models/wifi_interface.hpp"

namespace urwt {

WifiInterface::WifiInterface(std::string name) : name_(std::move(name)) {}

WifiInterface& WifiInterface::setName(std::string name) {
    name_ = std::move(name);
    return *this;
}

WifiInterface& WifiInterface::setType(std::string type) {
    type_ = std::move(type);
    return *this;
}

WifiInterface& WifiInterface::setStatus(InterfaceStatus status) {
    status_ = status;
    return *this;
}

WifiInterface& WifiInterface::setMac(MacAddress mac) {
    mac_ = std::move(mac);
    return *this;
}

WifiInterface& WifiInterface::setFrequency(int freq) {
    frequency_ = freq;
    if (freq > 0) {
        if (freq >= 2412 && freq <= 2484) {
            channel_ = (freq - 2412) / 5 + 1;
        } else if (freq >= 5170 && freq <= 5825) {
            channel_ = (freq - 5170) / 5 + 34;
        }
    }
    return *this;
}

WifiInterface& WifiInterface::setChannel(int channel) {
    channel_ = channel;
    return *this;
}

WifiInterface& WifiInterface::setSsid(std::string ssid) {
    ssid_ = std::move(ssid);
    return *this;
}

WifiInterface& WifiInterface::setSignalStrength(int strength) {
    signal_strength_ = strength;
    return *this;
}

WifiInterface& WifiInterface::setTxPower(int power) {
    tx_power_ = power;
    return *this;
}

bool WifiInterface::isValid() const {
    return !name_.empty();
}

void to_json(json& j, const WifiInterface& iface) {
    j = json{
        {"name", iface.name()},
        {"type", iface.type()},
        {"status", iface.status()},
        {"mac_address", iface.mac()}
    };

    if (iface.frequency()) j["frequency"] = *iface.frequency();
    if (iface.channel()) j["channel"] = *iface.channel();
    if (iface.ssid()) j["ssid"] = *iface.ssid();
    if (iface.signalStrength()) j["signal_strength"] = *iface.signalStrength();
    if (iface.txPower()) j["tx_power"] = *iface.txPower();
}

void from_json(const json& j, WifiInterface& iface) {
    iface.setName(j.at("name").get<std::string>());
    
    if (j.contains("type")) {
        iface.setType(j.at("type").get<std::string>());
    }
    
    if (j.contains("status")) {
        iface.setStatus(j.at("status").get<InterfaceStatus>());
    }
    
    if (j.contains("mac_address")) {
        iface.setMac(j.at("mac_address").get<MacAddress>());
    }
    
    if (j.contains("frequency")) iface.setFrequency(j.at("frequency").get<int>());
    if (j.contains("channel")) iface.setChannel(j.at("channel").get<int>());
    if (j.contains("ssid")) iface.setSsid(j.at("ssid").get<std::string>());
    if (j.contains("signal_strength")) iface.setSignalStrength(j.at("signal_strength").get<int>());
    if (j.contains("tx_power")) iface.setTxPower(j.at("tx_power").get<int>());
}

}
