# Comprehensive UR-RPC Template Integration Guide for C++ Applications

## Table of Contents

1. [Overview](#overview)
2. [Architecture Components](#architecture-components)
3. [Core Integration Patterns](#core-integration-patterns)
4. [RPC Client Implementation](#rpc-client-implementation)
5. [Operation Processing Architecture](#operation-processing-architecture)
6. [Message Handling Patterns](#message-handling-patterns)
7. [Thread Management Integration](#thread-management-integration)
8. [Configuration Management](#configuration-management)
9. [Request/Response Lifecycle](#requestresponse-lifecycle)
10. [Error Handling and Resilience](#error-handling-and-resilience)
11. [Security Considerations](#security-considerations)
12. [Performance Optimization](#performance-optimization)
13. [Complete Implementation Examples](#complete-implementation-examples)
14. [Best Practices](#best-practices)
15. [Troubleshooting and Debugging](#troubleshooting-and-debugging)
16. [Advanced Patterns](#advanced-patterns)
17. [Testing Strategies](#testing-strategies)
18. [Deployment Considerations](#deployment-considerations)
19. [Migration Guide](#migration-guide)
20. [Reference Implementation](#reference-implementation)

---

## Overview

This comprehensive guide explains how to integrate the **UR-RPC Template** into C++ applications using the proven patterns from ur-licence-mann. The UR-RPC framework provides a robust, thread-safe foundation for MQTT-based RPC communication with modern C++ interfaces, proper resource management, and scalable architecture.

### Key Benefits

- **Thread-safe RPC communication** over MQTT with automatic reconnection
- **Modern C++ API** with smart pointers, RAII, and exception safety
- **Asynchronous message processing** with configurable thread pools
- **Scalable operation handling** with concurrent request processing
- **Flexible topic subscription** and message routing patterns
- **JSON-based configuration** for easy deployment and management
- **Built-in error handling** and recovery mechanisms
- **Performance monitoring** and statistics collection

### Integration Philosophy

The UR-RPC Template follows a **layered architecture** approach:

1. **Transport Layer**: MQTT-based communication with mosquitto
2. **Protocol Layer**: JSON-RPC 2.0 compliant message format
3. **Processing Layer**: Asynchronous operation execution
4. **Application Layer**: Business logic implementation

This separation ensures **clean concerns**, **testability**, and **maintainability** while providing the flexibility to adapt to various application requirements.

---

## Architecture Components

### 1. Core RPC Client (`rpc_client.hpp/cpp`)

The RPC client serves as the **foundation** for all RPC communication:

```cpp
class RpcClient {
public:
    // Constructor with configuration path and client identifier
    RpcClient(const std::string& configPath, const std::string& clientId);
    
    // Lifecycle management
    bool start();
    void stop();
    bool isRunning() const;
    
    // Message handling configuration
    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler);
    
    // Response transmission
    void sendResponse(const std::string& topic, const std::string& response);
    
private:
    // Thread management
    std::unique_ptr<ThreadMgr::ThreadManager> threadManager_;
    unsigned int rpcThreadId_{0};
    
    // Internal state
    std::atomic<bool> running_{false};
    std::function<void(const std::string&, const std::string&)> messageHandler_;
    
    // Core thread function
    void rpcClientThreadFunc();
    
    // Static callback for C interoperability
    static void staticMessageHandler(const char* topic, const char* payload, 
                                   size_t payload_len, void* user_data);
};
```

**Key Design Principles:**

- **Thread Safety**: All public methods are thread-safe using atomic operations
- **Resource Management**: RAII pattern ensures proper cleanup
- **Callback Flexibility**: Configurable message handlers for different use cases
- **Error Resilience**: Comprehensive error handling and recovery

### 2. Operation Processor (`rpc_operation_processor.hpp/cpp`)

The operation processor handles **concurrent request processing**:

```cpp
class RpcOperationProcessor {
public:
    // Constructor with configuration and verbosity settings
    RpcOperationProcessor(const PackageConfig& config, bool verbose);
    ~RpcOperationProcessor();
    
    // Core processing interface
    void processRequest(const char* payload, size_t payload_len);
    void setResponseTopic(const std::string& topic);
    
private:
    // Thread management for concurrent operations
    std::shared_ptr<ThreadMgr::ThreadManager> threadManager_;
    std::set<unsigned int> activeThreads_;
    std::mutex threadsMutex_;
    std::atomic<bool> isShuttingDown_{false};
    
    // Request context for thread-safe data passing
    struct RequestContext {
        std::string requestJson;
        std::string transactionId;
        std::string responseTopic;
        std::shared_ptr<const PackageConfig> config;
        bool verbose;
        // Thread synchronization primitives
        std::shared_ptr<std::promise<unsigned int>> threadIdPromise;
        std::shared_future<unsigned int> threadIdFuture;
    };
    
    // Processing methods
    void processOperationThread(std::shared_ptr<RequestContext> context);
    static void processOperationThreadStatic(std::shared_ptr<RequestContext> context);
    
    // Response handling
    void sendResponse(const std::string& transactionId, bool success, 
                      const std::string& result, const std::string& error = "");
    static void sendResponseStatic(const std::string& transactionId, bool success,
                                   const std::string& result, const std::string& error,
                                   const std::string& responseTopic);
};
```

**Advanced Features:**

- **Concurrent Processing**: Thread pool for handling multiple requests simultaneously
- **Memory Safety**: Shared pointers prevent data corruption during concurrent access
- **Graceful Shutdown**: Proper thread cleanup and resource deallocation
- **Performance Monitoring**: Thread tracking and statistics collection

### 3. Operation Handler (`operation_handler.hpp/cpp`)

The operation handler implements **business logic execution**:

```cpp
class OperationHandler {
public:
    // Main execution interface
    static int execute(const OperationConfig& op_config, 
                      const PackageConfig& pkg_config, bool verbose);
    
private:
    // Specific operation handlers
    static int handle_verify(const std::map<std::string, std::string>& params, 
                            const PackageConfig& pkg_config, bool verbose);
    static int handle_update(const std::map<std::string, std::string>& params, 
                            const PackageConfig& pkg_config, bool verbose);
    static int handle_get_license_info(const std::map<std::string, std::string>& params, 
                                      const PackageConfig& pkg_config, bool verbose);
    // ... additional operation handlers
};
```

**Design Patterns:**

- **Strategy Pattern**: Different handlers for various operation types
- **Static Methods**: Stateless execution for thread safety
- **Configuration Injection**: Dependency injection for testability
- **Result Standardization**: Consistent return codes and error handling

---

## Core Integration Patterns

### 1. Initialization Pattern

The **three-phase initialization** ensures robust startup:

```cpp
// Phase 1: Configuration Loading
PackageConfig pkg_config = load_package_config(package_config_path, verbose);

// Phase 2: Component Initialization
if (!InitManager::initialize(pkg_config, verbose)) {
    std::cerr << "Initialization failed" << std::endl;
    return 1;
}

// Phase 3: RPC Client Setup
g_rpcClient = std::make_shared<RpcClient>(rpc_config_path, "application-name");
g_operationProcessor = std::make_unique<RpcOperationProcessor>(pkg_config, verbose);
```

### 2. Message Handler Registration Pattern

**Handler registration before client start** is critical:

```cpp
// CRITICAL: Set message handler BEFORE starting the client
g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
    // Topic filtering for selective processing
    if (topic.find("direct_messaging/application-name/requests") == std::string::npos) {
        return;
    }
    
    // Delegate to operation processor
    if (g_operationProcessor) {
        g_operationProcessor->processRequest(payload.c_str(), payload.size());
    }
});

// Start client after handler registration
if (!g_rpcClient->start()) {
    std::cerr << "Failed to start RPC client" << std::endl;
    return 1;
}
```

### 3. Graceful Shutdown Pattern

**Coordinated shutdown** ensures resource cleanup:

```cpp
// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "Caught signal " << signal << ", shutting down..." << std::endl;
    
    // Stop RPC client first
    if (g_rpcClient) {
        g_rpcClient->stop();
    }
    
    // Set global shutdown flag
    g_running.store(false);
}

// Main loop with shutdown monitoring
while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// Final cleanup
g_rpcClient->stop();
```

---

## RPC Client Implementation

### 1. Constructor and Initialization

```cpp
RpcClient::RpcClient(const std::string &configPath, const std::string &clientId)
    : configPath_(configPath), clientId_(clientId) {
    // Initialize thread manager with configurable pool size
    threadManager_ = std::make_unique<ThreadMgr::ThreadManager>(10);
}
```

**Key Considerations:**

- **Configuration Path**: Must be valid JSON with RPC settings
- **Client ID**: Unique identifier for MQTT connection
- **Thread Pool**: Size based on expected concurrency

### 2. Thread Management

```cpp
bool RpcClient::start() {
    if (running_.load()) {
        return true; // Already running
    }

    try {
        // Create RPC client thread using ThreadManager
        rpcThreadId_ = threadManager_->createThread([this]() {
            this->rpcClientThreadFunc();
        });

        // Wait for thread initialization with timeout
        const int MAX_WAIT_MS = 3000;
        const int POLL_INTERVAL_MS = 100;
        int elapsed = 0;
        
        while (elapsed < MAX_WAIT_MS && !running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
            elapsed += POLL_INTERVAL_MS;
        }

        return running_.load();
        
    } catch (const std::exception &e) {
        std::cerr << "start() failed: " << e.what() << std::endl;
        return false;
    }
}
```

**Thread Safety Features:**

- **Atomic State Management**: Thread-safe running flag
- **Timeout Protection**: Prevents indefinite blocking
- **Exception Handling**: Catches and reports thread creation failures

### 3. Core Thread Function

```cpp
void RpcClient::rpcClientThreadFunc() {
    try {
        // Validate message handler is set
        if (!messageHandler_) {
            std::cerr << "ERROR: No message handler set!" << std::endl;
            running_.store(false);
            return;
        }

        // Create thread context for UR-RPC template
        direct_client_thread_t* ctx = direct_client_thread_create(configPath_.c_str());
        
        if (!ctx) {
            std::cerr << "Failed to create client thread context" << std::endl;
            running_.store(false);
            return;
        }

        // Set message handler BEFORE starting the thread
        direct_client_set_message_handler(ctx, staticMessageHandler, this);

        // Start the client thread
        if (direct_client_thread_start(ctx) != 0) {
            std::cerr << "Failed to start client thread" << std::endl;
            direct_client_thread_destroy(ctx);
            running_.store(false);
            return;
        }

        // Wait for connection establishment
        if (!direct_client_thread_wait_for_connection(ctx, 10000)) {
            std::cerr << "Connection timeout" << std::endl;
            direct_client_thread_stop(ctx);
            direct_client_thread_destroy(ctx);
            running_.store(false);
            return;
        }

        running_.store(true);

        // Main thread loop
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Cleanup
        direct_client_thread_stop(ctx);
        direct_client_thread_destroy(ctx);
        
    } catch (const std::exception &e) {
        std::cerr << "Thread function error: " << e.what() << std::endl;
        running_.store(false);
    }
}
```

**Critical Implementation Details:**

- **Handler Validation**: Ensures message handler is set before connection
- **Context Management**: Proper creation and cleanup of UR-RPC context
- **Connection Waiting**: Blocks until connection is established
- **Exception Safety**: Comprehensive error handling throughout

### 4. Static Message Handler

```cpp
void RpcClient::staticMessageHandler(const char *topic, const char *payload,
                                     size_t payload_len, void *user_data) {
    RpcClient *self = static_cast<RpcClient *>(user_data);
    if (!self || !self->messageHandler_) {
        return;
    }

    // Convert C strings to C++ strings safely
    const std::string topicStr(topic ? topic : "");
    const std::string payloadStr(payload ? payload : "", payload_len);

    // Delegate to instance handler
    self->messageHandler_(topicStr, payloadStr);
}
```

**C/C++ Interoperability:**

- **Static Function**: Required for C callback interface
- **User Data Passing**: Uses void* for instance access
- **Safe String Conversion**: Handles null pointers and length
- **Exception Propagation**: Errors caught in instance handler

---

## Operation Processing Architecture

### 1. Request Processing Flow

```cpp
void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    // Input validation
    if (!payload || payload_len == 0) {
        std::cerr << "Empty payload received" << std::endl;
        return;
    }

    // Size validation (prevent memory exhaustion)
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        std::cerr << "Payload too large: " << payload_len << " bytes" << std::endl;
        return;
    }

    try {
        // JSON parsing
        json root = json::parse(payload, payload + payload_len);

        // JSON-RPC 2.0 validation
        if (!root.contains("jsonrpc") || root["jsonrpc"].get<std::string>() != "2.0") {
            std::cerr << "Invalid or missing JSON-RPC version" << std::endl;
            return;
        }

        // Extract transaction ID
        std::string transactionId = extractTransactionId(root);

        // Extract method
        if (!root.contains("method") || !root["method"].is_string()) {
            sendResponse(transactionId, false, "", "Missing method in request");
            return;
        }
        std::string method = root["method"].get<std::string>();

        // Extract parameters
        if (!root.contains("params") || !root["params"].is_object()) {
            sendResponse(transactionId, false, "", "Missing or invalid params in request");
            return;
        }

        // Create processing context
        auto context = createContext(root, transactionId);

        // Launch asynchronous processing
        launchProcessingThread(context);

    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception processing request: " << e.what() << std::endl;
    }
}
```

### 2. Thread-Safe Context Creation

```cpp
auto context = std::make_shared<RequestContext>(
    requestJson,           // Request data
    transactionId,         // Transaction identifier
    responseTopic_,        // Response topic
    config_,              // Shared configuration (immutable)
    verbose_,             // Verbosity setting
    threadManager,        // Thread manager reference
    &activeThreads_,      // Active threads tracking
    &threadsMutex_        // Thread synchronization
);
```

**Context Design Principles:**

- **Shared Ownership**: Uses shared_ptr for safe concurrent access
- **Immutable Data**: Configuration is shared as const to prevent corruption
- **Thread Synchronization**: Includes mutex and thread tracking
- **Resource Safety**: Automatic cleanup when threads complete

### 3. Asynchronous Thread Launch

```cpp
// Check shutdown state
bool shuttingDown = isShuttingDown_.load();
if (shuttingDown) {
    sendResponse(transactionId, false, "", "Server is shutting down");
    return;
}

try {
    // Create processing thread
    unsigned int threadId = threadMgr->createThread([context]() {
        RpcOperationProcessor::processOperationThreadStatic(context);
    });
    
    // Register thread in tracking set
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        activeThreads_.insert(threadId);
    }
    
    // Set thread ID for worker access
    context->threadId.store(threadId);
    context->threadIdPromise->set_value(threadId);

} catch (const std::exception& e) {
    std::cerr << "Failed to create thread: " << e.what() << std::endl;
    
    // Fallback to synchronous processing
    context->threadIdPromise->set_value(0);
    RpcOperationProcessor::processOperationThreadStatic(context);
}
```

**Thread Launch Features:**

- **Shutdown Awareness**: Prevents new thread creation during shutdown
- **Thread Tracking**: Maintains registry of active threads
- **Promise/Future Sync**: Coordinates thread ID sharing
- **Fallback Strategy**: Synchronous processing if thread creation fails

### 4. Thread-Safe Operation Execution

```cpp
void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    // Wait for thread ID synchronization
    unsigned int threadId = context->threadIdFuture.get();
    
    // Extract context data safely
    const std::string& requestJson = context->requestJson;
    const std::string& transactionId = context->transactionId;
    std::shared_ptr<const PackageConfig> config = context->config;
    bool verbose = context->verbose;
    
    try {
        // Parse request in thread context
        json root = json::parse(requestJson);
        
        // Extract method and parameters
        std::string method = root["method"].get<std::string>();
        json paramsObj = root["params"];
        
        // Build operation configuration
        OperationConfig opConfig = buildOperationConfig(method, paramsObj);
        
        // Execute operation with output capture if needed
        int exitCode = executeOperation(opConfig, *config, verbose);
        
        // Send response based on execution result
        handleOperationResult(exitCode, transactionId, verbose);
        
    } catch (const std::exception& e) {
        sendResponseStatic(transactionId, false, "", 
                          std::string("Exception: ") + e.what(), 
                          context->responseTopic);
    }

    // Cleanup thread from tracking
    cleanupThreadTracking(threadId, context);
}
```

---

## Message Handling Patterns

### 1. Topic-Based Routing

```cpp
// Message handler with topic filtering
g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
    // Application-specific topic filtering
    if (topic.find("direct_messaging/my-app/requests") == std::string::npos) {
        return; // Ignore unrelated topics
    }
    
    // Extract topic components for routing
    std::vector<std::string> topicParts = splitTopic(topic);
    
    if (topicParts.size() >= 4) {
        std::string messageType = topicParts[3];
        
        if (messageType == "requests") {
            // Handle RPC requests
            g_operationProcessor->processRequest(payload.c_str(), payload.size());
        } else if (messageType == "control") {
            // Handle control messages
            handleControlMessage(payload);
        }
    }
});
```

### 2. Message Validation

```cpp
bool validateRpcMessage(const std::string& payload) {
    try {
        json message = json::parse(payload);
        
        // Required JSON-RPC 2.0 fields
        if (!message.contains("jsonrpc") || 
            message["jsonrpc"].get<std::string>() != "2.0") {
            return false;
        }
        
        if (!message.contains("method") || !message["method"].is_string()) {
            return false;
        }
        
        if (!message.contains("params") || !message["params"].is_object()) {
            return false;
        }
        
        // Optional transaction ID
        if (message.contains("id")) {
            return message["id"].is_string() || message["id"].is_number();
        }
        
        return true;
        
    } catch (const json::parse_error&) {
        return false;
    }
}
```

### 3. Response Formatting

```cpp
void sendResponseStatic(const std::string& transactionId, bool success,
                       const std::string& result, const std::string& error,
                       const std::string& responseTopic) {
    try {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = transactionId;

        if (success) {
            // Handle JSON result parsing
            if (!result.empty() && result[0] == '{') {
                try {
                    json parsedResult = json::parse(result);
                    response["result"] = parsedResult;
                } catch (const json::parse_error&) {
                    response["result"] = result;
                }
            } else if (!result.empty()) {
                response["result"] = result;
            } else {
                response["result"] = "Operation completed successfully";
            }
        } else {
            // Error response format
            json errorObj;
            errorObj["code"] = -1;
            errorObj["message"] = error;
            response["error"] = errorObj;
        }

        // Publish response
        std::string responseJson = response.dump();
        direct_client_publish_raw_message(responseTopic.c_str(), 
                                         responseJson.c_str(), 
                                         responseJson.size());

    } catch (const std::exception& e) {
        std::cerr << "Failed to send response: " << e.what() << std::endl;
    }
}
```

---

## Thread Management Integration

### 1. ThreadManager Configuration

```cpp
// Initialize ThreadManager with appropriate pool size
threadManager_ = std::make_shared<ThreadMgr::ThreadManager>(100);

// ThreadManager provides:
// - Thread creation with automatic cleanup
// - Thread lifecycle management
// - Resource tracking and statistics
// - Exception-safe thread execution
```

### 2. Thread Lifecycle Management

```cpp
~RpcOperationProcessor() {
    // Set shutdown flag to prevent new thread creation
    isShuttingDown_.store(true);
    
    // Collect all active threads for joining
    std::vector<unsigned int> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        threadsToJoin.assign(activeThreads_.begin(), activeThreads_.end());
    }
    
    // Join all threads with timeout
    for (unsigned int threadId : threadsToJoin) {
        if (threadManager_->isThreadAlive(threadId)) {
            bool completed = threadManager_->joinThread(threadId, 
                                                       std::chrono::minutes(5));
            if (!completed) {
                std::cerr << "WARNING: Thread " << threadId 
                         << " did not complete after 5 minutes" << std::endl;
            }
        }
    }
}
```

### 3. Thread Cleanup Strategy

```cpp
void cleanupCompletedThreads() {
    if (!threadManager_) {
        return;
    }
    
    std::vector<unsigned int> threadsToClean;
    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        
        // Find completed threads
        for (unsigned int threadId : activeThreads_) {
            if (!threadManager_->isThreadAlive(threadId)) {
                threadsToClean.push_back(threadId);
            }
        }
        
        // Remove from tracking
        for (unsigned int threadId : threadsToClean) {
            activeThreads_.erase(threadId);
        }
    }
    
    // Log cleanup statistics
    if (!threadsToClean.empty()) {
        std::cout << "Cleaned up " << threadsToClean.size() 
                  << " completed threads" << std::endl;
    }
}
```

---

## Configuration Management

### 1. Package Configuration Structure

```cpp
struct PackageConfig {
    // File paths
    std::string private_key_file = "private.pem";
    std::string public_key_file = "public.pem";
    std::string license_definitions_file = "license_definitions.json";
    std::string encrypted_license_definitions_file = "license_definitions.json.enc";
    
    // Security settings
    bool require_hardware_binding = true;
    bool auto_encrypt_licenses = true;
    bool auto_encrypt_definitions = true;
    
    // Serialization support
    static PackageConfig from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};
```

### 2. Configuration Loading Pattern

```cpp
PackageConfig load_package_config(const std::string &config_path, bool verbose) {
    PackageConfig config;

    std::ifstream file(config_path);
    if (!file.good()) {
        if (verbose) {
            std::cout << "Package config file not found, using defaults" << std::endl;
        }
        return config;
    }

    try {
        nlohmann::json j;
        file >> j;
        config = PackageConfig::from_json(j);
        if (verbose) {
            std::cout << "Loaded package config from: " << config_path << std::endl;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error loading package config: " << e.what() << std::endl;
    }

    return config;
}
```

### 3. RPC Configuration

The UR-RPC template uses a generic JSON configuration file that defines client connection parameters, topic subscriptions, and heartbeat settings:

```json
{
  "client_id": "ur-licence-mann",
  "broker_host": "127.0.0.1",
  "broker_port": 1899,
  "keepalive": 60,
  "qos": 0,
  "auto_reconnect": true,
  "reconnect_delay_min": 1,
  "reconnect_delay_max": 60,
  "use_tls": false,
  "heartbeat": {
    "enabled": true,
    "interval_seconds": 5,
    "topic": "clients/ur-licence-mann/heartbeat",
    "payload": "{\"client\":\"ur-licence-mann\",\"status\":\"alive\"}"
  },
  "json_added_pubs": {
    "topics": [
      "direct_messaging/ur-licence-mann/responses"
    ]
  },
  "json_added_subs": {
    "topics": [
      "direct_messaging/ur-licence-mann/requests"
    ]
  }
}
```

**Configuration Fields:**

- **client_id**: Unique identifier for the MQTT client connection
- **broker_host**: MQTT broker hostname or IP address
- **broker_port**: MQTT broker port number
- **keepalive**: Keep-alive interval in seconds for MQTT connection
- **qos**: Quality of Service level (0, 1, or 2)
- **auto_reconnect**: Enable automatic reconnection on connection loss
- **reconnect_delay_min/ max**: Minimum and maximum delay between reconnection attempts
- **use_tls**: Enable TLS encryption for MQTT connection
- **heartbeat**: Configuration for periodic heartbeat messages
- **json_added_pubs**: Topics that the client will publish to
- **json_added_subs**: Topics that the client will subscribe to

---

## Request/Response Lifecycle

### 1. Request Reception

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│   MQTT Broker   │───▶│  RpcClient       │───▶│  Message Handler     │
│                 │    │                  │    │                     │
│ Topic:          │    │ staticMessage    │    │ Lambda Function      │
│ direct_messaging│    │ Handler()         │    │ filters & routes     │
│ /app/requests   │    │                  │    │                     │
└─────────────────┘    └──────────────────┘    └─────────────────────┘
```

### 2. Request Processing

```
┌─────────────────────┐    ┌──────────────────────┐    ┌─────────────────┐
│  Message Handler     │───▶│ Operation Processor  │───▶│ Thread Pool     │
│                     │    │                      │    │                 │
│ Topic filtering     │    │ JSON validation      │    │ Concurrent      │
│ Payload extraction   │    │ Context creation      │    │ execution       │
│                     │    │ Thread launch         │    │                 │
└─────────────────────┘    └──────────────────────┘    └─────────────────┘
```

### 3. Operation Execution

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│  Worker Thread   │───▶│ Operation Handler│───▶│ Business Logic      │
│                 │    │                  │    │                     │
│ Static method   │    │ Method routing   │    │ Domain-specific     │
│ Context access  │    │ Parameter        │    │ operations          │
│                 │    │ extraction        │    │                     │
└─────────────────┘    └──────────────────┘    └─────────────────────┘
```

### 4. Response Transmission

```
┌─────────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  Operation Handler  │───▶│ Response Builder │───▶│  MQTT Broker    │
│                     │    │                  │    │                 │
│ Result formatting   │    │ JSON-RPC 2.0     │    │ Topic:          │
│ Error handling      │    │ response         │    │ direct_messaging│
│                     │    │                  │    │ /app/responses  │
└─────────────────────┘    └──────────────────┘    └─────────────────┘
```

---

## Error Handling and Resilience

### 1. Connection Error Handling

```cpp
void RpcClient::rpcClientThreadFunc() {
    try {
        // Connection establishment with timeout
        if (!direct_client_thread_wait_for_connection(ctx, 10000)) {
            std::cerr << "Connection timeout" << std::endl;
            // Cleanup and return
            direct_client_thread_stop(ctx);
            direct_client_thread_destroy(ctx);
            running_.store(false);
            return;
        }

        running_.store(true);
        
        // Main loop with error monitoring
        while (running_.load()) {
            if (!direct_client_thread_is_connected(ctx)) {
                std::cerr << "Connection lost, attempting reconnection..." << std::endl;
                // Reconnection is handled automatically by UR-RPC template
                if (!direct_client_thread_wait_for_connection(ctx, 5000)) {
                    std::cerr << "Reconnection failed, shutting down" << std::endl;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

    } catch (const std::exception &e) {
        std::cerr << "Thread function error: " << e.what() << std::endl;
        running_.store(false);
    }
}
```

### 2. Memory Safety

```cpp
void RpcOperationProcessor::processRequest(const char* payload, size_t payload_len) {
    // Input validation to prevent memory exhaustion
    const size_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    if (payload_len > MAX_PAYLOAD_SIZE) {
        std::cerr << "Payload too large: " << payload_len << " bytes" << std::endl;
        return;
    }

    try {
        // Use RAII and smart pointers throughout
        auto context = std::make_shared<RequestContext>(
            /* parameters with automatic cleanup */
        );

        // Exception-safe thread creation
        unsigned int threadId = threadManager_->createThread([context]() {
            try {
                processOperationThreadStatic(context);
            } catch (const std::bad_alloc& e) {
                std::cerr << "Memory allocation failed in worker thread" << std::endl;
                sendResponseStatic(context->transactionId, false, "", 
                                  "Server error - out of memory", 
                                  context->responseTopic);
            }
        });

    } catch (const std::bad_alloc& e) {
        std::cerr << "CRITICAL: std::bad_alloc caught: " << e.what() << std::endl;
        // Send error response if possible
    }
}
```

### 3. Thread Error Recovery

```cpp
void RpcOperationProcessor::processOperationThreadStatic(std::shared_ptr<RequestContext> context) {
    try {
        // Main processing logic
        executeOperation(context);
        
    } catch (const std::bad_alloc& e) {
        std::cerr << "CRITICAL: Memory allocation failed: " << e.what() << std::endl;
        sendResponseStatic(context->transactionId, false, "", 
                          "Server error - out of memory", 
                          context->responseTopic);
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parsing failed: " << e.what() << std::endl;
        sendResponseStatic(context->transactionId, false, "", 
                          "Invalid JSON format", 
                          context->responseTopic);
    } catch (const std::exception& e) {
        std::cerr << "General exception: " << e.what() << std::endl;
        sendResponseStatic(context->transactionId, false, "", 
                          std::string("Exception: ") + e.what(), 
                          context->responseTopic);
    }

    // Ensure thread cleanup regardless of errors
    if (context->activeThreads && context->threadsMutex) {
        std::lock_guard<std::mutex> lock(*context->threadsMutex);
        context->activeThreads->erase(context->threadId.load());
    }
}
```

---

## Security Considerations

### 1. Input Validation

```cpp
bool validateInputParameters(const std::map<std::string, std::string>& params) {
    // Check for dangerous parameter values
    for (const auto& [key, value] : params) {
        // Path traversal protection
        if (value.find("..") != std::string::npos) {
            std::cerr << "Potentially dangerous path in parameter: " << key << std::endl;
            return false;
        }
        
        // Command injection protection
        if (value.find(";") != std::string::npos || 
            value.find("|") != std::string::npos ||
            value.find("&") != std::string::npos) {
            std::cerr << "Potentially dangerous characters in parameter: " << key << std::endl;
            return false;
        }
        
        // Length limits
        if (value.length() > 4096) {
            std::cerr << "Parameter too long: " << key << std::endl;
            return false;
        }
    }
    return true;
}
```

### 2. Topic Security

```cpp
bool isAuthorizedTopic(const std::string& topic, const std::string& clientId) {
    // Verify topic belongs to this client
    std::string expectedPrefix = "direct_messaging/" + clientId + "/";
    
    if (topic.find(expectedPrefix) != 0) {
        std::cerr << "Unauthorized topic access: " << topic << std::endl;
        return false;
    }
    
    // Additional topic validation rules
    std::vector<std::string> allowedSuffixes = {
        "/requests", "/responses", "/heartbeat", "/control"
    };
    
    for (const auto& suffix : allowedSuffixes) {
        if (topic.length() >= suffix.length() &&
            topic.substr(topic.length() - suffix.length()) == suffix) {
            return true;
        }
    }
    
    return false;
}
```

### 3. Rate Limiting

```cpp
class RateLimiter {
private:
    std::chrono::steady_clock::time_point window_start_;
    std::atomic<int> request_count_{0};
    const int max_requests_per_window_ = 100;
    const std::chrono::milliseconds window_duration_{60000}; // 1 minute
    
public:
    bool allowRequest() {
        auto now = std::chrono::steady_clock::now();
        
        if (now - window_start_ > window_duration_) {
            window_start_ = now;
            request_count_.store(0);
        }
        
        if (request_count_.load() >= max_requests_per_window_) {
            return false;
        }
        
        request_count_.fetch_add(1);
        return true;
    }
};
```

---

## Performance Optimization

### 1. Memory Pool Management

```cpp
class MemoryPool {
private:
    std::vector<std::unique_ptr<char[]>> pools_;
    std::queue<char*> available_;
    std::mutex mutex_;
    const size_t pool_size_ = 64 * 1024; // 64KB blocks
    
public:
    char* allocate(size_t size) {
        if (size > pool_size_) {
            return new char[size]; // Fallback for large allocations
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (available_.empty()) {
            pools_.push_back(std::make_unique<char[]>(pool_size_));
            for (size_t i = 0; i < pool_size_; i += 1024) {
                available_.push(pools_.back().get() + i);
            }
        }
        
        char* ptr = available_.front();
        available_.pop();
        return ptr;
    }
    
    void deallocate(char* ptr) {
        if (!ptr) return;
        
        // Check if ptr belongs to pool (simplified)
        for (const auto& pool : pools_) {
            if (ptr >= pool.get() && ptr < pool.get() + pool_size_) {
                std::lock_guard<std::mutex> lock(mutex_);
                available_.push(ptr);
                return;
            }
        }
        
        delete[] ptr; // Not from pool
    }
};
```

### 2. Connection Pooling

```cpp
class ConnectionPool {
private:
    std::vector<std::unique_ptr<RpcClient>> clients_;
    std::queue<size_t> available_indices_;
    std::mutex mutex_;
    std::atomic<size_t> round_robin_{0};
    
public:
    size_t getClientIndex() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (available_indices_.empty()) {
            // Round-robin fallback
            return round_robin_.fetch_add(1) % clients_.size();
        }
        
        size_t index = available_indices_.front();
        available_indices_.pop();
        return index;
    }
    
    void returnClientIndex(size_t index) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_indices_.push(index);
    }
};
```

### 3. Batching Operations

```cpp
class BatchProcessor {
private:
    std::vector<std::string> pending_requests_;
    std::mutex mutex_;
    std::thread batch_thread_;
    std::atomic<bool> running_{true};
    const size_t batch_size_ = 10;
    const std::chrono::milliseconds batch_timeout_{100};
    
public:
    void addRequest(const std::string& request) {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_requests_.push_back(request);
        
        if (pending_requests_.size() >= batch_size_) {
            processBatch();
        }
    }
    
private:
    void processBatch() {
        if (pending_requests_.empty()) return;
        
        // Process all pending requests
        for (const auto& request : pending_requests_) {
            // Process request
        }
        
        pending_requests_.clear();
    }
};
```

---

## Complete Implementation Examples

### 1. Minimal RPC Client

```cpp
#include "rpc_client.hpp"
#include "rpc_operation_processor.hpp"
#include <memory>
#include <atomic>
#include <signal.h>

class MinimalRpcApp {
private:
    std::shared_ptr<RpcClient> rpcClient_;
    std::unique_ptr<RpcOperationProcessor> operationProcessor_;
    std::atomic<bool> running_{true};
    
public:
    bool initialize(const std::string& rpcConfig, const std::string& packageConfig) {
        // Load configuration
        PackageConfig config = loadConfig(packageConfig);
        
        // Create RPC client
        rpcClient_ = std::make_shared<RpcClient>(rpcConfig, "minimal-app");
        
        // Create operation processor
        operationProcessor_ = std::make_unique<RpcOperationProcessor>(config, false);
        
        // Set message handler
        rpcClient_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            if (topic.find("direct_messaging/minimal-app/requests") != std::string::npos) {
                operationProcessor_->processRequest(payload.c_str(), payload.size());
            }
        });
        
        // Start RPC client
        return rpcClient_->start();
    }
    
    void run() {
        // Setup signal handlers
        signal(SIGINT, [](int) { running_.store(false); });
        signal(SIGTERM, [](int) { running_.store(false); });
        
        // Main loop
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Cleanup
        if (rpcClient_) {
            rpcClient_->stop();
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <rpc-config> <package-config>" << std::endl;
        return 1;
    }
    
    MinimalRpcApp app;
    if (!app.initialize(argv[1], argv[2])) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    app.run();
    return 0;
}
```

### 2. Advanced Multi-Service Application

```cpp
class AdvancedRpcApp {
private:
    struct ServiceConfig {
        std::string name;
        std::shared_ptr<RpcClient> client;
        std::unique_ptr<RpcOperationProcessor> processor;
        std::string requestTopic;
        std::string responseTopic;
    };
    
    std::vector<ServiceConfig> services_;
    std::atomic<bool> running_{true};
    std::shared_ptr<ThreadMgr::ThreadManager> globalThreadManager_;
    
public:
    bool initialize(const std::vector<std::string>& serviceConfigs) {
        // Initialize global thread manager
        globalThreadManager_ = std::make_shared<ThreadMgr::ThreadManager>(200);
        
        for (const auto& configPath : serviceConfigs) {
            ServiceConfig service = loadServiceConfig(configPath);
            
            // Create service-specific components
            service.client = std::make_shared<RpcClient>(service.name + "_rpc.json", service.name);
            service.processor = std::make_unique<RpcOperationProcessor>(service.packageConfig, true);
            
            // Configure message handler with service-specific logic
            service.client->setMessageHandler([this, &service](const std::string& topic, const std::string& payload) {
                if (topic.find(service.requestTopic) != std::string::npos) {
                    // Add service-specific preprocessing
                    std::string processedPayload = preprocessPayload(payload, service.name);
                    service.processor->processRequest(processedPayload.c_str(), processedPayload.size());
                }
            });
            
            // Start service
            if (!service.client->start()) {
                std::cerr << "Failed to start service: " << service.name << std::endl;
                return false;
            }
            
            services_.push_back(std::move(service));
        }
        
        return true;
    }
    
    void run() {
        // Setup monitoring thread
        std::thread monitorThread([this]() {
            while (running_.load()) {
                monitorServices();
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        });
        
        // Main application loop
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Graceful shutdown
        shutdown();
        monitorThread.join();
    }
    
private:
    void monitorServices() {
        for (const auto& service : services_) {
            if (!service.client->isRunning()) {
                std::cerr << "Service " << service.name << " is not running, attempting restart..." << std::endl;
                // Attempt restart logic
            }
        }
    }
    
    void shutdown() {
        for (auto& service : services_) {
            if (service.client) {
                service.client->stop();
            }
        }
    }
};
```

### 3. High-Performance Server

```cpp
class HighPerformanceServer {
private:
    struct PerformanceMetrics {
        std::atomic<uint64_t> requests_processed{0};
        std::atomic<uint64_t> errors_count{0};
        std::atomic<uint64_t> average_response_time_us{0};
        std::chrono::steady_clock::time_point start_time;
    };
    
    std::shared_ptr<RpcClient> rpcClient_;
    std::unique_ptr<RpcOperationProcessor> operationProcessor_;
    PerformanceMetrics metrics_;
    std::thread metrics_thread_;
    
public:
    bool initialize(const std::string& config) {
        // Initialize with performance optimizations
        rpcClient_ = std::make_shared<RpcClient>(config, "high-perf-server");
        operationProcessor_ = std::make_unique<RpcOperationProcessor>(loadConfig(config), true);
        
        // Enable performance monitoring
        metrics_.start_time = std::chrono::steady_clock::now();
        
        // Set optimized message handler
        rpcClient_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Process request
            operationProcessor_->processRequest(payload.c_str(), payload.size());
            
            // Update metrics
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            metrics_.requests_processed.fetch_add(1);
            updateAverageResponseTime(duration.count());
        });
        
        // Start metrics collection thread
        metrics_thread_ = std::thread([this]() {
            while (running_.load()) {
                collectMetrics();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });
        
        return rpcClient_->start();
    }
    
private:
    void updateAverageResponseTime(uint64_t response_time_us) {
        // Exponential moving average
        const double alpha = 0.1;
        uint64_t current_avg = metrics_.average_response_time_us.load();
        uint64_t new_avg = static_cast<uint64_t>(alpha * response_time_us + (1.0 - alpha) * current_avg);
        metrics_.average_response_time_us.store(new_avg);
    }
    
    void collectMetrics() {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - metrics_.start_time);
        
        double requests_per_second = static_cast<double>(metrics_.requests_processed.load()) / uptime.count();
        
        std::cout << "Performance Metrics:" << std::endl;
        std::cout << "  Requests processed: " << metrics_.requests_processed.load() << std::endl;
        std::cout << "  Requests/second: " << requests_per_second << std::endl;
        std::cout << "  Average response time: " << metrics_.average_response_time_us.load() << " μs" << std::endl;
        std::cout << "  Error count: " << metrics_.errors_count.load() << std::endl;
    }
};
```

---

## Best Practices

### 1. Initialization Best Practices

```cpp
// DO: Use RAII for resource management
class RpcApplication {
private:
    std::shared_ptr<RpcClient> client_;
    std::unique_ptr<RpcOperationProcessor> processor_;
    
public:
    RpcApplication(const std::string& config) {
        // Automatic initialization in constructor
        initialize(config);
    }
    
    ~RpcApplication() {
        // Automatic cleanup in destructor
        shutdown();
    }
};

// DON'T: Manual resource management
RpcClient* client = new RpcClient(config);
// ... later ...
delete client; // Error-prone
```

### 2. Error Handling Best Practices

```cpp
// DO: Comprehensive error handling with specific types
try {
    operationProcessor_->processRequest(payload.c_str(), payload.size());
} catch (const json::parse_error& e) {
    sendErrorResponse("Invalid JSON format: " + std::string(e.what()));
} catch (const std::bad_alloc& e) {
    sendErrorResponse("Server out of memory");
    logCriticalError(e);
} catch (const std::exception& e) {
    sendErrorResponse("Internal server error");
    logError(e);
}

// DON'T: Generic error handling
try {
    // ... code ...
} catch (...) {
    // Loses all error information
}
```

### 3. Thread Safety Best Practices

```cpp
// DO: Use atomic operations for simple flags
std::atomic<bool> shutdown_requested_{false};

// DO: Use mutexes for complex data structures
std::mutex config_mutex_;
PackageConfig config_;

void updateConfig(const PackageConfig& new_config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = new_config;
}

// DON'T: Share mutable state without synchronization
class BadExample {
    std::vector<std::string> shared_data_; // Not thread-safe!
public:
    void addData(const std::string& data) {
        shared_data_.push_back(data); // Race condition!
    }
};
```

### 4. Memory Management Best Practices

```cpp
// DO: Use smart pointers for automatic memory management
auto context = std::make_shared<RequestContext>(
    requestJson, transactionId, responseTopic, config, verbose
);

// DO: Use weak_ptr to avoid circular references
std::weak_ptr<RpcClient> weak_client = shared_client;

// DON'T: Raw pointers with manual deletion
RequestContext* context = new RequestContext(...);
// ... later ...
delete context; // Easy to forget or delete twice
```

### 5. Configuration Best Practices

```cpp
// DO: Validate configuration at startup
bool validateConfig(const PackageConfig& config) {
    if (config.private_key_file.empty()) {
        throw std::invalid_argument("Private key file is required");
    }
    
    if (!std::filesystem::exists(config.private_key_file)) {
        throw std::invalid_argument("Private key file does not exist: " + config.private_key_file);
    }
    
    return true;
}

// DO: Use sensible defaults
PackageConfig loadConfig(const std::string& path) {
    PackageConfig config;
    
    if (std::filesystem::exists(path)) {
        config = PackageConfig::from_json(parseJsonFile(path));
    } else {
        std::cout << "Using default configuration" << std::endl;
    }
    
    return config;
}
```

---

## Troubleshooting and Debugging

### 1. Common Connection Issues

```cpp
// Symptom: Client fails to start
// Diagnosis: Check configuration and network connectivity
void debugConnectionIssues(const std::string& configPath) {
    std::cout << "Debugging connection issues..." << std::endl;
    
    // Check configuration file
    if (!std::filesystem::exists(configPath)) {
        std::cerr << "ERROR: Configuration file not found: " << configPath << std::endl;
        return;
    }
    
    // Parse and validate configuration
    try {
        auto config = parseJsonFile(configPath);
        std::cout << "MQTT Broker: " << config["mqtt"]["broker"] << std::endl;
        std::cout << "Port: " << config["mqtt"]["port"] << std::endl;
        std::cout << "Client ID: " << config["mqtt"]["client_id"] << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Invalid configuration: " << e.what() << std::endl;
        return;
    }
    
    // Test network connectivity
    std::string broker = config["mqtt"]["broker"];
    int port = config["mqtt"]["port"];
    
    // Simple connectivity test (would need actual implementation)
    if (!testTcpConnection(broker, port)) {
        std::cerr << "ERROR: Cannot connect to MQTT broker" << std::endl;
    } else {
        std::cout << "Network connectivity OK" << std::endl;
    }
}
```

### 2. Memory Leak Detection

```cpp
// Enable memory leak detection in debug builds
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

// In main function
int main(int argc, char** argv) {
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    
    // Application code
    
    return 0;
}
```

### 3. Performance Profiling

```cpp
class PerformanceProfiler {
private:
    std::map<std::string, std::chrono::high_resolution_clock::time_point> start_times_;
    std::map<std::string, std::vector<uint64_t>> measurements_;
    
public:
    void startMeasurement(const std::string& name) {
        start_times_[name] = std::chrono::high_resolution_clock::now();
    }
    
    void endMeasurement(const std::string& name) {
        auto it = start_times_.find(name);
        if (it != start_times_.end()) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - it->second).count();
            
            measurements_[name].push_back(duration);
            start_times_.erase(it);
        }
    }
    
    void printStatistics() {
        for (const auto& [name, times] : measurements_) {
            if (times.empty()) continue;
            
            uint64_t total = 0;
            uint64_t min_time = times[0];
            uint64_t max_time = times[0];
            
            for (uint64_t time : times) {
                total += time;
                min_time = std::min(min_time, time);
                max_time = std::max(max_time, time);
            }
            
            double average = static_cast<double>(total) / times.size();
            
            std::cout << name << " Statistics:" << std::endl;
            std::cout << "  Count: " << times.size() << std::endl;
            std::cout << "  Average: " << average << " μs" << std::endl;
            std::cout << "  Min: " << min_time << " μs" << std::endl;
            std::cout << "  Max: " << max_time << " μs" << std::endl;
        }
    }
};

// Usage example
PerformanceProfiler profiler;

void processRequest(const std::string& request) {
    profiler.startMeasurement("request_processing");
    
    // Process request
    
    profiler.endMeasurement("request_processing");
}
```

### 4. Logging and Monitoring

```cpp
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger {
private:
    std::ofstream log_file_;
    LogLevel min_level_;
    std::mutex mutex_;
    
public:
    Logger(const std::string& filename, LogLevel min_level = LogLevel::INFO)
        : min_level_(min_level) {
        log_file_.open(filename, std::ios::app);
    }
    
    void log(LogLevel level, const std::string& message) {
        if (level < min_level_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        log_file_ << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        log_file_ << " [" << levelToString(level) << "] ";
        log_file_ << message << std::endl;
        log_file_.flush();
    }
    
private:
    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
};

// Global logger instance
Logger g_logger("application.log", LogLevel::DEBUG);

// Usage macros for convenience
#define LOG_DEBUG(msg) g_logger.log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg) g_logger.log(LogLevel::INFO, msg)
#define LOG_WARNING(msg) g_logger.log(LogLevel::WARNING, msg)
#define LOG_ERROR(msg) g_logger.log(LogLevel::ERROR, msg)
#define LOG_CRITICAL(msg) g_logger.log(LogLevel::CRITICAL, msg)
```

---

## Advanced Patterns

### 1. Request Pipeline Pattern

```cpp
class RequestPipeline {
private:
    struct PipelineStage {
        std::string name;
        std::function<bool(const std::string&, std::string&)> processor;
    };
    
    std::vector<PipelineStage> stages_;
    
public:
    void addStage(const std::string& name, 
                  std::function<bool(const std::string&, std::string&)> processor) {
        stages_.push_back({name, processor});
    }
    
    bool process(const std::string& input, std::string& output) {
        std::string current = input;
        
        for (const auto& stage : stages_) {
            std::string next;
            if (!stage.processor(current, next)) {
                LOG_ERROR("Pipeline stage failed: " + stage.name);
                return false;
            }
            current = next;
        }
        
        output = current;
        return true;
    }
};

// Usage example
RequestPipeline pipeline;

// Add validation stage
pipeline.addStage("validation", [](const std::string& input, std::string& output) {
    if (!validateJsonRpcMessage(input)) {
        return false;
    }
    output = input;
    return true;
});

// Add authentication stage
pipeline.addStage("authentication", [](const std::string& input, std::string& output) {
    if (!authenticateRequest(input)) {
        return false;
    }
    output = input;
    return true;
});

// Add processing stage
pipeline.addStage("processing", [](const std::string& input, std::string& output) {
    return processBusinessLogic(input, output);
});
```

### 2. Circuit Breaker Pattern

```cpp
class CircuitBreaker {
private:
    enum class State {
        CLOSED,    // Normal operation
        OPEN,      // Failing, reject requests
        HALF_OPEN  // Testing if service recovered
    };
    
    std::atomic<State> state_{State::CLOSED};
    std::atomic<int> failure_count_{0};
    std::atomic<int> success_count_{0};
    std::chrono::steady_clock::time_point last_failure_time_;
    std::mutex mutex_;
    
    const int failure_threshold_ = 5;
    const int success_threshold_ = 3;
    const std::chrono::seconds timeout_{30};
    
public:
    bool execute(std::function<bool()> operation) {
        State current_state = state_.load();
        
        if (current_state == State::OPEN) {
            // Check if timeout has passed
            if (std::chrono::steady_clock::now() - last_failure_time_ > timeout_) {
                state_.store(State::HALF_OPEN);
                success_count_.store(0);
            } else {
                return false; // Circuit is open
            }
        }
        
        try {
            bool result = operation();
            
            if (result) {
                onSuccess();
            } else {
                onFailure();
            }
            
            return result;
            
        } catch (const std::exception&) {
            onFailure();
            return false;
        }
    }
    
private:
    void onSuccess() {
        State current_state = state_.load();
        
        if (current_state == State::HALF_OPEN) {
            int successes = success_count_.fetch_add(1) + 1;
            if (successes >= success_threshold_) {
                state_.store(State::CLOSED);
                failure_count_.store(0);
            }
        } else if (current_state == State::CLOSED) {
            failure_count_.store(0);
        }
    }
    
    void onFailure() {
        int failures = failure_count_.fetch_add(1) + 1;
        last_failure_time_ = std::chrono::steady_clock::now();
        
        if (failures >= failure_threshold_) {
            state_.store(State::OPEN);
        }
        
        if (state_.load() == State::HALF_OPEN) {
            state_.store(State::OPEN);
        }
    }
};
```

### 3. Request Caching Pattern

```cpp
template<typename Key, typename Value>
class RequestCache {
private:
    struct CacheEntry {
        Value value;
        std::chrono::steady_clock::time_point expiry_time;
    };
    
    std::unordered_map<Key, CacheEntry> cache_;
    std::shared_mutex mutex_;  // Read-write lock
    std::chrono::seconds default_ttl_;
    
public:
    RequestCache(std::chrono::seconds default_ttl = std::chrono::seconds(300))
        : default_ttl_(default_ttl) {}
    
    std::optional<Value> get(const Key& key) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        
        if (std::chrono::steady_clock::now() > it->second.expiry_time) {
            lock.unlock();
            std::unique_lock<std::shared_mutex> unique_lock(mutex_);
            cache_.erase(it);
            return std::nullopt;
        }
        
        return it->second.value;
    }
    
    void put(const Key& key, const Value& value, 
             std::optional<std::chrono::seconds> ttl = std::nullopt) {
        auto expiry_time = std::chrono::steady_clock::now() + 
                          (ttl.value_or(default_ttl_));
        
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_[key] = {value, expiry_time};
    }
    
    void clearExpired() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto it = cache_.begin();
        
        while (it != cache_.end()) {
            if (now > it->second.expiry_time) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// Usage example for caching operation results
RequestCache<std::string, std::string> operation_cache;

std::string getCachedOperationResult(const std::string& operation_key,
                                    std::function<std::string()> operation) {
    // Try to get from cache
    auto cached_result = operation_cache.get(operation_key);
    if (cached_result) {
        return *cached_result;
    }
    
    // Execute operation and cache result
    std::string result = operation();
    operation_cache.put(operation_key, result, std::chrono::seconds(60));
    
    return result;
}
```

---

## Testing Strategies

### 1. Unit Testing Framework

```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Mock classes for testing
class MockRpcClient : public RpcClient {
public:
    MOCK_METHOD(bool, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
    MOCK_METHOD(void, setMessageHandler, 
                (std::function<void(const std::string&, const std::string&)>), 
                (override));
};

class MockOperationProcessor : public RpcOperationProcessor {
public:
    MOCK_METHOD(void, processRequest, (const char*, size_t), (override));
};

// Test fixture
class RpcApplicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_client_ = std::make_shared<MockRpcClient>();
        mock_processor_ = std::make_unique<MockOperationProcessor>();
    }
    
    std::shared_ptr<MockRpcClient> mock_client_;
    std::unique_ptr<MockOperationProcessor> mock_processor_;
};

// Test cases
TEST_F(RpcApplicationTest, SuccessfulInitialization) {
    EXPECT_CALL(*mock_client_, start())
        .WillOnce(::testing::Return(true));
    
    EXPECT_CALL(*mock_client_, setMessageHandler(::testing::_))
        .Times(1);
    
    RpcApplication app(mock_client_, std::move(mock_processor_));
    bool result = app.initialize();
    
    EXPECT_TRUE(result);
}

TEST_F(RpcApplicationTest, HandlerProcessesCorrectTopic) {
    std::string received_topic;
    std::string received_payload;
    
    EXPECT_CALL(*mock_processor_, processRequest(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([&](const char* payload, size_t len) {
            received_payload = std::string(payload, len);
        }));
    
    // Set up message handler
    std::function<void(const std::string&, const std::string&)> handler;
    EXPECT_CALL(*mock_client_, setMessageHandler(::testing::_))
        .WillOnce(::testing::SaveArg<0>(&handler));
    
    RpcApplication app(mock_client_, std::move(mock_processor_));
    app.initialize();
    
    // Test message processing
    std::string test_topic = "direct_messaging/test-app/requests";
    std::string test_payload = R"({"jsonrpc":"2.0","method":"test","params":{}})";
    
    handler(test_topic, test_payload);
    
    EXPECT_EQ(received_payload, test_payload);
}
```

### 2. Integration Testing

```cpp
class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start test MQTT broker
        test_broker_ = std::make_unique<TestMqttBroker>();
        test_broker_->start("localhost", 1884);
        
        // Create test configuration
        createTestConfig();
    }
    
    void TearDown() override {
        test_broker_->stop();
        std::filesystem::remove("test_config.json");
        std::filesystem::remove("test_package.json");
    }
    
    void createTestConfig() {
        nlohmann::json rpc_config = {
            {"mqtt", {
                {"broker", "localhost"},
                {"port", 1884},
                {"client_id", "test-client"}
            }},
            {"topics", {
                {"request_topic", "direct_messaging/test/requests"},
                {"response_topic", "direct_messaging/test/responses"}
            }}
        };
        
        std::ofstream rpc_file("test_config.json");
        rpc_file << rpc_config.dump();
        rpc_file.close();
        
        nlohmann::json package_config = {
            {"private_key_file", "test_private.pem"},
            {"public_key_file", "test_public.pem"}
        };
        
        std::ofstream package_file("test_package.json");
        package_file << package_config.dump();
        package_file.close();
    }
    
    std::unique_ptr<TestMqttBroker> test_broker_;
};

TEST_F(IntegrationTest, EndToEndRequestProcessing) {
    // Create application
    RpcApplication app;
    ASSERT_TRUE(app.initialize("test_config.json", "test_package.json"));
    
    // Start in separate thread
    std::thread app_thread([&app]() {
        app.run();
    });
    
    // Wait for application to start
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Send test request
    TestMqttClient client;
    client.connect("localhost", 1884);
    
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", "test-123"},
        {"method", "verify"},
        {"params", {
            {"license_file", "test_license.lic"}
        }}
    };
    
    client.publish("direct_messaging/test/requests", request.dump());
    
    // Wait for response
    std::string response = client.waitForMessage("direct_messaging/test/responses", 5000);
    
    ASSERT_FALSE(response.empty());
    
    auto response_json = nlohmann::json::parse(response);
    EXPECT_EQ(response_json["id"], "test-123");
    EXPECT_TRUE(response_json.contains("result"));
    
    // Shutdown
    app.shutdown();
    app_thread.join();
}
```

### 3. Performance Testing

```cpp
class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup performance monitoring
        profiler_ = std::make_unique<PerformanceProfiler>();
    }
    
    void runLoadTest(int num_requests, int num_threads) {
        std::vector<std::thread> threads;
        std::atomic<int> completed_requests{0};
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < num_requests / num_threads; ++j) {
                    profiler_->startMeasurement("single_request");
                    
                    // Send request
                    std::string request_id = "perf-" + std::to_string(i) + "-" + std::to_string(j);
                    sendTestRequest(request_id);
                    
                    // Wait for response
                    waitForResponse(request_id);
                    
                    profiler_->endMeasurement("single_request");
                    completed_requests.fetch_add(1);
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // Report results
        std::cout << "Load Test Results:" << std::endl;
        std::cout << "  Total requests: " << num_requests << std::endl;
        std::cout << "  Concurrent threads: " << num_threads << std::endl;
        std::cout << "  Total time: " << total_duration.count() << " ms" << std::endl;
        std::cout << "  Requests per second: " 
                  << (num_requests * 1000.0) / total_duration.count() << std::endl;
        
        profiler_->printStatistics();
    }
    
    std::unique_ptr<PerformanceProfiler> profiler_;
};

TEST_F(PerformanceTest, HighConcurrencyTest) {
    runLoadTest(1000, 10);
}

TEST_F(PerformanceTest, SustainedLoadTest) {
    runLoadTest(5000, 20);
}
```

---

## Deployment Considerations

### 1. Production Configuration

```json
{
  "mqtt": {
    "broker": "production-mqtt.company.com",
    "port": 8883,
    "username": "production_user",
    "password": "${MQTT_PASSWORD}",
    "client_id": "prod-licence-mann-${HOSTNAME}",
    "keepalive": 60,
    "clean_session": false,
    "auto_reconnect": true,
    "reconnect_delay": 1000,
    "max_reconnect_attempts": 50,
    "will_topic": "direct_messaging/licence-mann/status",
    "will_payload": "offline",
    "will_qos": 1,
    "will_retain": true
  },
  "security": {
    "use_tls": true,
    "ca_cert": "/etc/ssl/certs/company-ca.pem",
    "client_cert": "/etc/ssl/certs/licence-mann-client.pem",
    "client_key": "/etc/ssl/private/licence-mann-client.key",
    "key_password": "${SSL_KEY_PASSWORD}"
  },
  "topics": {
    "request_topic": "direct_messaging/licence-mann/requests",
    "response_topic": "direct_messaging/licence-mann/responses",
    "status_topic": "direct_messaging/licence-mann/status",
    "metrics_topic": "direct_messaging/licence-mann/metrics"
  },
  "performance": {
    "thread_pool_size": 50,
    "max_concurrent_requests": 100,
    "request_timeout_ms": 30000,
    "heartbeat_interval_ms": 30000
  },
  "logging": {
    "level": "INFO",
    "file": "/var/log/licence-mann/application.log",
    "max_size_mb": 100,
    "backup_count": 10
  }
}
```

### 2. Docker Deployment

```dockerfile
# Multi-stage build for production
FROM gcc:11 as builder

WORKDIR /build
COPY . .

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    libmosquitto-dev \
    libssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Build application
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# Production stage
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libmosquitto1 \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create application user
RUN useradd -r -s /bin/false licence-mann

# Copy application
COPY --from=builder /build/build/ur-licence-mann /usr/local/bin/
COPY --from=builder /build/config /etc/licence-mann/

# Create directories
RUN mkdir -p /var/log/licence-mann /var/lib/licence-mann && \
    chown -R licence-mann:licence-mann /var/log/licence-mann /var/lib/licence-mann

# Switch to non-root user
USER licence-mann

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD /usr/local/bin/ur-licence-mann --health-check || exit 1

EXPOSE 1883 8883

CMD ["/usr/local/bin/ur-licence-mann", "--package-config", "/etc/licence-mann/package.json", "--rpc-config", "/etc/licence-mann/rpc.json"]
```

### 3. Kubernetes Deployment

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: licence-mann
  labels:
    app: licence-mann
spec:
  replicas: 3
  selector:
    matchLabels:
      app: licence-mann
  template:
    metadata:
      labels:
        app: licence-mann
    spec:
      containers:
      - name: licence-mann
        image: licence-mann:latest
        ports:
        - containerPort: 1883
        - containerPort: 8883
        env:
        - name: MQTT_PASSWORD
          valueFrom:
            secretKeyRef:
              name: mqtt-secrets
              key: password
        - name: SSL_KEY_PASSWORD
          valueFrom:
            secretKeyRef:
              name: ssl-secrets
              key: key-password
        - name: HOSTNAME
          valueFrom:
            fieldRef:
              fieldPath: metadata.name
        resources:
          requests:
            memory: "256Mi"
            cpu: "250m"
          limits:
            memory: "512Mi"
            cpu: "500m"
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 5
        volumeMounts:
        - name: config
          mountPath: /etc/licence-mann
        - name: logs
          mountPath: /var/log/licence-mann
        - name: data
          mountPath: /var/lib/licence-mann
      volumes:
      - name: config
        configMap:
          name: licence-mann-config
      - name: logs
        emptyDir: {}
      - name: data
        persistentVolumeClaim:
          claimName: licence-mann-data
---
apiVersion: v1
kind: Service
metadata:
  name: licence-mann-service
spec:
  selector:
    app: licence-mann
  ports:
  - name: mqtt
    port: 1883
    targetPort: 1883
  - name: mqtts
    port: 8883
    targetPort: 8883
  type: ClusterIP
```

---

## Migration Guide

### 1. From Legacy RPC Systems

```cpp
// Legacy system pattern
class LegacyRpcHandler {
public:
    void handleRequest(const std::string& method, const std::string& params) {
        if (method == "old_method_1") {
            handleOldMethod1(params);
        } else if (method == "old_method_2") {
            handleOldMethod2(params);
        }
        // ... many more methods
    }
};

// Migrated to UR-RPC Template
class ModernRpcHandler {
private:
    std::map<std::string, std::function<int(const std::map<std::string, std::string>&)>> handlers_;
    
public:
    ModernRpcHandler() {
        // Register handlers using modern pattern
        handlers_["old_method_1"] = [this](const auto& params) {
            return handleOldMethod1Modern(params);
        };
        handlers_["old_method_2"] = [this](const auto& params) {
            return handleOldMethod2Modern(params);
        };
    }
    
    int execute(const std::string& method, const std::map<std::string, std::string>& params) {
        auto it = handlers_.find(method);
        if (it != handlers_.end()) {
            return it->second(params);
        }
        return -1; // Method not found
    }
};
```

### 2. Configuration Migration

```cpp
// Legacy configuration format
class LegacyConfig {
public:
    std::string server_host;
    int server_port;
    std::string auth_token;
    
    void loadFromFile(const std::string& filename) {
        // Custom parsing logic
    }
};

// Modern configuration format
class ModernConfig {
public:
    static ModernConfig fromLegacy(const LegacyConfig& legacy) {
        ModernConfig modern;
        
        // Convert to MQTT-based configuration
        modern.mqtt_broker = legacy.server_host;
        modern.mqtt_port = legacy.server_port;
        modern.mqtt_username = "legacy_user";
        modern.mqtt_password = legacy.auth_token;
        
        // Set modern defaults
        modern.client_id = "migrated-app";
        modern.keepalive = 60;
        modern.auto_reconnect = true;
        
        return modern;
    }
    
    nlohmann::json toJson() const {
        return {
            {"mqtt", {
                {"broker", mqtt_broker},
                {"port", mqtt_port},
                {"username", mqtt_username},
                {"password", mqtt_password},
                {"client_id", client_id},
                {"keepalive", keepalive},
                {"auto_reconnect", auto_reconnect}
            }}
        };
    }
};
```

### 3. Gradual Migration Strategy

```cpp
class HybridRpcSystem {
private:
    std::unique_ptr<LegacyRpcHandler> legacy_handler_;
    std::unique_ptr<ModernRpcHandler> modern_handler_;
    std::shared_ptr<RpcClient> rpc_client_;
    std::atomic<bool> migration_mode_{false};
    
public:
    void initialize(const std::string& config) {
        // Initialize both systems
        legacy_handler_ = std::make_unique<LegacyRpcHandler>();
        modern_handler_ = std::make_unique<ModernRpcHandler>();
        
        // Setup RPC client with hybrid message handler
        rpc_client_ = std::make_shared<RpcClient>(config, "hybrid-system");
        
        rpc_client_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            if (migration_mode_.load()) {
                // Use modern handler
                handleModernRequest(payload);
            } else {
                // Use legacy handler with compatibility layer
                handleLegacyRequest(payload);
            }
        });
    }
    
    void enableMigrationMode() {
        migration_mode_.store(true);
        std::cout << "Migration mode enabled - using modern handlers" << std::endl;
    }
    
    void disableMigrationMode() {
        migration_mode_.store(false);
        std::cout << "Migration mode disabled - using legacy handlers" << std::endl;
    }
    
private:
    void handleLegacyRequest(const std::string& payload) {
        // Parse legacy format and convert to modern
        auto legacy_request = parseLegacyRequest(payload);
        auto modern_params = convertLegacyParams(legacy_request.params);
        
        // Route to appropriate handler
        if (legacy_request.method.starts_with("legacy_")) {
            // Handle with legacy system
            legacy_handler_->handleRequest(legacy_request.method, legacy_request.params);
        } else {
            // Handle with modern system
            modern_handler_->execute(legacy_request.method, modern_params);
        }
    }
    
    void handleModernRequest(const std::string& payload) {
        // Direct modern processing
        auto modern_request = parseModernRequest(payload);
        modern_handler_->execute(modern_request.method, modern_request.params);
    }
};
```

---

## Reference Implementation

### Complete Production-Ready Application

```cpp
#pragma once

#include <memory>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <fstream>
#include <sstream>
#include <iostream>

#include "rpc_client.hpp"
#include "rpc_operation_processor.hpp"
#include "operation_handler.hpp"
#include "package_config.hpp"
#include "ThreadManager.hpp"

#include <nlohmann/json.hpp>
#include <mosquitto.h>
#include <cJSON.h>

using json = nlohmann::json;

/**
 * @brief Complete production-ready RPC application
 * 
 * This class demonstrates the full integration of UR-RPC Template
 * with all production considerations including:
 * - Graceful startup and shutdown
 * - Comprehensive error handling
 * - Performance monitoring
 * - Security features
 * - Logging and metrics
 * - Configuration management
 */
class ProductionRpcApplication {
public:
    struct Metrics {
        std::atomic<uint64_t> requests_received{0};
        std::atomic<uint64_t> requests_processed{0};
        std::atomic<uint64_t> requests_failed{0};
        std::atomic<uint64_t> average_response_time_us{0};
        std::atomic<uint64_t> peak_memory_usage_mb{0};
        std::chrono::steady_clock::time_point start_time;
        
        void reset() {
            requests_received.store(0);
            requests_processed.store(0);
            requests_failed.store(0);
            average_response_time_us.store(0);
            peak_memory_usage_mb.store(0);
            start_time = std::chrono::steady_clock::now();
        }
        
        json toJson() const {
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                now - start_time).count();
            
            return {
                {"uptime_seconds", uptime},
                {"requests_received", requests_received.load()},
                {"requests_processed", requests_processed.load()},
                {"requests_failed", requests_failed.load()},
                {"success_rate", calculateSuccessRate()},
                {"average_response_time_us", average_response_time_us.load()},
                {"peak_memory_usage_mb", peak_memory_usage_mb.load()}
            };
        }
        
    private:
        double calculateSuccessRate() const {
            uint64_t total = requests_processed.load() + requests_failed.load();
            if (total == 0) return 0.0;
            return (static_cast<double>(requests_processed.load()) / total) * 100.0;
        }
    };
    
    struct SecurityConfig {
        bool enable_authentication = true;
        bool enable_authorization = true;
        bool enable_rate_limiting = true;
        int max_requests_per_minute = 1000;
        std::vector<std::string> allowed_topics;
        std::string authentication_token;
        
        bool isTopicAuthorized(const std::string& topic) const {
            if (!enable_authorization) return true;
            
            for (const auto& allowed : allowed_topics) {
                if (topic.find(allowed) == 0) {
                    return true;
                }
            }
            return false;
        }
    };
    
    struct LoggingConfig {
        enum class Level {
            DEBUG = 0,
            INFO = 1,
            WARNING = 2,
            ERROR = 3,
            CRITICAL = 4
        };
        
        Level min_level = Level::INFO;
        std::string log_file = "application.log";
        size_t max_file_size_mb = 100;
        int backup_count = 10;
        bool enable_console_output = true;
        bool enable_metrics_logging = true;
    };
    
private:
    // Core components
    std::shared_ptr<RpcClient> rpc_client_;
    std::unique_ptr<RpcOperationProcessor> operation_processor_;
    
    // Configuration
    PackageConfig package_config_;
    SecurityConfig security_config_;
    LoggingConfig logging_config_;
    
    // Runtime state
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::string application_id_;
    
    // Thread management
    std::unique_ptr<ThreadMgr::ThreadManager> thread_manager_;
    std::thread metrics_thread_;
    std::thread health_check_thread_;
    
    // Monitoring and metrics
    Metrics metrics_;
    std::mutex metrics_mutex_;
    
    // Rate limiting
    std::chrono::steady_clock::time_point rate_limit_window_start_;
    std::atomic<int> requests_in_current_window_{0};
    
    // Logging
    std::ofstream log_file_;
    std::mutex log_mutex_;
    
public:
    ProductionRpcApplication(const std::string& application_id)
        : application_id_(application_id) {
        
        // Initialize metrics
        metrics_.reset();
        
        // Setup signal handlers
        setupSignalHandlers();
    }
    
    ~ProductionRpcApplication() {
        shutdown();
    }
    
    bool initialize(const std::string& rpc_config_path, 
                   const std::string& package_config_path) {
        try {
            log(LoggingConfig::Level::INFO, "Initializing application: " + application_id_);
            
            // Load configurations
            if (!loadConfigurations(rpc_config_path, package_config_path)) {
                log(LoggingConfig::Level::ERROR, "Failed to load configurations");
                return false;
            }
            
            // Initialize thread manager
            thread_manager_ = std::make_unique<ThreadMgr::ThreadManager>(50);
            
            // Initialize RPC client
            rpc_client_ = std::make_shared<RpcClient>(rpc_config_path, application_id_);
            
            // Initialize operation processor
            operation_processor_ = std::make_unique<RpcOperationProcessor>(package_config_, true);
            
            // Setup message handler with security features
            setupMessageHandler();
            
            // Start monitoring threads
            startMonitoringThreads();
            
            log(LoggingConfig::Level::INFO, "Application initialized successfully");
            return true;
            
        } catch (const std::exception& e) {
            log(LoggingConfig::Level::CRITICAL, "Initialization failed: " + std::string(e.what()));
            return false;
        }
    }
    
    bool start() {
        if (running_.load()) {
            log(LoggingConfig::Level::WARNING, "Application is already running");
            return true;
        }
        
        try {
            log(LoggingConfig::Level::INFO, "Starting RPC client...");
            
            if (!rpc_client_->start()) {
                log(LoggingConfig::Level::ERROR, "Failed to start RPC client");
                return false;
            }
            
            // Wait for client to be ready
            if (!waitForClientReady()) {
                log(LoggingConfig::Level::ERROR, "RPC client failed to become ready");
                return false;
            }
            
            running_.store(true);
            log(LoggingConfig::Level::INFO, "Application started successfully");
            
            return true;
            
        } catch (const std::exception& e) {
            log(LoggingConfig::Level::CRITICAL, "Failed to start application: " + std::string(e.what()));
            return false;
        }
    }
    
    void run() {
        if (!running_.load()) {
            log(LoggingConfig::Level::ERROR, "Cannot run - application not started");
            return;
        }
        
        log(LoggingConfig::Level::INFO, "Application is running. Press Ctrl+C to stop.");
        
        // Main application loop
        while (running_.load() && !shutdown_requested_.load()) {
            try {
                // Periodic health check
                performHealthCheck();
                
                // Sleep to prevent busy waiting
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
            } catch (const std::exception& e) {
                log(LoggingConfig::Level::ERROR, "Error in main loop: " + std::string(e.what()));
            }
        }
        
        log(LoggingConfig::Level::INFO, "Application loop ended");
    }
    
    void shutdown() {
        if (!running_.load()) {
            return;
        }
        
        log(LoggingConfig::Level::INFO, "Shutting down application...");
        
        shutdown_requested_.store(true);
        running_.store(false);
        
        // Stop RPC client
        if (rpc_client_) {
            rpc_client_->stop();
        }
        
        // Stop monitoring threads
        stopMonitoringThreads();
        
        // Final metrics report
        reportFinalMetrics();
        
        log(LoggingConfig::Level::INFO, "Application shutdown complete");
    }
    
    const Metrics& getMetrics() const {
        return metrics_;
    }
    
    json getStatus() const {
        return {
            {"application_id", application_id_},
            {"running", running_.load()},
            {"shutdown_requested", shutdown_requested_.load()},
            {"rpc_client_running", rpc_client_ ? rpc_client_->isRunning() : false},
            {"metrics", metrics_.toJson()},
            {"uptime_seconds", getUptimeSeconds()}
        };
    }
    
private:
    bool loadConfigurations(const std::string& rpc_config_path, 
                           const std::string& package_config_path) {
        try {
            // Load package configuration
            std::ifstream package_file(package_config_path);
            if (package_file.good()) {
                json package_json;
                package_file >> package_json;
                package_config_ = PackageConfig::from_json(package_json);
                log(LoggingConfig::Level::INFO, "Loaded package configuration from: " + package_config_path);
            } else {
                log(LoggingConfig::Level::WARNING, "Package config not found, using defaults");
            }
            
            // Load and validate RPC configuration
            std::ifstream rpc_file(rpc_config_path);
            if (!rpc_file.good()) {
                log(LoggingConfig::Level::ERROR, "RPC configuration file not found: " + rpc_config_path);
                return false;
            }
            
            json rpc_json;
            rpc_file >> rpc_json;
            
            // Extract security configuration
            if (rpc_json.contains("security")) {
                auto sec_json = rpc_json["security"];
                security_config_.enable_authentication = sec_json.value("enable_authentication", true);
                security_config_.enable_authorization = sec_json.value("enable_authorization", true);
                security_config_.enable_rate_limiting = sec_json.value("enable_rate_limiting", true);
                security_config_.max_requests_per_minute = sec_json.value("max_requests_per_minute", 1000);
                
                if (sec_json.contains("allowed_topics")) {
                    for (const auto& topic : sec_json["allowed_topics"]) {
                        security_config_.allowed_topics.push_back(topic.get<std::string>());
                    }
                }
            }
            
            // Extract logging configuration
            if (rpc_json.contains("logging")) {
                auto log_json = rpc_json["logging"];
                std::string level_str = log_json.value("level", "INFO");
                logging_config_.min_level = stringToLogLevel(level_str);
                logging_config_.log_file = log_json.value("log_file", "application.log");
                logging_config_.max_file_size_mb = log_json.value("max_file_size_mb", 100);
                logging_config_.backup_count = log_json.value("backup_count", 10);
                logging_config_.enable_console_output = log_json.value("enable_console_output", true);
                logging_config_.enable_metrics_logging = log_json.value("enable_metrics_logging", true);
            }
            
            // Initialize logging
            initializeLogging();
            
            log(LoggingConfig::Level::INFO, "Loaded RPC configuration from: " + rpc_config_path);
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error loading configurations: " << e.what() << std::endl;
            return false;
        }
    }
    
    void setupMessageHandler() {
        rpc_client_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            try {
                // Update metrics
                metrics_.requests_received.fetch_add(1);
                
                // Security checks
                if (!performSecurityChecks(topic, payload)) {
                    metrics_.requests_failed.fetch_add(1);
                    return;
                }
                
                // Process request
                operation_processor_->processRequest(payload.c_str(), payload.size());
                
                // Update success metrics
                metrics_.requests_processed.fetch_add(1);
                
                // Update response time metrics
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time).count();
                updateAverageResponseTime(duration);
                
                if (logging_config_.enable_metrics_logging) {
                    log(LoggingConfig::Level::DEBUG, 
                        "Processed request in " + std::to_string(duration) + " μs");
                }
                
            } catch (const std::exception& e) {
                metrics_.requests_failed.fetch_add(1);
                log(LoggingConfig::Level::ERROR, "Error processing request: " + std::string(e.what()));
            }
        });
    }
    
    bool performSecurityChecks(const std::string& topic, const std::string& payload) {
        // Topic authorization
        if (!security_config_.isTopicAuthorized(topic)) {
            log(LoggingConfig::Level::WARNING, "Unauthorized topic access: " + topic);
            return false;
        }
        
        // Rate limiting
        if (security_config_.enable_rate_limiting && !checkRateLimit()) {
            log(LoggingConfig::Level::WARNING, "Rate limit exceeded");
            return false;
        }
        
        // Payload size validation
        const size_t MAX_PAYLOAD_SIZE = 10 * 1024 * 1024; // 10MB
        if (payload.size() > MAX_PAYLOAD_SIZE) {
            log(LoggingConfig::Level::WARNING, "Payload too large: " + std::to_string(payload.size()) + " bytes");
            return false;
        }
        
        // Basic JSON validation
        try {
            json::parse(payload);
        } catch (const json::parse_error& e) {
            log(LoggingConfig::Level::WARNING, "Invalid JSON payload: " + std::string(e.what()));
            return false;
        }
        
        return true;
    }
    
    bool checkRateLimit() {
        auto now = std::chrono::steady_clock::now();
        
        // Reset window if needed
        if (now - rate_limit_window_start_ > std::chrono::minutes(1)) {
            rate_limit_window_start_ = now;
            requests_in_current_window_.store(0);
        }
        
        // Check limit
        if (requests_in_current_window_.load() >= security_config_.max_requests_per_minute) {
            return false;
        }
        
        requests_in_current_window_.fetch_add(1);
        return true;
    }
    
    void startMonitoringThreads() {
        // Metrics collection thread
        metrics_thread_ = std::thread([this]() {
            while (running_.load()) {
                try {
                    collectMetrics();
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                } catch (const std::exception& e) {
                    log(LoggingConfig::Level::ERROR, "Error in metrics thread: " + std::string(e.what()));
                }
            }
        });
        
        // Health check thread
        health_check_thread_ = std::thread([this]() {
            while (running_.load()) {
                try {
                    performHealthCheck();
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                } catch (const std::exception& e) {
                    log(LoggingConfig::Level::ERROR, "Error in health check thread: " + std::string(e.what()));
                }
            }
        });
    }
    
    void stopMonitoringThreads() {
        if (metrics_thread_.joinable()) {
            metrics_thread_.join();
        }
        
        if (health_check_thread_.joinable()) {
            health_check_thread_.join();
        }
    }
    
    void collectMetrics() {
        try {
            // Memory usage
            size_t memory_usage = getCurrentMemoryUsage();
            size_t current_peak = metrics_.peak_memory_usage_mb.load();
            if (memory_usage > current_peak) {
                metrics_.peak_memory_usage_mb.store(memory_usage);
            }
            
            // Log metrics if enabled
            if (logging_config_.enable_metrics_logging) {
                json metrics_json = metrics_.toJson();
                log(LoggingConfig::Level::INFO, "Metrics: " + metrics_json.dump());
            }
            
        } catch (const std::exception& e) {
            log(LoggingConfig::Level::ERROR, "Error collecting metrics: " + std::string(e.what()));
        }
    }
    
    void performHealthCheck() {
        try {
            // Check RPC client health
            if (!rpc_client_->isRunning()) {
                log(LoggingConfig::Level::WARNING, "RPC client is not running");
            }
            
            // Check thread manager health
            if (!thread_manager_) {
                log(LoggingConfig::Level::CRITICAL, "Thread manager is null");
                return;
            }
            
            // Check memory usage
            size_t memory_usage = getCurrentMemoryUsage();
            const size_t MEMORY_WARNING_THRESHOLD_MB = 1024; // 1GB
            
            if (memory_usage > MEMORY_WARNING_THRESHOLD_MB) {
                log(LoggingConfig::Level::WARNING, 
                    "High memory usage: " + std::to_string(memory_usage) + " MB");
            }
            
        } catch (const std::exception& e) {
            log(LoggingConfig::Level::ERROR, "Error in health check: " + std::string(e.what()));
        }
    }
    
    void updateAverageResponseTime(uint64_t response_time_us) {
        // Exponential moving average with alpha = 0.1
        const double alpha = 0.1;
        uint64_t current_avg = metrics_.average_response_time_us.load();
        uint64_t new_avg = static_cast<uint64_t>(
            alpha * response_time_us + (1.0 - alpha) * current_avg);
        metrics_.average_response_time_us.store(new_avg);
    }
    
    size_t getCurrentMemoryUsage() {
        // Platform-specific memory usage calculation
        // This is a simplified implementation
        std::ifstream status_file("/proc/self/status");
        std::string line;
        
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string key, value, unit;
                iss >> key >> value >> unit;
                return std::stoul(value) / 1024; // Convert KB to MB
            }
        }
        
        return 0;
    }
    
    void initializeLogging() {
        if (!logging_config_.log_file.empty()) {
            log_file_.open(logging_config_.log_file, std::ios::app);
            if (!log_file_.is_open()) {
                std::cerr << "Failed to open log file: " << logging_config_.log_file << std::endl;
            }
        }
    }
    
    void log(LoggingConfig::Level level, const std::string& message) {
        if (level < logging_config_.min_level) {
            return;
        }
        
        std::string timestamp = getCurrentTimestamp();
        std::string level_str = logLevelToString(level);
        std::string full_message = timestamp + " [" + level_str + "] " + message;
        
        // Console output
        if (logging_config_.enable_console_output) {
            std::cout << full_message << std::endl;
        }
        
        // File output
        if (log_file_.is_open()) {
            std::lock_guard<std::mutex> lock(log_mutex_);
            log_file_ << full_message << std::endl;
            log_file_.flush();
        }
    }
    
    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    LoggingConfig::Level stringToLogLevel(const std::string& level_str) const {
        if (level_str == "DEBUG") return LoggingConfig::Level::DEBUG;
        if (level_str == "INFO") return LoggingConfig::Level::INFO;
        if (level_str == "WARNING") return LoggingConfig::Level::WARNING;
        if (level_str == "ERROR") return LoggingConfig::Level::ERROR;
        if (level_str == "CRITICAL") return LoggingConfig::Level::CRITICAL;
        return LoggingConfig::Level::INFO; // Default
    }
    
    std::string logLevelToString(LoggingConfig::Level level) const {
        switch (level) {
            case LoggingConfig::Level::DEBUG: return "DEBUG";
            case LoggingConfig::Level::INFO: return "INFO";
            case LoggingConfig::Level::WARNING: return "WARNING";
            case LoggingConfig::Level::ERROR: return "ERROR";
            case LoggingConfig::Level::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
    
    bool waitForClientReady() const {
        const int MAX_WAIT_SECONDS = 30;
        const int CHECK_INTERVAL_MS = 500;
        
        int elapsed = 0;
        while (elapsed < MAX_WAIT_SECONDS * 1000) {
            if (rpc_client_->isRunning()) {
                return true;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
            elapsed += CHECK_INTERVAL_MS;
        }
        
        return false;
    }
    
    void setupSignalHandlers() {
        std::signal(SIGINT, [](int) {
            std::cout << "\nReceived SIGINT, shutting down gracefully..." << std::endl;
            // Note: In a real implementation, you'd need access to the instance
            // This is simplified for demonstration
        });
        
        std::signal(SIGTERM, [](int) {
            std::cout << "\nReceived SIGTERM, shutting down gracefully..." << std::endl;
        });
    }
    
    uint64_t getUptimeSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - metrics_.start_time).count();
    }
    
    void reportFinalMetrics() {
        log(LoggingConfig::Level::INFO, "Final Application Metrics:");
        log(LoggingConfig::Level::INFO, "  Total requests received: " + 
            std::to_string(metrics_.requests_received.load()));
        log(LoggingConfig::Level::INFO, "  Total requests processed: " + 
            std::to_string(metrics_.requests_processed.load()));
        log(LoggingConfig::Level::INFO, "  Total requests failed: " + 
            std::to_string(metrics_.requests_failed.load()));
        log(LoggingConfig::Level::INFO, "  Peak memory usage: " + 
            std::to_string(metrics_.peak_memory_usage_mb.load()) + " MB");
        log(LoggingConfig::Level::INFO, "  Total uptime: " + 
            std::to_string(getUptimeSeconds()) + " seconds");
    }
};

/**
 * @brief Main function demonstrating the complete application
 */
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <rpc-config> <package-config>" << std::endl;
        return 1;
    }
    
    try {
        // Create and initialize application
        ProductionRpcApplication app("production-rpc-app");
        
        if (!app.initialize(argv[1], argv[2])) {
            std::cerr << "Failed to initialize application" << std::endl;
            return 1;
        }
        
        // Start the application
        if (!app.start()) {
            std::cerr << "Failed to start application" << std::endl;
            return 1;
        }
        
        // Run the main loop
        app.run();
        
        // Graceful shutdown
        app.shutdown();
        
        std::cout << "Application terminated successfully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
```

---

## Conclusion

This comprehensive guide demonstrates the complete integration of the **UR-RPC Template** into C++ applications using the proven patterns from ur-licence-mann. The integration provides:

### Key Achievements

1. **Robust Architecture**: Thread-safe, scalable, and maintainable design
2. **Production Ready**: Comprehensive error handling, monitoring, and security
3. **Performance Optimized**: Efficient resource utilization and concurrent processing
4. **Developer Friendly**: Clean APIs, extensive documentation, and examples
5. **Operationally Sound**: Easy deployment, monitoring, and troubleshooting

### Integration Benefits

- **Reduced Development Time**: Proven patterns accelerate implementation
- **Lower Maintenance Costs**: Well-structured code is easier to maintain
- **Improved Reliability**: Comprehensive error handling and recovery
- **Better Performance**: Optimized threading and resource management
- **Enhanced Security**: Built-in security features and best practices

### Next Steps

1. **Adapt the Patterns**: Modify the examples to fit your specific use case
2. **Implement Business Logic**: Replace the placeholder operations with your actual functionality
3. **Configure for Production**: Adjust security, performance, and monitoring settings
4. **Test Thoroughly**: Use the provided testing strategies to validate your implementation
5. **Deploy Gradually**: Use the migration guide for smooth transition from existing systems

The UR-RPC Template integration demonstrated in this guide provides a solid foundation for building robust, scalable, and maintainable RPC-based applications in C++. By following these patterns and best practices, you can create production-ready systems that meet the demanding requirements of modern distributed applications.

---

*This guide is based on the real-world implementation from ur-licence-mann and has been generalized to demonstrate universal patterns for UR-RPC Template integration in C++ applications.*
