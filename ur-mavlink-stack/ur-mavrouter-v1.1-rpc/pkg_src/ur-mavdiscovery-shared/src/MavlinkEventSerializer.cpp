#include "MavlinkEventSerializer.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace MavlinkShared {

// DeviceInfo serialization
json MavlinkEventSerializer::deviceInfoToJson(const DeviceInfo& deviceInfo) {
    json deviceJson;
    deviceJson["devicePath"] = deviceInfo.devicePath;
    deviceJson["state"] = deviceStateToString(deviceInfo.state);
    deviceJson["baudrate"] = deviceInfo.baudrate;
    deviceJson["systemId"] = deviceInfo.sysid;
    deviceJson["componentId"] = deviceInfo.compid;
    deviceJson["mavlinkVersion"] = deviceInfo.mavlinkVersion;
    deviceJson["timestamp"] = deviceInfo.timestamp;
    
    // Add USB info
    deviceJson["usbInfo"] = usbInfoToJson(deviceInfo.usbInfo);
    
    // Add messages
    json messagesJson = json::array();
    for (const auto& msg : deviceInfo.messages) {
        messagesJson.push_back(mavlinkMessageToJson(msg));
    }
    deviceJson["messages"] = messagesJson;
    
    return deviceJson;
}

DeviceInfo MavlinkEventSerializer::deviceInfoFromJson(const json& jsonData) {
    DeviceInfo deviceInfo;
    deviceInfo.devicePath = safeGetString(jsonData, "devicePath");
    deviceInfo.state = deviceStateFromString(safeGetString(jsonData, "state", "UNKNOWN"));
    deviceInfo.baudrate = safeGetInt(jsonData, "baudrate");
    deviceInfo.sysid = static_cast<uint8_t>(safeGetInt(jsonData, "systemId"));
    deviceInfo.compid = static_cast<uint8_t>(safeGetInt(jsonData, "componentId"));
    deviceInfo.mavlinkVersion = static_cast<uint8_t>(safeGetInt(jsonData, "mavlinkVersion"));
    deviceInfo.timestamp = safeGetString(jsonData, "timestamp");
    
    // Parse USB info
    if (jsonData.contains("usbInfo") && jsonData["usbInfo"].is_object()) {
        deviceInfo.usbInfo = usbInfoFromJson(jsonData["usbInfo"]);
    }
    
    // Parse messages
    if (jsonData.contains("messages") && jsonData["messages"].is_array()) {
        for (const auto& msgJson : jsonData["messages"]) {
            deviceInfo.messages.push_back(mavlinkMessageFromJson(msgJson));
        }
    }
    
    return deviceInfo;
}

// USBDeviceInfo serialization
json MavlinkEventSerializer::usbInfoToJson(const USBDeviceInfo& usbInfo) {
    json usbJson;
    usbJson["deviceName"] = usbInfo.deviceName;
    usbJson["manufacturer"] = usbInfo.manufacturer;
    usbJson["serialNumber"] = usbInfo.serialNumber;
    usbJson["vendorId"] = usbInfo.vendorId;
    usbJson["productId"] = usbInfo.productId;
    usbJson["boardClass"] = usbInfo.boardClass;
    usbJson["boardName"] = usbInfo.boardName;
    usbJson["autopilotType"] = usbInfo.autopilotType;
    return usbJson;
}

USBDeviceInfo MavlinkEventSerializer::usbInfoFromJson(const json& jsonData) {
    USBDeviceInfo usbInfo;
    usbInfo.deviceName = safeGetString(jsonData, "deviceName");
    usbInfo.manufacturer = safeGetString(jsonData, "manufacturer");
    usbInfo.serialNumber = safeGetString(jsonData, "serialNumber");
    usbInfo.vendorId = safeGetString(jsonData, "vendorId");
    usbInfo.productId = safeGetString(jsonData, "productId");
    usbInfo.boardClass = safeGetString(jsonData, "boardClass");
    usbInfo.boardName = safeGetString(jsonData, "boardName");
    usbInfo.autopilotType = safeGetString(jsonData, "autopilotType");
    return usbInfo;
}

