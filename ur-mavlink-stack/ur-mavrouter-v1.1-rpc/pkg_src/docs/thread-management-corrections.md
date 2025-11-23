# Thread Management Implementation Corrections

## Summary

Based on analysis of the original ur-mavrouter-v1.1 implementation, the RPC client and startup mechanism have been corrected to properly launch both mainloop and extension threads as separate threads.

## Key Corrections Made

### 1. HTTP Server Disabled
- HTTP server support has been disabled via `cmake -D_BUILD_HTTP=OFF`
- This removes dependency on HTTP endpoints for thread management
- Focus is now on RPC-based device discovery and startup

### 2. Mainloop Startup Callback Fixed

**Before**: The mainloop startup callback was not properly loading extensions after mainloop startup
**After**: Corrected to follow the original pattern:

```cpp
// Start mainloop
auto startResult = this->startThread(ThreadTarget::MAINLOOP);
if (startResult.status == OperationStatus::SUCCESS) {
    // Wait for mainloop to initialize
    while (!isMainloopRunning() && retryCount < maxRetries) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retryCount++;
    }
    
    // Load and start extension configurations
    auto* extensionManager = operations_->getExtensionManager();
    if (extensionManager) {
        // Load configs if needed
        if (allExtensions.empty()) {
            extensionManager->loadExtensionConfigs(extensionConfDir);
        }
        
        // Start all extensions
        for (const auto& extInfo : allExtensions) {
            extensionManager->startExtension(extInfo.name);
        }
    }
}
```

### 3. handleDeviceAddedEvent Method Corrected

**Before**: Only started mainloop, extensions were not loaded automatically
**After**: Properly loads and starts extensions after mainloop is ready:

- Starts mainloop thread first
- Waits for mainloop to be ready (up to 2.5 seconds)
- Loads extension configurations from "config" directory
- Starts all extension threads using ExtensionManager directly
- Each extension runs as a separate thread with isolated mainloop instance

### 4. RPC Message Handler Fixed

**Before**: When mainloop was already running, extensions were not being started
**After**: Properly handles both scenarios:

- **Mainloop not running**: Starts mainloop, waits for readiness, then loads/starts extensions
- **Mainloop already running**: Directly loads and starts extensions

### 5. Extension Thread Management

**Corrected Implementation**:
- Extensions are loaded via `ExtensionManager::loadExtensionConfigs("config")`
- Each extension configuration JSON file creates a separate thread
- Extension threads are created via `ThreadManager::createThread()`
- Each extension has its own isolated mainloop instance
- Extensions are registered with ThreadManager using attachment IDs like "extension_<name>"

## Thread Launch Sequence

### Device Discovery Trigger
1. **DeviceDiscoveryCronJob** discovers devices via ur-mavdiscovery
2. For each discovered device, calls `mainloopStartupCallback()`
3. Mainloop startup callback starts mainloop and extensions

### Mainloop Startup
1. **mainloop thread** created via `startThread(ThreadTarget::MAINLOOP)`
2. Mainloop initializes global configuration
3. Waits for mainloop to be ready (`isMainloopRunning()` check)

### Extension Startup
1. **ExtensionManager::loadExtensionConfigs()** loads JSON configs
2. For each config, **ExtensionManager::createExtension()** creates thread
3. **launchExtensionThread()** creates separate thread for each extension
4. Each extension runs with isolated mainloop instance and endpoints

## Thread Architecture

```
Main Thread (ur-mavrouter)
├── Device Discovery Thread
├── Mainloop Thread (main communication hub)
└── Extension Threads (separate for each extension)
    ├── extension_test_extension_1
    ├── extension_test_extension_2
    └── ... (one thread per extension config)
```

## Key Files Modified

1. **RpcControllerNew.cpp**:
   - Fixed `mainloopStartupCallback` to load extensions
   - Fixed `handleDeviceAddedEvent` to start extensions
   - Fixed RPC message handler for both scenarios

2. **DeviceDiscoveryCronJob.cpp**:
   - Removed test fallback mechanism
   - Restored original device discovery logic

3. **Build Configuration**:
   - HTTP server disabled (`_BUILD_HTTP=OFF`)
   - Focus on RPC-based startup mechanism

## Verification

The implementation now correctly follows the original pattern:

1. **Sequential Dependency**: Mainloop starts before extensions
2. **Separate Threads**: Each extension runs in its own thread
3. **Proper Registration**: All threads registered with ThreadManager
4. **Graceful Startup**: Waits for mainloop readiness before loading extensions
5. **Configuration Loading**: Extensions loaded from JSON config files

## Usage

When a device is discovered (via ur-mavdiscovery), the system will:

1. Automatically start the mainloop thread
2. Load extension configurations from `config/extension_*.json`
3. Start each extension as a separate thread
4. All threads run independently and communicate via extension points

The system is now ready to handle device discovery and automatically launch both mainloop and extension threads correctly.
