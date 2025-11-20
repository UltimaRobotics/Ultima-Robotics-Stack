
# State Management

## Overview

The DeviceStateDB provides centralized, thread-safe storage for all discovered and verified devices.

## DeviceState Enum

```cpp
enum class DeviceState {
    UNKNOWN,      // Initial state
    VERIFYING,    // Verification in progress
    VERIFIED,     // MAVLink device confirmed
    NON_MAVLINK,  // Not a MAVLink device
    REMOVED       // Device disconnected
};
```

## DeviceInfo Structure

```cpp
struct DeviceInfo {
    std::string devicePath;               // Device file path
    std::atomic<DeviceState> state;       // Current state
    int baudrate;                         // Working baudrate
    uint8_t sysid;                        // MAVLink system ID
    uint8_t compid;                       // MAVLink component ID
    uint8_t mavlinkVersion;               // 1 or 2
    std::vector<MAVLinkMessage> messages; // Received messages
    std::string timestamp;                // Discovery time
};
```

## Singleton Pattern

### Access

```cpp
DeviceStateDB& db = DeviceStateDB::getInstance();
```

Benefits:
- Single source of truth
- Global access point
- Controlled initialization

## Operations

### Add Device

```cpp
void addDevice(const std::string& devicePath);
```

Called when:
- Device detected by monitor
- Creates initial DeviceInfo
- Sets state to UNKNOWN

### Update Device

```cpp
void updateDevice(const std::string& devicePath, const DeviceInfo& info);
```

Called when:
- Verification completes
- State changes
- New information available

Thread-safe:
- Mutex-protected
- Atomic state updates

### Remove Device

```cpp
void removeDevice(const std::string& devicePath);
```

Called when:
- Device disconnected
- Sets state to REMOVED
- Removes from database

### Query Device

```cpp
std::shared_ptr<DeviceInfo> getDevice(const std::string& devicePath);
```

Returns:
- Shared pointer to device info
- nullptr if not found

### Get All Devices

```cpp
std::vector<std::shared_ptr<DeviceInfo>> getAllDevices();
```

Returns:
- Vector of all device infos
- Snapshot at call time

## Thread Safety

### Mutex Protection

All operations use mutex:

```cpp
std::lock_guard<std::mutex> lock(mutex_);
// Critical section
```

### Atomic State

Device state uses atomic type:

```cpp
std::atomic<DeviceState> state;
```

Operations:
- `state.load()`: Read current state
- `state.store()`: Update state
- Lock-free operations

### Shared Pointers

Device info returned as shared_ptr:
- Thread-safe reference counting
- Automatic memory management
- Safe concurrent access

## State Transitions

### Normal Flow

```
UNKNOWN → VERIFYING → VERIFIED
                   → NON_MAVLINK
```

### Device Removal

```
Any State → REMOVED
```

### State Machine

States are mutually exclusive:
- One state at a time
- Atomic transitions
- No intermediate states

## Usage Patterns

### Device Discovery

```cpp
// Monitor adds device
db.addDevice("/dev/ttyUSB0");

// Verifier updates during verification
DeviceInfo info;
info.devicePath = "/dev/ttyUSB0";
info.state = DeviceState::VERIFYING;
db.updateDevice("/dev/ttyUSB0", info);

// Verifier updates on completion
info.state = DeviceState::VERIFIED;
info.baudrate = 57600;
info.sysid = 1;
info.compid = 1;
db.updateDevice("/dev/ttyUSB0", info);
```

### Device Query

```cpp
auto device = db.getDevice("/dev/ttyUSB0");
if (device) {
    if (device->state.load() == DeviceState::VERIFIED) {
        // Use device information
    }
}
```

### Device Enumeration

```cpp
auto devices = db.getAllDevices();
for (const auto& device : devices) {
    LOG_INFO("Device: " + device->devicePath + 
             " State: " + stateToString(device->state.load()));
}
```

## Logging

All state changes are logged:
- Device addition
- State updates
- Device removal

Aids in:
- Debugging
- Monitoring
- Audit trail
