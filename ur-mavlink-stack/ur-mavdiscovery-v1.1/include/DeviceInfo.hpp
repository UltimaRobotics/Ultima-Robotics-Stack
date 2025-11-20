
#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <cstdint>
#include "../thirdparty/nholmann/json.hpp"

using json = nlohmann::json;

enum class DeviceState {
    UNKNOWN,
    VERIFYING,
    VERIFIED,
    NON_MAVLINK,
    REMOVED
};

struct MAVLinkMessage {
    uint8_t msgid;
    std::string name;
};

struct USBDeviceInfo {
    std::string deviceName;
    std::string manufacturer;
    std::string serialNumber;
    std::string vendorId;
    std::string productId;
    std::string boardClass;      // Flight controller class (e.g., "Pixhawk", "ArduPilot")
    std::string boardName;        // Specific board name (e.g., "PX4 FMU V2", "Cube Orange")
    std::string autopilotType;    // Type of autopilot (e.g., "PX4", "ArduPilot", "Generic")
};

struct DeviceInfo {
    std::string devicePath;
    std::atomic<DeviceState> state{DeviceState::UNKNOWN};
    int baudrate{0};
    uint8_t sysid{0};
    uint8_t compid{0};
    std::vector<MAVLinkMessage> messages;
    uint8_t mavlinkVersion{0};
    std::string timestamp;
    USBDeviceInfo usbInfo;
    
    // Helper method to convert device info to JSON for RPC notifications
    json toJson() const {
        json deviceJson;
        deviceJson["autopilotType"] = usbInfo.autopilotType;
        deviceJson["baudrate"] = baudrate;
        deviceJson["boardClass"] = usbInfo.boardClass;
        deviceJson["boardName"] = usbInfo.boardName;
        deviceJson["componentId"] = compid;
        deviceJson["deviceName"] = usbInfo.deviceName;
        deviceJson["devicePath"] = devicePath;
        deviceJson["manufacturer"] = usbInfo.manufacturer;
        deviceJson["mavlinkVersion"] = mavlinkVersion;
        deviceJson["productId"] = usbInfo.productId;
        deviceJson["serialNumber"] = usbInfo.serialNumber;
        deviceJson["systemId"] = sysid;
        deviceJson["timestamp"] = timestamp;
        deviceJson["vendorId"] = usbInfo.vendorId;
        return deviceJson;
    }
};
