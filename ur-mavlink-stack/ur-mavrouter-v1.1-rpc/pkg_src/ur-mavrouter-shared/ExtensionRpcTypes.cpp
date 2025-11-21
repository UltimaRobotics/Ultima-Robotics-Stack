#include "ExtensionRpcTypes.hpp"
#include "../modules/nholmann/json.hpp"
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

namespace URMavRouterShared {

std::string ExtensionTypeConverter::extensionTypeToString(ExtensionType type) {
    switch (type) {
        case ExtensionType::TCP: return "TCP";
        case ExtensionType::UDP: return "UDP";
        case ExtensionType::SERIAL: return "SERIAL";
        case ExtensionType::LOGGING: return "LOGGING";
        case ExtensionType::FILTER: return "FILTER";
        case ExtensionType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

ExtensionType ExtensionTypeConverter::stringToExtensionType(const std::string& type) {
    std::string upperType = type;
    std::transform(upperType.begin(), upperType.end(), upperType.begin(), ::toupper);
    
    if (upperType == "TCP") return ExtensionType::TCP;
    if (upperType == "UDP") return ExtensionType::UDP;
    if (upperType == "SERIAL") return ExtensionType::SERIAL;
    if (upperType == "LOGGING") return ExtensionType::LOGGING;
    if (upperType == "FILTER") return ExtensionType::FILTER;
    if (upperType == "CUSTOM") return ExtensionType::CUSTOM;
    
    return ExtensionType::CUSTOM; // Default
}

std::string ExtensionTypeConverter::extensionOperationToString(ExtensionOperation operation) {
    switch (operation) {
        case ExtensionOperation::ADD: return "add";
        case ExtensionOperation::REMOVE: return "remove";
        case ExtensionOperation::START: return "start";
        case ExtensionOperation::STOP: return "stop";
        case ExtensionOperation::STATUS: return "status";
        case ExtensionOperation::CONFIGURE: return "configure";
        default: return "unknown";
    }
}

ExtensionOperation ExtensionTypeConverter::stringToExtensionOperation(const std::string& operation) {
    std::string lowerOp = operation;
    std::transform(lowerOp.begin(), lowerOp.end(), lowerOp.begin(), ::tolower);
    
    if (lowerOp == "add") return ExtensionOperation::ADD;
    if (lowerOp == "remove") return ExtensionOperation::REMOVE;
    if (lowerOp == "start") return ExtensionOperation::START;
    if (lowerOp == "stop") return ExtensionOperation::STOP;
    if (lowerOp == "status") return ExtensionOperation::STATUS;
    if (lowerOp == "configure") return ExtensionOperation::CONFIGURE;
    
    return ExtensionOperation::STATUS; // Default
}

std::string ExtensionTypeConverter::extensionStateToString(ExtensionState state) {
    switch (state) {
        case ExtensionState::CREATED: return "created";
        case ExtensionState::RUNNING: return "running";
        case ExtensionState::STOPPED: return "stopped";
        case ExtensionState::ERROR: return "error";
        case ExtensionState::CONFIGURED: return "configured";
        default: return "unknown";
    }
}

ExtensionState ExtensionTypeConverter::stringToExtensionState(const std::string& state) {
    std::string lowerState = state;
    std::transform(lowerState.begin(), lowerState.end(), lowerState.begin(), ::tolower);
    
    if (lowerState == "created") return ExtensionState::CREATED;
    if (lowerState == "running") return ExtensionState::RUNNING;
    if (lowerState == "stopped") return ExtensionState::STOPPED;
    if (lowerState == "error") return ExtensionState::ERROR;
    if (lowerState == "configured") return ExtensionState::CONFIGURED;
    
    return ExtensionState::CREATED; // Default
}

ExtensionConfig ExtensionTypeConverter::parseExtensionConfigFromJson(const std::string& jsonStr) {
    ExtensionConfig config;
    
    try {
        json j = json::parse(jsonStr);
        
        if (j.contains("name")) {
            config.name = j["name"].get<std::string>();
        }
        
        if (j.contains("type")) {
            config.type = stringToExtensionType(j["type"].get<std::string>());
        }
        
        if (j.contains("address")) {
            config.address = j["address"].get<std::string>();
        }
        
        if (j.contains("port")) {
            config.port = j["port"].get<int>();
        }
        
        if (j.contains("devicePath")) {
            config.devicePath = j["devicePath"].get<std::string>();
        }
        
        if (j.contains("baudrate")) {
            config.baudrate = j["baudrate"].get<int>();
        }
        
        if (j.contains("flowControl")) {
            config.flowControl = j["flowControl"].get<bool>();
        }
        
        if (j.contains("assigned_extension_point")) {
            config.assignedExtensionPoint = j["assigned_extension_point"].get<std::string>();
        }
        
        if (j.contains("parameters") && j["parameters"].is_object()) {
            for (auto& [key, value] : j["parameters"].items()) {
                config.parameters[key] = value.get<std::string>();
            }
        }
        
    } catch (const std::exception& e) {
        // Return default config on parse error
        config = ExtensionConfig();
    }
    
    return config;
}

std::string ExtensionTypeConverter::extensionConfigToJson(const ExtensionConfig& config) {
    json j;
    
    j["name"] = config.name;
    j["type"] = extensionTypeToString(config.type);
    j["address"] = config.address;
    j["port"] = config.port;
    j["devicePath"] = config.devicePath;
    j["baudrate"] = config.baudrate;
    j["flowControl"] = config.flowControl;
    j["assigned_extension_point"] = config.assignedExtensionPoint;
    
    if (!config.parameters.empty()) {
        j["parameters"] = config.parameters;
    }
    
    return j.dump();
}

ExtensionInfo ExtensionTypeConverter::createExtensionInfo(const ExtensionConfig& config, 
                                                         unsigned int threadId, 
                                                         ExtensionState state, 
                                                         bool isRunning) {
    ExtensionInfo info;
    
    info.name = config.name;
    info.threadId = threadId;
    info.type = config.type;
    info.state = state;
    info.isRunning = isRunning;
    info.address = config.address;
    info.port = config.port;
    info.devicePath = config.devicePath;
    info.baudrate = config.baudrate;
    info.assignedExtensionPoint = config.assignedExtensionPoint;
    info.parameters = config.parameters;
    info.errorMessage = "";
    
    return info;
}

std::string ExtensionTypeConverter::extensionInfoToJson(const ExtensionInfo& info) {
    json j;
    
    j["name"] = info.name;
    j["threadId"] = info.threadId;
    j["type"] = extensionTypeToString(info.type);
    j["state"] = extensionStateToString(info.state);
    j["isRunning"] = info.isRunning;
    j["address"] = info.address;
    j["port"] = info.port;
    j["devicePath"] = info.devicePath;
    j["baudrate"] = info.baudrate;
    j["assigned_extension_point"] = info.assignedExtensionPoint;
    
    if (!info.parameters.empty()) {
        j["parameters"] = info.parameters;
    }
    
    if (!info.errorMessage.empty()) {
        j["errorMessage"] = info.errorMessage;
    }
    
    return j.dump();
}

std::string ExtensionTypeConverter::extensionListToJson(const std::vector<ExtensionInfo>& extensions) {
    json j = json::array();
    
    for (const auto& info : extensions) {
        j.push_back(json::parse(extensionInfoToJson(info)));
    }
    
    return j.dump();
}

} // namespace URMavRouterShared