// MAVLinkMessage serialization
json MavlinkEventSerializer::mavlinkMessageToJson(const MAVLinkMessage& message) {
    json msgJson;
    msgJson["msgid"] = message.msgid;
    msgJson["name"] = message.name;
    return msgJson;
}

MAVLinkMessage MavlinkEventSerializer::mavlinkMessageFromJson(const json& jsonData) {
    MAVLinkMessage message;
    message.msgid = static_cast<uint8_t>(safeGetInt(jsonData, "msgid"));
    message.name = safeGetString(jsonData, "name");
    return message;
}

// Enum conversions
std::string MavlinkEventSerializer::deviceStateToString(DeviceState state) {
    switch (state) {
        case DeviceState::UNKNOWN: return "UNKNOWN";
        case DeviceState::VERIFYING: return "VERIFYING";
        case DeviceState::VERIFIED: return "VERIFIED";
        case DeviceState::NON_MAVLINK: return "NON_MAVLINK";
        case DeviceState::REMOVED: return "REMOVED";
        default: return "UNKNOWN";
    }
}

DeviceState MavlinkEventSerializer::deviceStateFromString(const std::string& stateStr) {
    if (stateStr == "VERIFYING") return DeviceState::VERIFYING;
    if (stateStr == "VERIFIED") return DeviceState::VERIFIED;
    if (stateStr == "NON_MAVLINK") return DeviceState::NON_MAVLINK;
    if (stateStr == "REMOVED") return DeviceState::REMOVED;
    return DeviceState::UNKNOWN;
}

std::string MavlinkEventSerializer::eventTypeToString(EventType type) {
    switch (type) {
        case EventType::DEVICE_ADDED: return "DEVICE_ADDED";
        case EventType::DEVICE_REMOVED: return "DEVICE_REMOVED";
        case EventType::DEVICE_VERIFIED: return "DEVICE_VERIFIED";
        case EventType::INIT_PROCESS_DISCOVERY: return "INIT_PROCESS_DISCOVERY";
        default: return "UNKNOWN";
    }
}

EventType MavlinkEventSerializer::eventTypeFromString(const std::string& typeStr) {
    if (typeStr == "DEVICE_ADDED") return EventType::DEVICE_ADDED;
    if (typeStr == "DEVICE_REMOVED") return EventType::DEVICE_REMOVED;
    if (typeStr == "DEVICE_VERIFIED") return EventType::DEVICE_VERIFIED;
    if (typeStr == "INIT_PROCESS_DISCOVERY") return EventType::INIT_PROCESS_DISCOVERY;
    return EventType::DEVICE_ADDED; // Default fallback
}

// RPC request creation (ur-rpc-template format)
json MavlinkEventSerializer::createDeviceAddedRequest(const DeviceAddedEvent& event) {
    json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "mavlink_device_added";
    request["params"] = deviceInfoToJson(event.deviceInfo);
    request["id"] = generateTransactionId();
    request["timestamp"] = event.timestamp;
    request["source"] = event.sourceService;
    return request;
}

json MavlinkEventSerializer::createDeviceRemovedRequest(const DeviceRemovedEvent& event) {
    json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "mavlink_device_removed";
    
    json params;
    params["devicePath"] = event.devicePath;
    params["timestamp"] = event.timestamp;
    params["source"] = event.sourceService;
    request["params"] = params;
    
    request["id"] = generateTransactionId();
    request["timestamp"] = event.timestamp;
    request["source"] = event.sourceService;
    return request;
}

// Notification creation (shared bus format)
json MavlinkEventSerializer::createDeviceVerifiedNotification(const DeviceVerifiedNotification& notification) {
    json notif;
    notif["eventType"] = eventTypeToString(notification.eventType);
    notif["source"] = notification.sourceService;
    notif["timestamp"] = notification.timestamp;
    notif["payload"] = deviceInfoToJson(notification.deviceInfo);
    notif["targetTopic"] = "ur-shared-bus/ur-mavlink-stack/notifications";
    return notif;
}

