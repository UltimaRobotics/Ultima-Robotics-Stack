#pragma once

#include "MavlinkDeviceStructs.hpp"
#include <string>
#include <memory>
#include <functional>

namespace MavlinkShared {

// Serialization/deserialization interface
class MavlinkEventSerializer {
public:
    // Convert DeviceInfo to JSON
    static json deviceInfoToJson(const DeviceInfo& deviceInfo);
    
    // Convert JSON to DeviceInfo
    static DeviceInfo deviceInfoFromJson(const json& jsonData);
    
    // Convert USBDeviceInfo to JSON
    static json usbInfoToJson(const USBDeviceInfo& usbInfo);
    
    // Convert JSON to USBDeviceInfo
    static USBDeviceInfo usbInfoFromJson(const json& jsonData);
    
    // Convert MAVLinkMessage to JSON
    static json mavlinkMessageToJson(const MAVLinkMessage& message);
    
    // Convert JSON to MAVLinkMessage
    static MAVLinkMessage mavlinkMessageFromJson(const json& jsonData);
    
    // Convert DeviceState enum to string
    static std::string deviceStateToString(DeviceState state);
    
    // Convert string to DeviceState enum
    static DeviceState deviceStateFromString(const std::string& stateStr);
    
    // Convert EventType enum to string
    static std::string eventTypeToString(EventType type);
    
    // Convert string to EventType enum
    static EventType eventTypeFromString(const std::string& typeStr);
    
    // Create RPC request JSON for device added (ur-rpc-template format)
    static json createDeviceAddedRequest(const DeviceAddedEvent& event);
    
    // Create RPC request JSON for device removed (ur-rpc-template format)
    static json createDeviceRemovedRequest(const DeviceRemovedEvent& event);
    
    // Create notification JSON for device verified (shared bus format)
    static json createDeviceVerifiedNotification(const DeviceVerifiedNotification& notification);
    
    // Create notification JSON for device removed (shared bus format)
    static json createDeviceRemovedNotification(const DeviceRemovedNotification& notification);
    
    // Create init process discovery notification
    static json createInitProcessDiscoveryNotification(const InitProcessDiscoveryEvent& event);
    
    // Parse RPC request from JSON
    static MavlinkRpcRequest parseRpcRequest(const json& jsonData);
    
    // Create RPC response JSON
    static json createRpcResponse(const MavlinkRpcResponse& response);
    
    // Parse notification from JSON
    static MavlinkNotification parseNotification(const json& jsonData);
    
    // Create notification JSON from event
    static json createNotificationJson(const MavlinkNotification& notification);
    
    // Utility functions
    static std::string generateTransactionId();
    static std::string getCurrentTimestamp();
    
private:
    // Helper to safely get string value from JSON
    static std::string safeGetString(const json& jsonData, const std::string& key, const std::string& defaultValue = "");
    
    // Helper to safely get number value from JSON
    static int safeGetInt(const json& jsonData, const std::string& key, int defaultValue = 0);
    
    // Helper to safely get boolean value from JSON
    static bool safeGetBool(const json& jsonData, const std::string& key, bool defaultValue = false);
};

} // namespace MavlinkShared
