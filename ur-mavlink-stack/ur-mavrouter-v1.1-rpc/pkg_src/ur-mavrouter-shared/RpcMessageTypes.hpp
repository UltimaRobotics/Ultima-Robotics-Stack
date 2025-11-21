#pragma once

#include "ThreadRpcTypes.hpp"
#include "ExtensionRpcTypes.hpp"
#include "../modules/nholmann/json.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

using json = nlohmann::json;

namespace URMavRouterShared {

/**
 * @brief RPC message types
 */
enum class RpcMessageType {
    REQUEST = 0,
    RESPONSE = 1,
    NOTIFICATION = 2
};

/**
 * @brief Base RPC message structure
 */
struct RpcMessage {
    std::string jsonrpc;
    RpcMessageType type;
    std::string method;
    std::string id;
    
    RpcMessage() 
        : jsonrpc("2.0")
        , type(RpcMessageType::REQUEST)
        , method("")
        , id("") {}
    
    RpcMessage(RpcMessageType msgType, const std::string& methodName, const std::string& messageId = "")
        : jsonrpc("2.0")
        , type(msgType)
        , method(methodName)
        , id(messageId) {}
};

/**
 * @brief RPC request structure
 */
struct RpcRequest : public RpcMessage {
    json params;
    
    RpcRequest() : RpcMessage(RpcMessageType::REQUEST, "") {}
    RpcRequest(const std::string& methodName, const json& requestParams = {}, const std::string& messageId = "")
        : RpcMessage(RpcMessageType::REQUEST, methodName, messageId), params(requestParams) {}
};

/**
 * @brief RPC response structure
 */
struct RpcResponse : public RpcMessage {
    json result;
    json error;
    
    RpcResponse() : RpcMessage(RpcMessageType::RESPONSE, "") {}
    RpcResponse(const std::string& messageId, const json& responseResult = {})
        : RpcMessage(RpcMessageType::RESPONSE, "", messageId), result(responseResult) {}
    
    RpcResponse(const std::string& messageId, int errorCode, const std::string& errorMessage)
        : RpcMessage(RpcMessageType::RESPONSE, "", messageId) {
        error = json{
            {"code", errorCode},
            {"message", errorMessage}
        };
    }
};

/**
 * @brief Thread-specific RPC requests
 */
struct ThreadRpcRequestWrapper {
    RpcRequest request;
    ThreadRpcRequest threadRequest;
    
    ThreadRpcRequestWrapper() {}
    
    ThreadRpcRequestWrapper(ThreadOperation operation, ThreadTarget target, 
                           const std::string& threadName = "", const std::string& requestId = "") {
        threadRequest = ThreadRpcRequest(operation, target, threadName);
        request = RpcRequest("thread_operation", threadOperationToJson(), requestId);
    }
    
    json threadOperationToJson() const {
        json j;
        j["operation"] = ThreadTypeConverter::threadOperationToString(threadRequest.operation);
        j["target"] = ThreadTypeConverter::threadTargetToString(threadRequest.target);
        
        if (!threadRequest.threadName.empty()) {
            j["threadName"] = threadRequest.threadName;
        }
        
        if (!threadRequest.parameters.empty()) {
            j["parameters"] = threadRequest.parameters;
        }
        
        return j;
    }
};

/**
 * @brief Extension-specific RPC requests
 */
struct ExtensionRpcRequestWrapper {
    RpcRequest request;
    ExtensionRpcRequest extensionRequest;
    
    ExtensionRpcRequestWrapper() {}
    
    ExtensionRpcRequestWrapper(ExtensionOperation operation, const std::string& extensionName = "", 
                              const std::string& requestId = "") {
        extensionRequest = ExtensionRpcRequest(operation, extensionName);
        request = RpcRequest("extension_operation", extensionOperationToJson(), requestId);
    }
    
    json extensionOperationToJson() const {
        json j;
        j["operation"] = ExtensionTypeConverter::extensionOperationToString(extensionRequest.operation);
        
        if (!extensionRequest.extensionName.empty()) {
            j["extensionName"] = extensionRequest.extensionName;
        }
        
        if (!extensionRequest.parameters.empty()) {
            j["parameters"] = extensionRequest.parameters;
        }
        
        return j;
    }
};

/**
 * @brief RPC request/response factory and parser
 */
class RpcMessageFactory {
public:
    // Thread operation factories
    static ThreadRpcRequestWrapper createGetAllThreadsStatus(const std::string& requestId = "");
    static ThreadRpcRequestWrapper createGetThreadStatus(const std::string& threadName, const std::string& requestId = "");
    static ThreadRpcRequestWrapper createStartMainloop(const DeviceConfig& deviceConfig = {}, const std::string& requestId = "");
    static ThreadRpcRequestWrapper createStopMainloop(const std::string& requestId = "");
    static ThreadRpcRequestWrapper createPauseMainloop(const std::string& requestId = "");
    static ThreadRpcRequestWrapper createResumeMainloop(const std::string& requestId = "");
    
    // Extension operation factories
    static ExtensionRpcRequestWrapper createGetAllExtensionsStatus(const std::string& requestId = "");
    static ExtensionRpcRequestWrapper createGetExtensionStatus(const std::string& extensionName, const std::string& requestId = "");
    static ExtensionRpcRequestWrapper createAddExtension(const ExtensionConfig& config, const std::string& requestId = "");
    static ExtensionRpcRequestWrapper createRemoveExtension(const std::string& extensionName, const std::string& requestId = "");
    static ExtensionRpcRequestWrapper createStartExtension(const std::string& extensionName, const std::string& requestId = "");
    static ExtensionRpcRequestWrapper createStopExtension(const std::string& extensionName, const std::string& requestId = "");
    
    // Response factories
    static RpcResponse createThreadResponse(const std::string& requestId, const ThreadRpcResponse& threadResponse);
    static RpcResponse createExtensionResponse(const std::string& requestId, const ExtensionRpcResponse& extensionResponse);
    static RpcResponse createErrorResponse(const std::string& requestId, int errorCode, const std::string& errorMessage);
    
    // Parsing functions
    static ThreadRpcRequestWrapper parseThreadRequest(const std::string& jsonStr);
    static ExtensionRpcRequestWrapper parseExtensionRequest(const std::string& jsonStr);
    static RpcRequest parseGenericRequest(const std::string& jsonStr);
    
    // Serialization functions
    static std::string serializeRequest(const RpcRequest& request);
    static std::string serializeResponse(const RpcResponse& response);
    static std::string serializeThreadRequest(const ThreadRpcRequestWrapper& wrapper);
    static std::string serializeExtensionRequest(const ExtensionRpcRequestWrapper& wrapper);
};

/**
 * @brief HTTP endpoint to RPC operation mapping
 */
class HttpEndpointMapper {
public:
    struct EndpointMapping {
        std::string httpMethod;
        std::string httpPath;
        std::string rpcMethod;
        std::map<std::string, std::string> parameterMapping;
    };
    
    static std::vector<EndpointMapping> getThreadEndpointMappings();
    static std::vector<EndpointMapping> getExtensionEndpointMappings();
    
    static ThreadRpcRequestWrapper httpToThreadRpc(const std::string& httpMethod, 
                                                  const std::string& httpPath, 
                                                  const std::map<std::string, std::string>& httpParams,
                                                  const std::string& requestBody = "");
    
    static ExtensionRpcRequestWrapper httpToExtensionRpc(const std::string& httpMethod, 
                                                        const std::string& httpPath, 
                                                        const std::map<std::string, std::string>& httpParams,
                                                        const std::string& requestBody = "");
};

} // namespace URMavRouterShared
