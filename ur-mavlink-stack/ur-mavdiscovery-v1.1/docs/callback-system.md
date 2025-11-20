
# Callback System

## Overview

The CallbackDispatcher provides a publish-subscribe mechanism for device verification events, enabling loose coupling between components.

## Architecture

### Singleton Pattern

```cpp
CallbackDispatcher& dispatcher = CallbackDispatcher::getInstance();
```

Global event notification system.

### Callback Type

```cpp
using DeviceCallback = std::function<void(const DeviceInfo&)>;
```

Callbacks receive complete device information.

## Operations

### Register Callback

```cpp
void registerCallback(DeviceCallback callback);
```

Example:

```cpp
dispatcher.registerCallback([](const DeviceInfo& info) {
    if (info.state.load() == DeviceState::VERIFIED) {
        std::cout << "Device verified: " << info.devicePath << std::endl;
    }
});
```

Multiple callbacks:
- Unlimited registrations
- All callbacks invoked on events
- Order of execution not guaranteed

### Notify

```cpp
void notify(const DeviceInfo& info);
```

Called by:
- DeviceVerifier on completion
- Triggers all registered callbacks

## Use Cases

### Logging

```cpp
dispatcher.registerCallback([](const DeviceInfo& info) {
    LOG_INFO("Device state changed: " + info.devicePath + 
             " -> " + stateToString(info.state.load()));
});
```

### Statistics

```cpp
static int verifiedCount = 0;
dispatcher.registerCallback([](const DeviceInfo& info) {
    if (info.state.load() == DeviceState::VERIFIED) {
        verifiedCount++;
        LOG_INFO("Total verified devices: " + std::to_string(verifiedCount));
    }
});
```

### Custom Actions

```cpp
dispatcher.registerCallback([](const DeviceInfo& info) {
    if (info.state.load() == DeviceState::VERIFIED) {
        // Send to external system
        // Update UI
        // Trigger automation
    }
});
```

## Thread Safety

### Mutex Protection

```cpp
std::lock_guard<std::mutex> lock(mutex_);
callbacks_.push_back(callback);
```

### Notification

All callbacks executed sequentially:
- Under mutex protection
- No concurrent callback execution
- Exception handling per callback

## Error Handling

### Exception Safety

```cpp
for (const auto& callback : callbacks_) {
    try {
        callback(info);
    } catch (const std::exception& e) {
        LOG_ERROR("Callback exception: " + std::string(e.what()));
    }
}
```

Benefits:
- One callback failure doesn't affect others
- Errors logged
- System continues

## Integration with HTTP

The HTTPClient uses the callback system indirectly:

```cpp
// In DeviceVerifier
CallbackDispatcher::getInstance().notify(info);

if (config_.enableHTTP && verified) {
    HTTPClient client(config_.httpEndpoint, config_.httpTimeoutMs);
    client.postDeviceInfo(info);
}
```

## Best Practices

1. **Keep callbacks fast**: Don't block
2. **Handle exceptions**: Prevent callback failures
3. **Avoid heavy operations**: Offload to threads if needed
4. **Thread-safe access**: If accessing shared data
5. **Register early**: Before device discovery starts
