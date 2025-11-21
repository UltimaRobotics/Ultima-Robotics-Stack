#include "RpcMessageTypes.hpp"
#include <sstream>
#include <regex>

namespace URMavRouterShared {

// Thread operation factories
ThreadRpcRequestWrapper RpcMessageFactory::createGetAllThreadsStatus(const std::string& requestId) {
    return ThreadRpcRequestWrapper(ThreadOperation::STATUS, ThreadTarget::ALL, "", requestId);
}

ThreadRpcRequestWrapper RpcMessageFactory::createGetThreadStatus(const std::string& threadName, const std::string& requestId) {
    return ThreadRpcRequestWrapper(ThreadOperation::STATUS, ThreadTarget::ALL, threadName, requestId);
}

ThreadRpcRequestWrapper RpcMessageFactory::createStartMainloop(const DeviceConfig& deviceConfig, const std::string& requestId) {
    ThreadRpcRequestWrapper wrapper(ThreadOperation::START, ThreadTarget::MAINLOOP, "mainloop", requestId);
    
    if (!deviceConfig.devicePath.empty()) {
        wrapper.threadRequest.parameters["devicePath"] = deviceConfig.devicePath;
        wrapper.threadRequest.parameters["baudrate"] = std::to_string(deviceConfig.baudrate);
    }
    
    wrapper.request = RpcRequest("thread_operation", wrapper.threadOperationToJson(), requestId);
    return wrapper;
}

ThreadRpcRequestWrapper RpcMessageFactory::createStopMainloop(const std::string& requestId) {
    return ThreadRpcRequestWrapper(ThreadOperation::STOP, ThreadTarget::MAINLOOP, "mainloop", requestId);
}

ThreadRpcRequestWrapper RpcMessageFactory::createPauseMainloop(const std::string& requestId) {
    return ThreadRpcRequestWrapper(ThreadOperation::PAUSE, ThreadTarget::MAINLOOP, "mainloop", requestId);
}

ThreadRpcRequestWrapper RpcMessageFactory::createResumeMainloop(const std::string& requestId) {
    return ThreadRpcRequestWrapper(ThreadOperation::RESUME, ThreadTarget::MAINLOOP, "mainloop", requestId);
}

// Extension operation factories
ExtensionRpcRequestWrapper RpcMessageFactory::createGetAllExtensionsStatus(const std::string& requestId) {
    ExtensionRpcRequestWrapper wrapper(ExtensionOperation::STATUS, "", requestId);
    wrapper.request = RpcRequest("extension_operation", wrapper.extensionOperationToJson(), requestId);
    return wrapper;
}

ExtensionRpcRequestWrapper RpcMessageFactory::createGetExtensionStatus(const std::string& extensionName, const std::string& requestId) {
    ExtensionRpcRequestWrapper wrapper(ExtensionOperation::STATUS, extensionName, requestId);
    wrapper.request = RpcRequest("extension_operation", wrapper.extensionOperationToJson(), requestId);
    return wrapper;
}

ExtensionRpcRequestWrapper RpcMessageFactory::createAddExtension(const ExtensionConfig& config, const std::string& requestId) {
    ExtensionRpcRequestWrapper wrapper(ExtensionOperation::ADD, config.name, requestId);
    wrapper.extensionRequest.config = config;
    wrapper.request = RpcRequest("extension_operation", wrapper.extensionOperationToJson(), requestId);
    return wrapper;
}

ExtensionRpcRequestWrapper RpcMessageFactory::createRemoveExtension(const std::string& extensionName, const std::string& requestId) {
    return ExtensionRpcRequestWrapper(ExtensionOperation::REMOVE, extensionName, requestId);
}

ExtensionRpcRequestWrapper RpcMessageFactory::createStartExtension(const std::string& extensionName, const std::string& requestId) {
    return ExtensionRpcRequestWrapper(ExtensionOperation::START, extensionName, requestId);
}

ExtensionRpcRequestWrapper RpcMessageFactory::createStopExtension(const std::string& extensionName, const std::string& requestId) {
    return ExtensionRpcRequestWrapper(ExtensionOperation::STOP, extensionName, requestId);
}

// Response factories
RpcResponse RpcMessageFactory::createThreadResponse(const std::string& requestId, const ThreadRpcResponse& threadResponse) {
    json result;
    result["status"] = static_cast<int>(threadResponse.status);
    result["message"] = threadResponse.message;
    
    if (!threadResponse.threadStates.empty()) {
        json threads = json::object();
        for (const auto& [name, state] : threadResponse.threadStates) {
            json threadInfo;
            threadInfo["threadId"] = state.threadId;
            threadInfo["state"] = ThreadTypeConverter::threadStateToString(state.state);
            threadInfo["isAlive"] = state.isAlive;
            threadInfo["attachmentId"] = state.attachmentId;
            threads[name] = threadInfo;
        }
        result["threads"] = threads;
    }
    
    return RpcResponse(requestId, result);
}

RpcResponse RpcMessageFactory::createExtensionResponse(const std::string& requestId, const ExtensionRpcResponse& extensionResponse) {
    json result;
    result["status"] = static_cast<int>(extensionResponse.status);
    result["message"] = extensionResponse.message;
    
    if (!extensionResponse.extensions.empty()) {
        json extensions = json::array();
        for (const auto& ext : extensionResponse.extensions) {
            extensions.push_back(json::parse(ExtensionTypeConverter::extensionInfoToJson(ext)));
        }
        result["extensions"] = extensions;
    }
    
    if (!extensionResponse.extension.name.empty()) {
        result["extension"] = json::parse(ExtensionTypeConverter::extensionInfoToJson(extensionResponse.extension));
    }
    
    return RpcResponse(requestId, result);
}

RpcResponse RpcMessageFactory::createErrorResponse(const std::string& requestId, int errorCode, const std::string& errorMessage) {
    return RpcResponse(requestId, errorCode, errorMessage);
}

// Parsing functions
ThreadRpcRequestWrapper RpcMessageFactory::parseThreadRequest(const std::string& jsonStr) {
    ThreadRpcRequestWrapper wrapper;
    
    try {
        json j = json::parse(jsonStr);
        
        if (j.contains("method")) {
            wrapper.request.method = j["method"].get<std::string>();
        }
        
        if (j.contains("id")) {
            wrapper.request.id = j["id"].get<std::string>();
        }
        
        if (j.contains("params")) {
            json params = j["params"];
            
            if (params.contains("operation")) {
                wrapper.threadRequest.operation = ThreadTypeConverter::stringToThreadOperation(
                    params["operation"].get<std::string>());
            }
            
            if (params.contains("target")) {
                wrapper.threadRequest.target = ThreadTypeConverter::stringToThreadTarget(
                    params["target"].get<std::string>());
            }
            
            if (params.contains("threadName")) {
                wrapper.threadRequest.threadName = params["threadName"].get<std::string>();
            }
            
            if (params.contains("parameters") && params["parameters"].is_object()) {
                for (auto& [key, value] : params["parameters"].items()) {
                    wrapper.threadRequest.parameters[key] = value.get<std::string>();
                }
            }
        }
        
        wrapper.request.params = j["params"];
        
    } catch (const std::exception& e) {
        // Return empty wrapper on parse error
        wrapper = ThreadRpcRequestWrapper();
    }
    
    return wrapper;
}

ExtensionRpcRequestWrapper RpcMessageFactory::parseExtensionRequest(const std::string& jsonStr) {
    ExtensionRpcRequestWrapper wrapper;
    
    try {
        json j = json::parse(jsonStr);
        
        if (j.contains("method")) {
            wrapper.request.method = j["method"].get<std::string>();
        }
        
        if (j.contains("id")) {
            wrapper.request.id = j["id"].get<std::string>();
        }
        
        if (j.contains("params")) {
            json params = j["params"];
            
            if (params.contains("operation")) {
                wrapper.extensionRequest.operation = ExtensionTypeConverter::stringToExtensionOperation(
                    params["operation"].get<std::string>());
            }
            
            if (params.contains("extensionName")) {
                wrapper.extensionRequest.extensionName = params["extensionName"].get<std::string>();
            }
            
            if (params.contains("config")) {
                wrapper.extensionRequest.config = ExtensionTypeConverter::parseExtensionConfigFromJson(
                    params["config"].dump());
            }
            
            if (params.contains("parameters") && params["parameters"].is_object()) {
                for (auto& [key, value] : params["parameters"].items()) {
                    wrapper.extensionRequest.parameters[key] = value.get<std::string>();
                }
            }
        }
        
        wrapper.request.params = j["params"];
        
    } catch (const std::exception& e) {
        // Return empty wrapper on parse error
        wrapper = ExtensionRpcRequestWrapper();
    }
    
    return wrapper;
}

RpcRequest RpcMessageFactory::parseGenericRequest(const std::string& jsonStr) {
    RpcRequest request;
    
    try {
        json j = json::parse(jsonStr);
        
        if (j.contains("jsonrpc")) {
            request.jsonrpc = j["jsonrpc"].get<std::string>();
        }
        
        if (j.contains("method")) {
            request.method = j["method"].get<std::string>();
        }
        
        if (j.contains("id")) {
            request.id = j["id"].get<std::string>();
        }
        
        if (j.contains("params")) {
            request.params = j["params"];
        }
        
    } catch (const std::exception& e) {
        // Return empty request on parse error
        request = RpcRequest();
    }
    
    return request;
}

// Serialization functions
std::string RpcMessageFactory::serializeRequest(const RpcRequest& request) {
    json j;
    j["jsonrpc"] = request.jsonrpc;
    j["method"] = request.method;
    
    if (!request.id.empty()) {
        j["id"] = request.id;
    }
    
    if (!request.params.empty()) {
        j["params"] = request.params;
    }
    
    return j.dump();
}

std::string RpcMessageFactory::serializeResponse(const RpcResponse& response) {
    json j;
    j["jsonrpc"] = response.jsonrpc;
    
    if (!response.id.empty()) {
        j["id"] = response.id;
    }
    
    if (!response.error.empty()) {
        j["error"] = response.error;
    } else {
        j["result"] = response.result;
    }
    
    return j.dump();
}

std::string RpcMessageFactory::serializeThreadRequest(const ThreadRpcRequestWrapper& wrapper) {
    return serializeRequest(wrapper.request);
}

std::string RpcMessageFactory::serializeExtensionRequest(const ExtensionRpcRequestWrapper& wrapper) {
    return serializeRequest(wrapper.request);
}

// HTTP endpoint mapping
std::vector<HttpEndpointMapper::EndpointMapping> HttpEndpointMapper::getThreadEndpointMappings() {
    std::vector<EndpointMapping> mappings;
    
    // GET /api/threads
    mappings.push_back({"GET", "/api/threads", "thread_operation", {}});
    
    // GET /api/threads/mainloop
    mappings.push_back({"GET", "/api/threads/mainloop", "thread_operation", {{"threadName", "mainloop"}}});
    
    // POST /api/threads/mainloop/start
    mappings.push_back({"POST", "/api/threads/mainloop/start", "thread_operation", 
                       {{"threadName", "mainloop"}, {"operation", "start"}}});
    
    // POST /api/threads/mainloop/stop
    mappings.push_back({"POST", "/api/threads/mainloop/stop", "thread_operation", 
                       {{"threadName", "mainloop"}, {"operation", "stop"}}});
    
    // POST /api/threads/mainloop/pause
    mappings.push_back({"POST", "/api/threads/mainloop/pause", "thread_operation", 
                       {{"threadName", "mainloop"}, {"operation", "pause"}}});
    
    // POST /api/threads/mainloop/resume
    mappings.push_back({"POST", "/api/threads/mainloop/resume", "thread_operation", 
                       {{"threadName", "mainloop"}, {"operation", "resume"}}});
    
    return mappings;
}

std::vector<HttpEndpointMapper::EndpointMapping> HttpEndpointMapper::getExtensionEndpointMappings() {
    std::vector<EndpointMapping> mappings;
    
    // GET /api/extensions/status
    mappings.push_back({"GET", "/api/extensions/status", "extension_operation", {}});
    
    // GET /api/extensions/status/{name}
    mappings.push_back({"GET", "/api/extensions/status/", "extension_operation", {}});
    
    // POST /api/extensions/add
    mappings.push_back({"POST", "/api/extensions/add", "extension_operation", 
                       {{"operation", "add"}}});
    
    // DELETE /api/extensions/{name}
    mappings.push_back({"DELETE", "/api/extensions/", "extension_operation", 
                       {{"operation", "remove"}}});
    
    // POST /api/extensions/start/{name}
    mappings.push_back({"POST", "/api/extensions/start/", "extension_operation", 
                       {{"operation", "start"}}});
    
    // POST /api/extensions/stop/{name}
    mappings.push_back({"POST", "/api/extensions/stop/", "extension_operation", 
                       {{"operation", "stop"}}});
    
    return mappings;
}

ThreadRpcRequestWrapper HttpEndpointMapper::httpToThreadRpc(const std::string& httpMethod, 
                                                          const std::string& httpPath, 
                                                          const std::map<std::string, std::string>& httpParams,
                                                          const std::string& requestBody) {
    
    if (httpMethod == "GET" && httpPath == "/api/threads") {
        return RpcMessageFactory::createGetAllThreadsStatus();
    }
    
    if (httpMethod == "GET" && httpPath == "/api/threads/mainloop") {
        return RpcMessageFactory::createGetThreadStatus("mainloop");
    }
    
    if (httpMethod == "POST" && httpPath == "/api/threads/mainloop/start") {
        DeviceConfig deviceConfig;
        if (!requestBody.empty()) {
            try {
                json j = json::parse(requestBody);
                if (j.contains("devicePath")) {
                    deviceConfig.devicePath = j["devicePath"].get<std::string>();
                }
                if (j.contains("baudrate")) {
                    deviceConfig.baudrate = j["baudrate"].get<int>();
                }
            } catch (const std::exception&) {
                // Use default config on parse error
            }
        }
        return RpcMessageFactory::createStartMainloop(deviceConfig);
    }
    
    if (httpMethod == "POST" && httpPath == "/api/threads/mainloop/stop") {
        return RpcMessageFactory::createStopMainloop();
    }
    
    if (httpMethod == "POST" && httpPath == "/api/threads/mainloop/pause") {
        return RpcMessageFactory::createPauseMainloop();
    }
    
    if (httpMethod == "POST" && httpPath == "/api/threads/mainloop/resume") {
        return RpcMessageFactory::createResumeMainloop();
    }
    
    // Default: return empty request
    return ThreadRpcRequestWrapper();
}

ExtensionRpcRequestWrapper HttpEndpointMapper::httpToExtensionRpc(const std::string& httpMethod, 
                                                                const std::string& httpPath, 
                                                                const std::map<std::string, std::string>& httpParams,
                                                                const std::string& requestBody) {
    
    if (httpMethod == "GET" && httpPath == "/api/extensions/status") {
        return RpcMessageFactory::createGetAllExtensionsStatus();
    }
    
    if (httpMethod == "GET" && httpPath.find("/api/extensions/status/") == 0) {
        std::string extensionName = httpPath.substr(std::string("/api/extensions/status/").length());
        return RpcMessageFactory::createGetExtensionStatus(extensionName);
    }
    
    if (httpMethod == "POST" && httpPath == "/api/extensions/add") {
        ExtensionConfig config;
        if (!requestBody.empty()) {
            config = ExtensionTypeConverter::parseExtensionConfigFromJson(requestBody);
        }
        return RpcMessageFactory::createAddExtension(config);
    }
    
    if (httpMethod == "DELETE" && httpPath.find("/api/extensions/") == 0) {
        std::string extensionName = httpPath.substr(std::string("/api/extensions/").length());
        return RpcMessageFactory::createRemoveExtension(extensionName);
    }
    
    if (httpMethod == "POST" && httpPath.find("/api/extensions/start/") == 0) {
        std::string extensionName = httpPath.substr(std::string("/api/extensions/start/").length());
        return RpcMessageFactory::createStartExtension(extensionName);
    }
    
    if (httpMethod == "POST" && httpPath.find("/api/extensions/stop/") == 0) {
        std::string extensionName = httpPath.substr(std::string("/api/extensions/stop/").length());
        return RpcMessageFactory::createStopExtension(extensionName);
    }
    
    // Default: return empty request
    return ExtensionRpcRequestWrapper();
}

} // namespace URMavRouterShared
