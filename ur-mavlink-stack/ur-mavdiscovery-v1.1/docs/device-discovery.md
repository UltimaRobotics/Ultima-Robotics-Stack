
# Device Discovery Mechanism

## Overview

The device discovery system uses Linux udev to monitor hardware events in real-time, detecting when serial devices are added or removed from the system.

## udev Integration

### Initialization

The DeviceMonitor creates a udev context and monitor:

```cpp
udev_ = udev_new();
monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
udev_monitor_filter_add_match_subsystem_devtype(monitor_, "tty", nullptr);
udev_monitor_enable_receiving(monitor_);
```

### Event Types

The monitor watches for:
- **add**: New device connected
- **remove**: Device disconnected

### Device Filtering

Devices are filtered based on path prefixes defined in configuration:

```json
{
  "devicePathFilters": [
    "/dev/ttyUSB",
    "/dev/ttyACM",
    "/dev/ttyS"
  ]
}
```

Only devices matching these filters are processed.

## Monitoring Thread

### Event Loop

The monitor runs in a dedicated thread using select() for efficient I/O:

```cpp
while (running_) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
    if (ret > 0 && FD_ISSET(fd, &fds)) {
        struct udev_device* dev = udev_monitor_receive_device(monitor_);
        // Process device event
    }
}
```

### Thread Lifecycle

1. Thread created via ThreadManager
2. Registered with identifier "device_monitor"
3. Runs until stop() is called
4. Graceful shutdown with timeout
5. Forceful stop if timeout exceeded

## Event Handling

### Device Add

When a device is added:
1. Check against path filters
2. Add to DeviceStateDB
3. Trigger add callback to DeviceManager
4. DeviceManager spawns DeviceVerifier

### Device Remove

When a device is removed:
1. Update DeviceStateDB
2. Trigger remove callback to DeviceManager
3. DeviceManager stops associated verifier

## Callback Mechanism

The DeviceMonitor uses std::function callbacks:

```cpp
void setAddCallback(std::function<void(const std::string&)> callback);
void setRemoveCallback(std::function<void(const std::string&)> callback);
```

This allows loose coupling between monitor and manager.

## Error Handling

- Failed udev initialization: Returns false from start()
- Monitor creation failure: Cleanup and return false
- Device event errors: Logged but don't stop monitoring
