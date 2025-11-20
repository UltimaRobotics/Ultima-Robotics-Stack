
# VPN Instance Manager Architecture

## Overview

The VPN Instance Manager is a multi-threaded application that manages multiple VPN instances (OpenVPN, WireGuard) in parallel. It uses existing wrapper libraries and the thread management API to launch, monitor, and control VPN connections based on JSON configuration.

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              VPN Instance Manager                    │   │
│  │  - JSON Config Parser                                │   │
│  │  - Instance Lifecycle Manager                        │   │
│  │  - Thread Registry & Coordination                    │   │
│  │  - Event Aggregator & Monitoring                     │   │
│  └─────────────────────────────────────────────────────┘   │
│                            │                                  │
│                            ↓                                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │           Thread Management Layer                    │   │
│  │  (ur-threadder-api)                                  │   │
│  │  - ThreadManager (C++ wrapper)                       │   │
│  │  - Thread registration & attachment                  │   │
│  │  - Process monitoring & control                      │   │
│  └─────────────────────────────────────────────────────┘   │
│                            │                                  │
│                            ↓                                  │
│  ┌──────────────┬──────────────┬──────────────────────┐   │
│  │  VPN Parser  │   OpenVPN    │    WireGuard         │   │
│  │   Layer      │   Wrapper    │    Wrapper           │   │
│  │              │   Library    │    Library           │   │
│  │  - Protocol  │              │                      │   │
│  │    Detection │  - C Bridge  │  - C Bridge          │   │
│  │  - Config    │  - C++       │  - C++ Wrapper       │   │
│  │    Parsing   │    Wrapper   │  - Event Callbacks   │   │
│  │  - Validation│  - JSON      │  - Stats Collection  │   │
│  │              │    Events    │                      │   │
│  └──────────────┴──────────────┴──────────────────────┘   │
│                            │                                  │
└────────────────────────────┼──────────────────────────────────┘
                             ↓
                    ┌──────────────────┐
                    │  VPN Protocols   │
                    │  - OpenVPN       │
                    │  - WireGuard     │
                    └──────────────────┘
```

## Core Components

### 1. JSON Configuration Schema

The system accepts a JSON configuration defining VPN instances:

```json
{
  "instances": [
    {
      "name": "corporate_vpn",
      "type": "openvpn",
      "config_file_content": "<base64 or raw config>",
      "enabled": true,
      "auto_reconnect": true,
      "metadata": {
        "description": "Corporate office VPN",
        "priority": 1
      }
    },
    {
      "name": "home_wireguard",
      "type": "wireguard",
      "config_file_content": "[Interface]\n...",
      "enabled": false,
      "auto_reconnect": false,
      "metadata": {
        "description": "Home server WireGuard",
        "priority": 2
      }
    }
  ],
  "global_settings": {
    "max_concurrent_connections": 5,
    "reconnect_delay_seconds": 30,
    "stats_interval_seconds": 10,
    "log_level": "info"
  }
}
```

### 2. VPN Instance Manager Core

**File**: `ur-vpn-mann/src/vpn_instance_manager.hpp`

```cpp
class VPNInstanceManager {
public:
    // Configuration & Lifecycle
    bool loadConfiguration(const std::string& json_config);
    bool saveConfiguration(const std::string& filepath);
    
    // Instance Control
    bool startInstance(const std::string& instance_name);
    bool stopInstance(const std::string& instance_name);
    bool restartInstance(const std::string& instance_name);
    bool startAllEnabled();
    bool stopAll();
    
    // Status & Monitoring
    json getInstanceStatus(const std::string& instance_name);
    json getAllInstancesStatus();
    json getAggregatedStats();
    
    // Event Handling
    void setGlobalEventCallback(EventCallback callback);
    
private:
    struct VPNInstance {
        std::string name;
        VPNType type;
        std::string config_content;
        bool enabled;
        bool auto_reconnect;
        json metadata;
        
