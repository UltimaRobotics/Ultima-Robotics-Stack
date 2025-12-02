#include "MavlinkUdpConnection.h"
#include <sstream>
#include <iomanip>
#include <bitset>

// Flight software version parsing
std::string MavlinkAutopilotVersionInfo::flightSwVersionString() const {
    uint8_t major = (flight_sw_version >> 24) & 0xFF;
    uint8_t minor = (flight_sw_version >> 16) & 0xFF;
    uint8_t patch = (flight_sw_version >> 8) & 0xFF;
    uint8_t type = flight_sw_version & 0xFF;
    
    std::stringstream ss;
    ss << static_cast<int>(major) << "." 
       << static_cast<int>(minor) << "." 
       << static_cast<int>(patch);
    
    // Add firmware type description
    switch (type) {
        case 0: ss << " (dev)"; break;
        case 1: ss << " (alpha)"; break;
        case 2: ss << " (beta)"; break;
        case 3: ss << " (rc)"; break;
        case 4: ss << " (release)"; break;
        default: ss << " (type " << static_cast<int>(type) << ")"; break;
    }
    return ss.str();
}

// Git hash parsing
std::string MavlinkAutopilotVersionInfo::flightCustomVersionString() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; i++) {
        ss << std::setw(2) << static_cast<unsigned int>(flight_custom_version[i]);
    }
    return ss.str();
}

std::string MavlinkAutopilotVersionInfo::middlewareSwVersionString() const {
    uint8_t major = (middleware_sw_version >> 24) & 0xFF;
    uint8_t minor = (middleware_sw_version >> 16) & 0xFF;
    uint8_t patch = (middleware_sw_version >> 8) & 0xFF;
    uint8_t type = middleware_sw_version & 0xFF;
    
    std::stringstream ss;
    ss << static_cast<int>(major) << "." 
       << static_cast<int>(minor) << "." 
       << static_cast<int>(patch);
    
    switch (type) {
        case 0: ss << " (dev)"; break;
        case 1: ss << " (alpha)"; break;
        case 2: ss << " (beta)"; break;
        case 3: ss << " (rc)"; break;
        case 4: ss << " (release)"; break;
        default: ss << " (type " << static_cast<int>(type) << ")"; break;
    }
    return ss.str();
}

std::string MavlinkAutopilotVersionInfo::middlewareCustomVersionString() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; i++) {
        ss << std::setw(2) << static_cast<unsigned int>(middleware_custom_version[i]);
    }
    return ss.str();
}

std::string MavlinkAutopilotVersionInfo::osSwVersionString() const {
    uint8_t major = (os_sw_version >> 24) & 0xFF;
    uint8_t minor = (os_sw_version >> 16) & 0xFF;
    uint8_t patch = (os_sw_version >> 8) & 0xFF;
    uint8_t type = os_sw_version & 0xFF;
    
    std::stringstream ss;
    ss << static_cast<int>(major) << "." 
       << static_cast<int>(minor) << "." 
       << static_cast<int>(patch);
    
    switch (type) {
        case 0: ss << " (dev)"; break;
        case 1: ss << " (alpha)"; break;
        case 2: ss << " (beta)"; break;
        case 3: ss << " (rc)"; break;
        case 4: ss << " (release)"; break;
        default: ss << " (type " << static_cast<int>(type) << ")"; break;
    }
    return ss.str();
}

std::string MavlinkAutopilotVersionInfo::osCustomVersionString() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; i++) {
        ss << std::setw(2) << static_cast<unsigned int>(os_custom_version[i]);
    }
    return ss.str();
}

std::string MavlinkAutopilotVersionInfo::boardVersionString() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << "0x" << std::setw(8) << board_version;
    return ss.str();
}

std::string MavlinkAutopilotVersionInfo::uid2String() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 18; i++) {
        ss << std::setw(2) << static_cast<unsigned int>(uid2[i]);
        if (i < 17 && i % 2 == 1) ss << ":";
    }
    return ss.str();
}

std::string MavlinkAutopilotVersionInfo::capabilitiesString() const {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setfill('0') << std::setw(16) << capabilities;
    return ss.str();
}
