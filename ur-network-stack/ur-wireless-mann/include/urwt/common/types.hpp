#ifndef URWT_COMMON_TYPES_HPP
#define URWT_COMMON_TYPES_HPP

#include <string>
#include <regex>
#include <stdexcept>
#include <iostream>
#include "../../json.hpp"

namespace urwt {

using json = nlohmann::json;

enum class InterfaceStatus {
    Up,
    Down,
    Unknown
};

enum class SecurityType {
    Open,
    WEP,
    WPA,
    WPA2,
    WPA3,
    Unknown
};

inline std::string to_string(InterfaceStatus status) {
    switch (status) {
        case InterfaceStatus::Up: return "up";
        case InterfaceStatus::Down: return "down";
        case InterfaceStatus::Unknown: return "unknown";
        default: return "unknown";
    }
}

inline std::string to_string(SecurityType sec) {
    switch (sec) {
        case SecurityType::Open: return "Open";
        case SecurityType::WEP: return "WEP";
        case SecurityType::WPA: return "WPA";
        case SecurityType::WPA2: return "WPA2";
        case SecurityType::WPA3: return "WPA3";
        case SecurityType::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

class MacAddress {
public:
    MacAddress() : address_("00:00:00:00:00:00") {}
    
    explicit MacAddress(const std::string& addr) : address_(addr) {
        if (!isValid(addr)) {
            throw std::invalid_argument("Invalid MAC address format: " + addr);
        }
    }

    const std::string& get() const noexcept { return address_; }
    std::string toString() const noexcept { return address_; }
    
    bool operator==(const MacAddress& other) const {
        return address_ == other.address_;
    }
    
    bool operator<(const MacAddress& other) const {
        return address_ < other.address_;
    }

    static bool isValid(const std::string& addr) {
        std::regex mac_regex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
        return std::regex_match(addr, mac_regex);
    }

private:
    std::string address_;
};

inline void to_json(json& j, const MacAddress& mac) {
    j = mac.get();
}

inline void from_json(const json& j, MacAddress& mac) {
    mac = MacAddress(j.get<std::string>());
}

}

NLOHMANN_JSON_SERIALIZE_ENUM(urwt::SecurityType, {
    {urwt::SecurityType::Open, "Open"},
    {urwt::SecurityType::WEP, "WEP"},
    {urwt::SecurityType::WPA, "WPA"},
    {urwt::SecurityType::WPA2, "WPA2"},
    {urwt::SecurityType::WPA3, "WPA3"},
    {urwt::SecurityType::Unknown, "Unknown"}
})

NLOHMANN_JSON_SERIALIZE_ENUM(urwt::InterfaceStatus, {
    {urwt::InterfaceStatus::Up, "up"},
    {urwt::InterfaceStatus::Down, "down"},
    {urwt::InterfaceStatus::Unknown, "unknown"}
})

#endif
