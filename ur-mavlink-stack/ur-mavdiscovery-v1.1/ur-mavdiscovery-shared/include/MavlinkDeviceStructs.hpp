#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <memory>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace MavlinkShared {

// Device state enumeration
enum class DeviceState {
    UNKNOWN = 0,
    VERIFYING = 1,
    VERIFIED = 2,
    NON_MAVLINK = 3,
    REMOVED = 4
};

// MAVLink message information
struct MAVLinkMessage {
    uint8_t msgid;
    std::string name;
    
    MAVLinkMessage() : msgid(0) {}
    MAVLinkMessage(uint8_t id, const std::string& msg_name) : msgid(id), name(msg_name) {}
};

// USB device information
struct USBDeviceInfo {
    std::string deviceName;
    std::string manufacturer;
    std::string serialNumber;
    std::string vendorId;
    std::string productId;
    std::string boardClass;      // Flight controller class (e.g., "Pixhawk", "ArduPilot")
    std::string boardName;        // Specific board name (e.g., "PX4 FMU V2", "Cube Orange")
    std::string autopilotType;    // Type of autopilot (e.g., "PX4", "ArduPilot", "Generic")
    
    USBDeviceInfo() = default;
};

// Complete device information
struct DeviceInfo {
    std::string devicePath;
    DeviceState state;
    int baudrate;
    uint8_t sysid;
    uint8_t compid;
    std::vector<MAVLinkMessage> messages;
    uint8_t mavlinkVersion;
    std::string timestamp;
    USBDeviceInfo usbInfo;
    
    DeviceInfo() : state(DeviceState::UNKNOWN), baudrate(0), sysid(0), compid(0), mavlinkVersion(0) {}
};

// Event types for notifications
enum class EventType {
    DEVICE_ADDED = 0,
    DEVICE_REMOVED = 1,
    DEVICE_VERIFIED = 2,
    INIT_PROCESS_DISCOVERY = 3
};

// Base event structure
struct BaseEvent {
    EventType eventType;
    std::string timestamp;
    std::string sourceService;    // e.g., "ur-mavdiscovery"
    
    BaseEvent(EventType type, const std::string& service = "ur-mavdiscovery") 
        : eventType(type), sourceService(service) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        timestamp = std::to_string(time_t);
    }
    virtual ~BaseEvent() = default;
};

// Device added event (for ur-mavrouter/ur-mavcollector requests)
struct DeviceAddedEvent : public BaseEvent {
    DeviceInfo deviceInfo;
    
    DeviceAddedEvent(const DeviceInfo& info) 
        : BaseEvent(EventType::DEVICE_ADDED), deviceInfo(info) {}
};

// Device removed event (for ur-mavrouter/ur-mavcollector requests)
struct DeviceRemovedEvent : public BaseEvent {
    std::string devicePath;
    
    DeviceRemovedEvent(const std::string& path) 
        : BaseEvent(EventType::DEVICE_REMOVED), devicePath(path) {}
};

// Device verified notification (for ur-shared-bus notifications)
struct DeviceVerifiedNotification : public BaseEvent {
    DeviceInfo deviceInfo;
    
    DeviceVerifiedNotification(const DeviceInfo& info) 
        : BaseEvent(EventType::DEVICE_VERIFIED), deviceInfo(info) {}
};

// Device removed notification (for ur-shared-bus notifications)
struct DeviceRemovedNotification : public BaseEvent {
    std::string devicePath;
    
    DeviceRemovedNotification(const std::string& path) 
        : BaseEvent(EventType::DEVICE_REMOVED), devicePath(path) {}
};

// Init process discovery event
struct InitProcessDiscoveryEvent : public BaseEvent {
    std::vector<DeviceInfo> existingDevices;
    
    InitProcessDiscoveryEvent(const std::vector<DeviceInfo>& devices) 
        : BaseEvent(EventType::INIT_PROCESS_DISCOVERY), existingDevices(devices) {}
};

// RPC request wrapper for ur-rpc-template
struct MavlinkRpcRequest {
    std::string transactionId;
    std::string method;
    std::string service;
    std::string authority;
    json params;
    
    MavlinkRpcRequest(const std::string& req_method, const std::string& req_service = "") 
        : method(req_method), service(req_service), authority("USER") {
        // Generate transaction ID using timestamp
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        transactionId = std::to_string(ms);
    }
};

// RPC response wrapper
struct MavlinkRpcResponse {
    std::string transactionId;
    bool success;
    json result;
    std::string errorMessage;
    int errorCode;
    
    MavlinkRpcResponse(const std::string& trans_id, bool resp_success = true) 
        : transactionId(trans_id), success(resp_success), errorCode(0) {}
};

// Notification wrapper for shared bus
struct MavlinkNotification {
    EventType eventType;
    std::string sourceService;
    std::string targetTopic;      // e.g., "ur-shared-bus/ur-mavlink-stack/notifications"
    json payload;
    std::string timestamp;
    
    MavlinkNotification(EventType type, const std::string& service = "ur-mavdiscovery") 
        : eventType(type), sourceService(service), 
          targetTopic("ur-shared-bus/ur-mavlink-stack/notifications") {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        timestamp = std::to_string(time_t);
    }
};

} // namespace MavlinkShared