        // Runtime state
        unsigned int thread_id;
        ConnectionState current_state;
        time_t start_time;
        std::shared_ptr<void> wrapper_instance;
    };
    
    std::map<std::string, VPNInstance> instances_;
    ThreadMgr::ThreadManager thread_manager_;
    std::mutex instances_mutex_;
    EventCallback global_event_callback_;
};
```

### 3. Instance Wrapper Integration

**File**: `ur-vpn-mann/src/vpn_wrapper_factory.hpp`

The factory creates appropriate wrapper instances based on VPN type:

```cpp
class VPNWrapperFactory {
public:
    enum class WrapperType {
        OPENVPN,
        WIREGUARD
    };
    
    struct WrapperInterface {
        virtual ~WrapperInterface() = default;
        virtual bool initialize(const std::string& config) = 0;
        virtual bool connect() = 0;
        virtual bool disconnect() = 0;
        virtual bool reconnect() = 0;
        virtual json getStatus() = 0;
        virtual json getStats() = 0;
        virtual bool isConnected() = 0;
    };
    
    static std::shared_ptr<WrapperInterface> createWrapper(
        WrapperType type,
        const std::string& instance_name
    );
};

// OpenVPN Wrapper Adapter
class OpenVPNWrapperAdapter : public VPNWrapperFactory::WrapperInterface {
private:
    std::unique_ptr<openvpn::OpenVPNWrapper> wrapper_;
    std::string config_file_path_;
    
public:
    bool initialize(const std::string& config) override;
    bool connect() override;
    bool disconnect() override;
    bool reconnect() override;
    json getStatus() override;
    json getStats() override;
    bool isConnected() override;
};

// WireGuard Wrapper Adapter
class WireGuardWrapperAdapter : public VPNWrapperFactory::WrapperInterface {
private:
    std::unique_ptr<wireguard::WireGuardWrapper> wrapper_;
    std::string config_file_path_;
    
public:
    bool initialize(const std::string& config) override;
    bool connect() override;
    bool disconnect() override;
    bool reconnect() override;
    json getStatus() override;
    json getStats() override;
    bool isConnected() override;
};
```

### 4. Thread Management Integration

**File**: `ur-vpn-mann/src/thread_coordinator.hpp`

Coordinates thread lifecycle using ur-threadder-api:

```cpp
class ThreadCoordinator {
public:
    explicit ThreadCoordinator(ThreadMgr::ThreadManager& manager);
    
    // Thread lifecycle with VPN wrapper
    unsigned int launchVPNThread(
        const std::string& instance_name,
        std::shared_ptr<VPNWrapperFactory::WrapperInterface> wrapper
    );
    
    bool stopVPNThread(const std::string& instance_name);
    bool restartVPNThread(const std::string& instance_name);
    
    // Thread monitoring
    bool isThreadAlive(const std::string& instance_name);
    ThreadMgr::ThreadState getThreadState(const std::string& instance_name);
    
private:
    ThreadMgr::ThreadManager& thread_manager_;
    
    // Worker function for VPN thread
    static void vpnWorkerFunction(
        std::shared_ptr<VPNWrapperFactory::WrapperInterface> wrapper,
        std::atomic<bool>& should_stop
    );
};
```

### 5. Configuration Parser & Validator

**File**: `ur-vpn-mann/src/config_parser.hpp`

Parses and validates JSON configuration:

```cpp
class ConfigParser {
public:
    struct ParsedConfig {
        std::vector<VPNInstanceConfig> instances;
        GlobalSettings global_settings;
        bool is_valid;
        std::string error_message;
    };
    
    static ParsedConfig parseFromJson(const std::string& json_str);
    static ParsedConfig parseFromFile(const std::string& filepath);
    
