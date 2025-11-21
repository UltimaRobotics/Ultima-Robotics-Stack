# UR-MavRouter Shared RPC Types

This library provides shared C++ types, enums, and structures for RPC requests and responses used in ur-mavrouter HTTP endpoints.

## Overview

The shared library contains three main components:

1. **ThreadRpcTypes** - Thread management RPC types
2. **ExtensionRpcTypes** - Extension management RPC types  
3. **RpcMessageTypes** - Generic RPC message handling and HTTP endpoint mapping

## Thread Management Types

### Enums

- **ThreadOperation**: START, STOP, PAUSE, RESUME, RESTART, STATUS
- **ThreadTarget**: MAINLOOP, HTTP_SERVER, STATISTICS, ALL
- **OperationStatus**: SUCCESS, FAILED, THREAD_NOT_FOUND, INVALID_OPERATION, ALREADY_IN_STATE, TIMEOUT, CONFIGURATION_ERROR, EXTENSION_ERROR

### Structures

```cpp
struct ThreadStateInfo {
    std::string threadName;
    unsigned int threadId;
    int state;
    bool isAlive;
    std::string attachmentId;
};

struct ThreadRpcRequest {
    ThreadOperation operation;
    ThreadTarget target;
    std::string threadName;
    std::map<std::string, std::string> parameters;
};

struct ThreadRpcResponse {
    OperationStatus status;
    std::string message;
    std::map<std::string, ThreadStateInfo> threadStates;
};

struct DeviceConfig {
    std::string devicePath;
    int baudrate;
};

struct MainloopStartRequest {
    DeviceConfig deviceConfig;
    bool loadExtensions;
    std::string extensionConfigDir;
};
```

## Extension Management Types

### Enums

- **ExtensionType**: TCP, UDP, SERIAL, LOGGING, FILTER, CUSTOM
- **ExtensionOperation**: ADD, REMOVE, START, STOP, STATUS, CONFIGURE
- **ExtensionState**: CREATED, RUNNING, STOPPED, ERROR, CONFIGURED

### Structures

```cpp
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
};

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
};

struct ExtensionRpcRequest {
    ExtensionOperation operation;
    std::string extensionName;
    ExtensionConfig config;
    std::map<std::string, std::string> parameters;
};

struct ExtensionRpcResponse {
    OperationStatus status;
    std::string message;
    std::vector<ExtensionInfo> extensions;
    ExtensionInfo extension;
};
```

## RPC Message Types

### Core Structures

```cpp
enum class RpcMessageType {
    REQUEST = 0,
    RESPONSE = 1,
    NOTIFICATION = 2
};

struct RpcMessage {
    std::string jsonrpc;
    RpcMessageType type;
    std::string method;
    std::string id;
};

struct RpcRequest : public RpcMessage {
    json params;
};

struct RpcResponse : public RpcMessage {
    json result;
    json error;
};
```

### HTTP Endpoint Mapping

The library provides automatic mapping between HTTP endpoints and RPC operations:

#### Thread Endpoints
- `GET /api/threads` → Get all thread status
- `GET /api/threads/mainloop` → Get mainloop thread status
- `POST /api/threads/mainloop/start` → Start mainloop thread
- `POST /api/threads/mainloop/stop` → Stop mainloop thread
- `POST /api/threads/mainloop/pause` → Pause mainloop thread
- `POST /api/threads/mainloop/resume` → Resume mainloop thread

#### Extension Endpoints
- `GET /api/extensions/status` → Get all extensions status
- `GET /api/extensions/status/{name}` → Get specific extension status
- `POST /api/extensions/add` → Add new extension
- `DELETE /api/extensions/{name}` → Remove extension
- `POST /api/extensions/start/{name}` → Start extension
- `POST /api/extensions/stop/{name}` → Stop extension

## Usage Examples

### Creating Thread RPC Requests

```cpp
#include "ur-mavrouter-shared/ThreadRpcTypes.hpp"
#include "ur-mavrouter-shared/RpcMessageTypes.hpp"

using namespace URMavRouterShared;

// Get all thread status
auto request = RpcMessageFactory::createGetAllThreadsStatus("req_001");
std::string jsonRequest = RpcMessageFactory::serializeThreadRequest(request);

// Start mainloop with device configuration
DeviceConfig device("/dev/ttyUSB0", 57600);
auto startRequest = RpcMessageFactory::createStartMainloop(device, "req_002");
std::string startJson = RpcMessageFactory::serializeThreadRequest(startRequest);
```