json MavlinkEventSerializer::createDeviceRemovedNotification(const DeviceRemovedNotification& notification) {
    json notif;
    notif["eventType"] = eventTypeToString(notification.eventType);
    notif["source"] = notification.sourceService;
    notif["timestamp"] = notification.timestamp;
    
    json payload;
    payload["devicePath"] = notification.devicePath;
    payload["timestamp"] = notification.timestamp;
    payload["source"] = notification.sourceService;
    notif["payload"] = payload;
    
    notif["targetTopic"] = "ur-shared-bus/ur-mavlink-stack/notifications";
    return notif;
}

json MavlinkEventSerializer::createInitProcessDiscoveryNotification(const InitProcessDiscoveryEvent& event) {
    json notif;
    notif["eventType"] = eventTypeToString(event.eventType);
    notif["source"] = event.sourceService;
    notif["timestamp"] = event.timestamp;
    
    json devicesArray = json::array();
    for (const auto& device : event.existingDevices) {
        devicesArray.push_back(deviceInfoToJson(device));
    }
    
    json payload;
    payload["existingDevices"] = devicesArray;
    payload["discoveryCount"] = static_cast<int>(event.existingDevices.size());
    notif["payload"] = payload;
    
    notif["targetTopic"] = "ur-shared-bus/ur-mavlink-stack/notifications";
    return notif;
}

// RPC request/response parsing
MavlinkRpcRequest MavlinkEventSerializer::parseRpcRequest(const json& jsonData) {
    std::string method = safeGetString(jsonData, "method");
    std::string service = safeGetString(jsonData, "service");
    MavlinkRpcRequest request(method, service);
    request.authority = safeGetString(jsonData, "authority", "USER");
    request.transactionId = safeGetString(jsonData, "id");
    
    if (jsonData.contains("params")) {
        request.params = jsonData["params"];
    }
    
    return request;
}

json MavlinkEventSerializer::createRpcResponse(const MavlinkRpcResponse& response) {
    json resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = response.transactionId;
    
    if (response.success) {
        resp["result"] = response.result;
    } else {
        resp["error"] = json::object();
        resp["error"]["code"] = response.errorCode;
        resp["error"]["message"] = response.errorMessage;
    }
    
    return resp;
}

MavlinkNotification MavlinkEventSerializer::parseNotification(const json& jsonData) {
    std::string eventTypeStr = safeGetString(jsonData, "eventType");
    std::string service = safeGetString(jsonData, "source", "ur-mavdiscovery");
    EventType eventType = eventTypeFromString(eventTypeStr);
    MavlinkNotification notification(eventType, service);
    notification.timestamp = safeGetString(jsonData, "timestamp");
    notification.targetTopic = safeGetString(jsonData, "targetTopic");
    
    if (jsonData.contains("payload")) {
        notification.payload = jsonData["payload"];
    }
    
    return notification;
}

json MavlinkEventSerializer::createNotificationJson(const MavlinkNotification& notification) {
    json notif;
    notif["eventType"] = eventTypeToString(notification.eventType);
    notif["source"] = notification.sourceService;
    notif["timestamp"] = notification.timestamp;
    notif["payload"] = notification.payload;
    notif["targetTopic"] = notification.targetTopic;
    return notif;
}

// Utility functions
std::string MavlinkEventSerializer::generateTransactionId() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return std::to_string(ms);
}

std::string MavlinkEventSerializer::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    return std::to_string(time_t);
}

// Helper functions
std::string MavlinkEventSerializer::safeGetString(const json& jsonData, const std::string& key, const std::string& defaultValue) {
    if (jsonData.contains(key) && jsonData[key].is_string()) {
        return jsonData[key].get<std::string>();
    }
    return defaultValue;
}

int MavlinkEventSerializer::safeGetInt(const json& jsonData, const std::string& key, int defaultValue) {
    if (jsonData.contains(key) && jsonData[key].is_number()) {
        return jsonData[key].get<int>();
    }
    return defaultValue;
}

bool MavlinkEventSerializer::safeGetBool(const json& jsonData, const std::string& key, bool defaultValue) {
    if (jsonData.contains(key) && jsonData[key].is_boolean()) {
        return jsonData[key].get<bool>();
    }
    return defaultValue;
}

} // namespace MavlinkShared