    // Validation using ur-vpn-parser
    static bool validateInstanceConfig(
        const std::string& config_content,
        const std::string& vpn_type,
        std::string& error_msg
    );
    
private:
    static bool validateOpenVPNConfig(const std::string& config);
    static bool validateWireGuardConfig(const std::string& config);
};
```

### 6. Event Aggregator & Monitoring

**File**: `ur-vpn-mann/src/event_aggregator.hpp`

Aggregates events from all VPN instances:

```cpp
class EventAggregator {
public:
    struct AggregatedEvent {
        std::string instance_name;
        std::string event_type;
        std::string message;
        json data;
        time_t timestamp;
    };
    
    using AggregatedEventCallback = std::function<void(const AggregatedEvent&)>;
    
    void registerInstanceCallback(
        const std::string& instance_name,
        std::function<void(const json&)> callback
    );
    
    void setAggregatedCallback(AggregatedEventCallback callback);
    
    // Stats collection
    json collectAllStats();
    json getInstanceHistory(const std::string& instance_name);
    
private:
    std::map<std::string, std::vector<AggregatedEvent>> event_history_;
    AggregatedEventCallback aggregated_callback_;
    std::mutex event_mutex_;
};
```

## Main Application Flow

### Initialization Sequence

```
1. Parse command-line arguments (config file path)
2. Load and parse JSON configuration
3. Initialize ThreadManager with appropriate capacity
4. Validate all instance configurations using ur-vpn-parser
5. Create VPN wrapper instances for each enabled instance
6. Register event callbacks for monitoring
7. Launch threads for enabled instances
8. Enter main monitoring loop
```

### Main Loop Operations

```cpp
// Main loop (pseudo-code)
while (running) {
    // 1. Check thread health
    for (auto& instance : instances) {
        if (instance.enabled && !thread_alive(instance.name)) {
            if (instance.auto_reconnect) {
                log("Restarting instance: " + instance.name);
                restart_instance(instance.name);
            }
        }
    }
    
    // 2. Collect and log stats
    if (stats_interval_elapsed()) {
        json all_stats = collect_all_stats();
        log_stats(all_stats);
    }
    
    // 3. Process user commands (if interactive mode)
    process_user_input();
    
    // 4. Sleep
    sleep(monitoring_interval);
}
```

### Shutdown Sequence

```
1. Set global shutdown flag
2. Stop all VPN threads gracefully
3. Wait for threads to exit (with timeout)
4. Save current state/configuration
5. Cleanup resources
6. Exit
```

## Main Application Structure

**File**: `ur-vpn-mann/src/main.cpp`

```cpp
int main(int argc, char* argv[]) {
    // 1. Parse arguments
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string config_file = argv[1];
    
    // 2. Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 3. Initialize VPN Instance Manager
    VPNInstanceManager manager;
    
    // 4. Load configuration
    if (!manager.loadConfiguration(config_file)) {
        std::cerr << "Failed to load configuration" << std::endl;
        return 1;
    }
    
    // 5. Set global event callback for monitoring
    manager.setGlobalEventCallback([](const auto& event) {
        json event_json = {
            {"instance", event.instance_name},
            {"type", event.event_type},
            {"message", event.message},
            {"timestamp", event.timestamp}
        };
        std::cout << event_json.dump() << std::endl;
    });
    
    // 6. Start all enabled instances
    if (!manager.startAllEnabled()) {
        std::cerr << "Failed to start instances" << std::endl;
        return 1;
    }
    
    // 7. Main monitoring loop
    while (g_running) {
        // Get aggregated status
        json status = manager.getAllInstancesStatus();
        
        // Check for failures and handle reconnection
        // ... implementation details ...
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    // 8. Cleanup
    manager.stopAll();
    
    std::cout << "VPN Instance Manager stopped" << std::endl;
    return 0;
}
```

## Thread Launch Strategy

### Per-Instance Thread Function

```cpp
void VPNInstanceManager::launchInstanceThread(VPNInstance& instance) {
    // Create wrapper based on type
    auto wrapper = VPNWrapperFactory::createWrapper(
        instance.type == VPNType::OPENVPN ? 
            VPNWrapperFactory::WrapperType::OPENVPN :
            VPNWrapperFactory::WrapperType::WIREGUARD,
        instance.name
    );
    
    // Initialize wrapper with config
    if (!wrapper->initialize(instance.config_content)) {
        log_error("Failed to initialize " + instance.name);
        return;
    }
    
    // Set event callback to forward to aggregator
    // (Implementation depends on wrapper interface extension)
    
    // Create thread using ThreadManager
    auto thread_func = [wrapper, &instance, this]() {
        // Connect VPN
        if (!wrapper->connect()) {
            log_error("Failed to connect " + instance.name);
            return;
        }
        
        // Monitor loop
        while (!should_exit(instance.thread_id)) {
            if (!wrapper->isConnected()) {
                if (instance.auto_reconnect) {
                    log_info("Reconnecting " + instance.name);
                    wrapper->reconnect();
                } else {
                    break;
                }
            }
            
            // Collect stats periodically
            json stats = wrapper->getStats();
            emit_stats_event(instance.name, stats);
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        // Cleanup
        wrapper->disconnect();
    };
    
    // Launch thread with registration
    instance.thread_id = thread_manager_.createThread(thread_func);
    thread_manager_.registerThread(instance.thread_id, instance.name);
}
```

## Error Handling & Recovery

### Connection Failure Handling

```cpp
1. Detect connection failure via wrapper->isConnected()
2. Log failure event with details
3. Check auto_reconnect flag
4. If enabled:
   - Wait reconnect_delay_seconds
   - Attempt reconnection via wrapper->reconnect()
   - Retry with exponential backoff (max 3 attempts)
5. If disabled or max retries exceeded:
   - Mark instance as failed
   - Stop thread
   - Emit failure event
```

### Thread Crash Recovery

```cpp
1. Monitor thread liveness via ThreadManager
2. Detect crashed thread (not alive, not in stopped state)
3. Log crash event
4. Clean up resources
5. If auto_reconnect enabled:
   - Create new thread with fresh wrapper instance
   - Register with same attachment name
6. Update instance status
```

## Data Flow Diagrams

### Instance Startup Flow

```
JSON Config → ConfigParser → Validation (ur-vpn-parser)
                                   ↓
                          VPNWrapperFactory
                                   ↓
                    [OpenVPN | WireGuard] Wrapper
                                   ↓
                          ThreadManager.createThread()
                                   ↓
                          Thread Registration
                                   ↓
                          wrapper->connect()
                                   ↓
                          Monitoring Loop
```

### Event Flow

```
VPN Wrapper Event → Wrapper Callback → EventAggregator
                                              ↓
                                    Global Event Callback
                                              ↓
                                    [JSON Output | Logger]
```

### Stats Collection Flow

```
Periodic Timer → VPNInstanceManager
                        ↓
              For each instance:
              wrapper->getStats()
                        ↓
              EventAggregator.collectAllStats()
                        ↓
              Aggregated JSON Output
```

## File Structure

```
ur-vpn-mann/
├── docs/
│   └── vpn-instance-manager-architecture.md (this file)
├── src/
│   ├── main.cpp
│   ├── vpn_instance_manager.hpp
│   ├── vpn_instance_manager.cpp
│   ├── vpn_wrapper_factory.hpp
│   ├── vpn_wrapper_factory.cpp
│   ├── thread_coordinator.hpp
│   ├── thread_coordinator.cpp
│   ├── config_parser.hpp
│   ├── config_parser.cpp
│   ├── event_aggregator.hpp
│   └── event_aggregator.cpp
├── include/
│   └── (public headers if needed)
├── CMakeLists.txt
└── README.md
```

## Dependencies

### Internal Dependencies
- `ur-openvpn-library` - OpenVPN wrapper and bridge
- `ur-wg_library/wireguard-wrapper` - WireGuard wrapper and bridge
- `ur-vpn-parser` - Configuration parsing and validation
- `ur-threadder-api` - Thread management (C++ wrapper)

### External Dependencies
- `nlohmann/json` - JSON parsing (already available)
- C++17 or later
- POSIX threads (pthread)
- Standard C++ libraries

## CMake Build Configuration

```cmake
cmake_minimum_required(VERSION 3.10)
project(vpn-instance-manager)

set(CMAKE_CXX_STANDARD 17)

# Add subdirectories for dependencies
add_subdirectory(ur-openvpn-library)
add_subdirectory(ur-wg_library/wireguard-wrapper)
add_subdirectory(ur-vpn-parser)
add_subdirectory(ur-threadder-api/cpp)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ur-openvpn-library/src
    ${CMAKE_SOURCE_DIR}/ur-wg_library/wireguard-wrapper/include
    ${CMAKE_SOURCE_DIR}/ur-vpn-parser
    ${CMAKE_SOURCE_DIR}/ur-threadder-api/cpp/include
)

# Source files
set(SOURCES
    src/main.cpp
    src/vpn_instance_manager.cpp
    src/vpn_wrapper_factory.cpp
    src/thread_coordinator.cpp
    src/config_parser.cpp
    src/event_aggregator.cpp
)

# Executable
add_executable(vpn-instance-manager ${SOURCES})

# Link libraries
target_link_libraries(vpn-instance-manager
    openvpn_wrapper
    wireguard_wrapper
    vpn_parser
    threadmanager_cpp
    pthread
)
```

## Usage Examples

### Start VPN Instance Manager

```bash
# With configuration file
./vpn-instance-manager config.json

# Output: JSON events to stdout
./vpn-instance-manager config.json | jq .

# Save logs to file
./vpn-instance-manager config.json > vpn-manager.log 2>&1
```

### Sample Configuration

```json
{
  "instances": [
    {
      "name": "office_openvpn",
      "type": "openvpn",
      "config_file_content": "client\nremote vpn.company.com 1194\n...",
      "enabled": true,
      "auto_reconnect": true
    },
    {
      "name": "home_wg",
      "type": "wireguard",
      "config_file_content": "[Interface]\nPrivateKey=...\n[Peer]\n...",
      "enabled": true,
      "auto_reconnect": true
    }
  ],
  "global_settings": {
    "max_concurrent_connections": 5,
    "reconnect_delay_seconds": 30,
    "stats_interval_seconds": 10
  }
}
```

### JSON Output Events

```json
{
  "instance": "office_openvpn",
  "type": "connected",
  "message": "VPN connection established",
  "timestamp": 1736276400,
  "data": {
    "local_ip": "10.8.0.2",
    "server_ip": "10.8.0.1"
  }
}

{
  "instance": "home_wg",
  "type": "stats",
  "message": "Statistics update",
  "timestamp": 1736276410,
  "data": {
    "bytes_sent": 1048576,
    "bytes_received": 2097152,
    "latency_ms": 45
  }
}
```

## Security Considerations

1. **Configuration Storage**: Sensitive config data (private keys, passwords) should be encrypted at rest
2. **Memory Protection**: Clear sensitive data from memory after use
3. **File Permissions**: Restrict config file permissions (0600)
4. **Thread Isolation**: Each VPN instance runs in isolated thread context
5. **Error Logging**: Sanitize logs to avoid exposing sensitive information

## Performance Characteristics

- **Thread Overhead**: One thread per active VPN instance
- **Memory Usage**: ~5-10MB per VPN instance (wrapper + buffers)
- **CPU Usage**: Minimal when idle, ~2-5% per active connection
- **Startup Time**: ~1-3 seconds per instance
- **Max Concurrent Instances**: Configurable, recommended < 20

## Extension Points

1. **Additional VPN Types**: Extend VPNWrapperFactory for new protocols
2. **Custom Event Handlers**: Implement custom EventAggregator handlers
3. **Remote Control**: Add HTTP/REST API for runtime control
4. **Persistence**: Add state persistence for restart recovery
5. **Metrics Export**: Add Prometheus/StatsD metrics export