### Creating Extension RPC Requests

```cpp
#include "ur-mavrouter-shared/ExtensionRpcTypes.hpp"
#include "ur-mavrouter-shared/RpcMessageTypes.hpp"

using namespace URMavRouterShared;

// Add new extension
ExtensionConfig config("my_extension", ExtensionType::TCP);
config.address = "192.168.1.100";
config.port = 14550;

auto addRequest = RpcMessageFactory::createAddExtension(config, "req_003");
std::string addJson = RpcMessageFactory::serializeExtensionRequest(addRequest);

// Start existing extension
auto startRequest = RpcMessageFactory::createStartExtension("my_extension", "req_004");
std::string startJson = RpcMessageFactory::serializeExtensionRequest(startRequest);
```

### Parsing HTTP Requests to RPC

```cpp
#include "ur-mavrouter-shared/RpcMessageTypes.hpp"

using namespace URMavRouterShared;

// Convert HTTP request to RPC
std::map<std::string, std::string> httpParams;
std::string requestBody = R"({"devicePath": "/dev/ttyUSB0", "baudrate": 57600})";

auto rpcRequest = HttpEndpointMapper::httpToThreadRpc(
    "POST", 
    "/api/threads/mainloop/start", 
    httpParams, 
    requestBody
);

std::string rpcJson = RpcMessageFactory::serializeThreadRequest(rpcRequest);
```

### Creating RPC Responses

```cpp
// Create thread operation response
ThreadRpcResponse threadResponse;
threadResponse.status = OperationStatus::SUCCESS;
threadResponse.message = "Mainloop started successfully";

ThreadStateInfo mainloopInfo("mainloop", 12345, 1, true, "mainloop_attachment");
threadResponse.threadStates["mainloop"] = mainloopInfo;

auto rpcResponse = RpcMessageFactory::createThreadResponse("req_002", threadResponse);
std::string jsonResponse = RpcMessageFactory::serializeResponse(rpcResponse);
```

## JSON Message Format

### Request Format
```json
{
  "jsonrpc": "2.0",
  "method": "thread_operation",
  "params": {
    "operation": "start",
    "target": "mainloop",
    "threadName": "mainloop",
    "parameters": {
      "devicePath": "/dev/ttyUSB0",
      "baudrate": "57600"
    }
  },
  "id": "req_001"
}
```

### Response Format
```json
{
  "jsonrpc": "2.0",
  "result": {
    "status": 0,
    "message": "Operation completed successfully",
    "threads": {
      "mainloop": {
        "threadId": 12345,
        "state": "running",
        "isAlive": true,
        "attachmentId": "mainloop_attachment"
      }
    }
  },
  "id": "req_001"
}
```

## Building

```bash
cd ur-mavrouter-shared
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Installation

```bash
sudo make install
```

This will install:
- Library files to `/usr/local/lib/`
- Header files to `/usr/local/include/ur-mavrouter-shared/`
- CMake targets to `/usr/local/lib/cmake/ur-mavrouter-shared/`

## Integration

To use in your project, add to your CMakeLists.txt:

```cmake
find_package(ur-mavrouter-shared REQUIRED)

target_link_libraries(your_target 
    ur-mavrouter-shared
    # or ur-mavrouter-shared-static for static linking
)
```

## Features

- **Type Safety**: Strong typing with enums and structs
- **JSON Serialization**: Built-in JSON conversion using nlohmann/json
- **HTTP Mapping**: Automatic HTTP endpoint to RPC operation mapping
- **Error Handling**: Comprehensive error codes and messages
- **Thread Safety**: All types are designed for thread-safe usage
- **Extensibility**: Easy to add new operations and types
- **Standards Compliance**: JSON-RPC 2.0 compliant messages

## Dependencies

- C++17 or higher
- nlohmann/json library
- pthread (for threading support)

This shared library provides a solid foundation for RPC communication in ur-mavrouter, ensuring consistency across all components and enabling easy integration with external systems.
