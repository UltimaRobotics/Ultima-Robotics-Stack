#pragma once

#include "ThreadRpcTypes.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace URMavRouterShared {

/**
 * @brief Extension types
 */
enum class ExtensionType {
    TCP = 0,
    UDP = 1,
    SERIAL = 2,
    LOGGING = 3,
    FILTER = 4,
    CUSTOM = 5
};

/**
 * @brief Extension operation types
 */
enum class ExtensionOperation {
    ADD = 0,
    REMOVE = 1,
    START = 2,
    STOP = 3,
    STATUS = 4,
    CONFIGURE = 5
};

/**
 * @brief Extension state
 */
enum class ExtensionState {
    CREATED = 0,
    RUNNING = 1,
    STOPPED = 2,
    ERROR = 3,
    CONFIGURED = 4
};

/**
 * @brief Extension configuration structure
 */
struct ExtensionConfig {
    std::string name;
    ExtensionType type;
    std::string address;
    int port;
    std::string devicePath;
    int baudrate;
    bool flowControl;
    std::string assignedExtensionPoint;
    std::map<std::string, std::string> parameters;
    
    ExtensionConfig() 
        : name("")
        , type(ExtensionType::TCP)
        , address("")
        , port(0)
        , devicePath("")
        , baudrate(57600)
        , flowControl(false)
        , assignedExtensionPoint("") {}
    
    ExtensionConfig(const std::string& extName, ExtensionType extType)
        : name(extName)
        , type(extType)
        , address("")
        , port(0)
        , devicePath("")
        , baudrate(57600)
        , flowControl(false)
        , assignedExtensionPoint("") {}
};

/**
 * @brief Extension information structure
 */
struct ExtensionInfo {
    std::string name;
    unsigned int threadId;
    ExtensionType type;
    ExtensionState state;
    bool isRunning;
    std::string address;
    int port;
    std::string devicePath;
    int baudrate;
    std::string assignedExtensionPoint;
    std::map<std::string, std::string> parameters;
    std::string errorMessage;
    
    ExtensionInfo() 
        : name("")
        , threadId(0)
        , type(ExtensionType::TCP)
        , state(ExtensionState::CREATED)
        , isRunning(false)
        , address("")
        , port(0)
        , devicePath("")
        , baudrate(57600)
        , assignedExtensionPoint("")
        , errorMessage("") {}
};

/**
 * @brief Extension RPC request structure
 */
struct ExtensionRpcRequest {
    ExtensionOperation operation;
    std::string extensionName;
    ExtensionConfig config;
    std::map<std::string, std::string> parameters;
    
    ExtensionRpcRequest() 
        : operation(ExtensionOperation::STATUS)
        , extensionName("") {}
    
    ExtensionRpcRequest(ExtensionOperation op, const std::string& name)
        : operation(op), extensionName(name) {}
};

/**
 * @brief Extension RPC response structure
 */
struct ExtensionRpcResponse {
    OperationStatus status;
    std::string message;
    std::vector<ExtensionInfo> extensions;
    ExtensionInfo extension;
    
    ExtensionRpcResponse() 
        : status(OperationStatus::SUCCESS)
        , message("") {}
    
    ExtensionRpcResponse(OperationStatus stat, const std::string& msg)
        : status(stat), message(msg) {}
};

/**
 * @brief Extension add request with full configuration
 */
struct ExtensionAddRequest {
    ExtensionConfig config;
    bool autoStart;
    bool saveConfig;
    
    ExtensionAddRequest() 
        : autoStart(true)
        , saveConfig(true) {}
};

/**
 * @brief Extension list response for status endpoints
 */
struct ExtensionListResponse {
    OperationStatus status;
    std::string message;
    std::vector<ExtensionInfo> extensions;
    int totalCount;
    
    ExtensionListResponse() 
        : status(OperationStatus::SUCCESS)
        , message("")
        , totalCount(0) {}
};

/**
 * @brief Extension conversion functions
 */
class ExtensionTypeConverter {
public:
    static std::string extensionTypeToString(ExtensionType type);
    static ExtensionType stringToExtensionType(const std::string& type);
    
    static std::string extensionOperationToString(ExtensionOperation operation);
    static ExtensionOperation stringToExtensionOperation(const std::string& operation);
    
    static std::string extensionStateToString(ExtensionState state);
    static ExtensionState stringToExtensionState(const std::string& state);
    
    static ExtensionConfig parseExtensionConfigFromJson(const std::string& json);
    static std::string extensionConfigToJson(const ExtensionConfig& config);
    
    static ExtensionInfo createExtensionInfo(const ExtensionConfig& config, 
                                            unsigned int threadId, 
                                            ExtensionState state, 
                                            bool isRunning);
    static std::string extensionInfoToJson(const ExtensionInfo& info);
    static std::string extensionListToJson(const std::vector<ExtensionInfo>& extensions);
};

} // namespace URMavRouterShared
