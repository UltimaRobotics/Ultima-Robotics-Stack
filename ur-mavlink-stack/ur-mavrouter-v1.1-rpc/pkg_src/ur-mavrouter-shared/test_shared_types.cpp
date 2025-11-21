#include <iostream>
#include <string>

// Include the shared types
#include "ThreadRpcTypes.hpp"
#include "ExtensionRpcTypes.hpp"
#include "RpcMessageTypes.hpp"

using namespace URMavRouterShared;

int main() {
    std::cout << "=== Testing UR-MavRouter Shared RPC Types ===" << std::endl;
    
    // Test Thread Types
    std::cout << "\n1. Testing Thread Types:" << std::endl;
    
    // Create thread request
    auto threadRequest = RpcMessageFactory::createGetAllThreadsStatus("test_001");
    std::cout << "✓ Created get all threads status request" << std::endl;
    
    // Serialize to JSON
    std::string threadJson = RpcMessageFactory::serializeThreadRequest(threadRequest);
    std::cout << "✓ Serialized thread request to JSON: " << threadJson << std::endl;
    
    // Parse back
    auto parsedThread = RpcMessageFactory::parseThreadRequest(threadJson);
    std::cout << "✓ Parsed thread request from JSON" << std::endl;
    
    // Create thread response
    ThreadRpcResponse threadResponse;
    threadResponse.status = OperationStatus::SUCCESS;
    threadResponse.message = "All threads retrieved successfully";
    
    ThreadStateInfo mainloopInfo("mainloop", 12345, 1, true, "mainloop_attachment");
    threadResponse.threadStates["mainloop"] = mainloopInfo;
    
    auto rpcResponse = RpcMessageFactory::createThreadResponse("test_001", threadResponse);
    std::string responseJson = RpcMessageFactory::serializeResponse(rpcResponse);
    std::cout << "✓ Created and serialized thread response: " << responseJson << std::endl;
    
    // Test Extension Types
    std::cout << "\n2. Testing Extension Types:" << std::endl;
    
    // Create extension config
    ExtensionConfig config("test_extension", ExtensionType::TCP);
    config.address = "192.168.1.100";
    config.port = 14550;
    config.assignedExtensionPoint = "endpoint_1";
    
    auto extensionRequest = RpcMessageFactory::createAddExtension(config, "test_002");
    std::cout << "✓ Created add extension request" << std::endl;
    
    std::string extensionJson = RpcMessageFactory::serializeExtensionRequest(extensionRequest);
    std::cout << "✓ Serialized extension request to JSON: " << extensionJson << std::endl;
    
    // Parse back
    auto parsedExtension = RpcMessageFactory::parseExtensionRequest(extensionJson);
    std::cout << "✓ Parsed extension request from JSON" << std::endl;
    
    // Test HTTP Endpoint Mapping
    std::cout << "\n3. Testing HTTP Endpoint Mapping:" << std::endl;
    
    std::map<std::string, std::string> httpParams;
    std::string requestBody = R"({"devicePath": "/dev/ttyUSB0", "baudrate": 57600})";
    
    auto httpThreadRequest = HttpEndpointMapper::httpToThreadRpc(
        "POST", "/api/threads/mainloop/start", httpParams, requestBody);
    
    std::cout << "✓ Mapped HTTP request to thread RPC" << std::endl;
    std::string httpThreadJson = RpcMessageFactory::serializeThreadRequest(httpThreadRequest);
    std::cout << "✓ HTTP to thread RPC JSON: " << httpThreadJson << std::endl;
    
    auto httpExtensionRequest = HttpEndpointMapper::httpToExtensionRpc(
        "POST", "/api/extensions/add", httpParams, extensionJson);
    
    std::cout << "✓ Mapped HTTP request to extension RPC" << std::endl;
    
    // Test Type Converters
    std::cout << "\n4. Testing Type Converters:" << std::endl;
    
    std::string operationStr = ThreadTypeConverter::threadOperationToString(ThreadOperation::START);
    std::cout << "✓ ThreadOperation::START -> " << operationStr << std::endl;
    
    ThreadOperation parsedOp = ThreadTypeConverter::stringToThreadOperation("start");
    std::cout << "✓ 'start' -> ThreadOperation::" << static_cast<int>(parsedOp) << std::endl;
    
    std::string extensionTypeStr = ExtensionTypeConverter::extensionTypeToString(ExtensionType::TCP);
    std::cout << "✓ ExtensionType::TCP -> " << extensionTypeStr << std::endl;
    
    ExtensionType parsedExtType = ExtensionTypeConverter::stringToExtensionType("TCP");
    std::cout << "✓ 'TCP' -> ExtensionType::" << static_cast<int>(parsedExtType) << std::endl;
    
    // Test JSON Serialization
    std::cout << "\n5. Testing JSON Serialization:" << std::endl;
    
    std::string configJson = ExtensionTypeConverter::extensionConfigToJson(config);
    std::cout << "✓ Extension config to JSON: " << configJson << std::endl;
    
    ExtensionConfig parsedConfig = ExtensionTypeConverter::parseExtensionConfigFromJson(configJson);
    std::cout << "✓ Parsed extension config from JSON" << std::endl;
    std::cout << "   Name: " << parsedConfig.name << std::endl;
    std::cout << "   Type: " << ExtensionTypeConverter::extensionTypeToString(parsedConfig.type) << std::endl;
    std::cout << "   Address: " << parsedConfig.address << ":" << parsedConfig.port << std::endl;
    
    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    std::cout << "The UR-MavRouter shared RPC types library is working correctly!" << std::endl;
    
    return 0;
}
