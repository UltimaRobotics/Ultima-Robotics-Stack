# USB Bus Tracking for Duplicate Device Detection

## Overview

This document describes the implementation of USB bus tracking in ur-mavdiscovery-v1.1 to handle devices with the same serial number as a single physical device.

## Problem Statement

Previously, when a USB device created multiple `/dev/ttyACM` paths (e.g., `/dev/ttyACM1` and `/dev/ttyACM2`) with the same serial number, the system would treat them as separate devices and send duplicate notifications to `ur-mavrouter` and `ur-mavcollector`.

## Solution

### Key Components

#### 1. Enhanced USBDeviceInfo Structure

Added new fields to track USB bus information:
```cpp
struct USBDeviceInfo {
    // ... existing fields ...
    std::string usbBusNumber;     // USB bus number (e.g., "1", "2")
    std::string usbDeviceAddress; // USB device address on the bus (e.g., "1", "2")
    std::string physicalDeviceId; // Unique identifier for the physical device (bus:vendor:product:serial)
};
```

#### 2. USBDeviceTracker Class

New singleton class that manages physical device tracking:
- **Location**: `src/USBDeviceTracker.cpp` and `include/USBDeviceTracker.hpp`
- **Purpose**: Tracks physical devices and their multiple device paths
- **Key Methods**:
  - `registerDevice()`: Registers a device path to a physical device
  - `removeDevice()`: Removes a device path and updates primary if needed
  - `isPrimaryPath()`: Checks if a path is the primary path for its physical device
  - `getPhysicalDeviceId()`: Gets the physical device ID for a path

#### 3. Physical Device ID Generation

The system generates a unique physical device ID using the format:
```
bus:vendor:product:serial
```

Example: `1:3162:0053:3B0032001651333337363133`

### Device Detection Logic

#### Device Verification Process

1. **USB Information Extraction**: Enhanced `DeviceVerifier::extractUSBInfo()` to extract:
   - USB bus number (`busnum`)
   - USB device address (`devnum`)
   - Generate physical device ID

2. **Device Registration**: When a device is verified:
   - Check if physical device ID already exists
   - If new: Register as new physical device with primary path
   - If existing: Add as secondary path, potentially update primary if lower ACM number

3. **Primary Path Selection**: The system prefers lower-numbered ACM devices as primary paths:
   - `/dev/ttyACM0` over `/dev/ttyACM1`
   - `/dev/ttyACM1` over `/dev/ttyACM2`

#### Notification Handling

- **Primary Paths**: Send RPC and shared bus notifications
- **Secondary Paths**: Skip notifications to prevent duplicates
- **Device Removal**: Only send removal notifications for primary paths, then promote secondary if available

## Implementation Details

### Modified Files

1. **`include/DeviceInfo.hpp`**: Added USB tracking fields
2. **`src/DeviceVerifier.cpp`**: Enhanced USB info extraction
3. **`src/DeviceManager.cpp`**: Integrated USB tracking into device lifecycle
4. **`src/DeviceStateDB.cpp`**: Store USB tracking info
5. **`src/DeviceDiscoveryCronJob.cpp`**: Include tracking in notifications
6. **`ur-mavdiscovery-shared/include/MavlinkDeviceStructs.hpp`**: Updated shared structures
7. **`CMakeLists.txt`**: Added new source file

### New Files

1. **`include/USBDeviceTracker.hpp`**: Header for device tracking
2. **`src/USBDeviceTracker.cpp`**: Implementation of device tracking

### Example Log Output

```
[INFO] USB Info - Manufacturer: Holybro, Serial: 3B0032001651333337363133, VID: 3162, PID: 0053, Bus: 1, DevAddr: 2, PhysicalID: 1:3162:0053:3B0032001651333337363133, Board: Holybro (Unknown Model), Type: PX4

[INFO] Registered new physical device: 1:3162:0053:3B0032001651333337363133 with primary path: /dev/ttyACM1

[INFO] Added additional path to physical device 1:3162:0053:3B0032001651333337363133: /dev/ttyACM2 (primary: /dev/ttyACM1)

[INFO] Secondary device path verified: /dev/ttyACM2 for physical device: 1:3162:0053:3B0032001651333337363133 (primary: /dev/ttyACM1) - skipping duplicate notifications
```

## Testing

### Build and Test

```bash
cd /home/fyou/Desktop/Ultima-Robotics-Stack/ur-mavlink-stack/ur-mavdiscovery-v1.1
./test_usb_tracking.sh
```

### Hardware Testing

1. Connect a USB device that creates multiple ACM paths
2. Run ur-mavdiscovery with logging enabled
3. Verify that only one set of notifications is sent
4. Check that USB bus information is extracted correctly

## Benefits

1. **Eliminates Duplicate Notifications**: Prevents multiple RPC calls for the same physical device
2. **Improved Device Management**: Clear distinction between physical and logical devices
3. **Better USB Bus Tracking**: Full visibility into USB topology
4. **Robust Primary Path Management**: Automatic failover to secondary paths
5. **Backward Compatibility**: Existing functionality preserved
6. **Enhanced Cron Job Data**: Complete device information with USB tracking in periodic notifications
7. **Smart Device Filtering**: Cron job only sends primary devices, preventing duplicate entries in device lists

## Cron Job Device List Improvements

The DeviceDiscoveryCronJob now sends comprehensive device data including:

### Complete Device Information
- **Basic Info**: devicePath, baudrate, sysid, compid, mavlinkVersion, timestamp
- **USB Hardware**: manufacturer, serialNumber, vendorId, productId, deviceName
- **USB Bus Tracking**: usbBusNumber, usbDeviceAddress, physicalDeviceId
- **Flight Controller**: boardClass, boardName, autopilotType
- **Device State**: Current verification state

### Duplicate Prevention
- Only includes primary device paths in the cron job notifications
- Filters out secondary paths to prevent duplicate device entries
- Uses USBDeviceTracker to identify primary vs secondary paths
- Provides logging for debugging device inclusion/exclusion

### Real-time Data
- Uses actual UTC timestamps instead of hardcoded values
- Provides accurate device count reflecting unique physical devices
- Enhanced logging shows which devices are included/excluded with physical device IDs

### Example Notification Format
```json
{
  "eventType": "DEVICE_LIST_UPDATE",
  "source": "ur-mavdiscovery",
  "timestamp": "2025-11-22T11:36:00Z",
  "payload": [
    {
      "devicePath": "/dev/ttyACM1",
      "baudrate": 57600,
      "sysid": 1,
      "compid": 1,
      "mavlinkVersion": 2,
      "deviceName": "Pixhawk6C",
      "manufacturer": "Holybro",
      "serialNumber": "3B0032001651333337363133",
      "vendorId": "3162",
      "productId": "0053",
      "usbBusNumber": "1",
      "usbDeviceAddress": "2",
      "physicalDeviceId": "1:3162:0053:3B0032001651333337363133",
      "boardClass": "Pixhawk",
      "boardName": "Holybro (Unknown Model)",
      "autopilotType": "PX4",
      "state": "VERIFIED",
      "timestamp": "2025-11-22T11:33:50"
    }
  ],
  "deviceCount": 1,
  "targetTopic": "ur-shared-bus/ur-mavlink-stack/notifications"
}
```

## Future Enhancements

1. **USB Device Hot-swapping**: Better handling of device reconnection on different buses
2. **Device Path Persistence**: Remember primary path preferences across restarts
3. **USB Hub Detection**: Track devices through USB hubs
4. **Enhanced Logging**: More detailed USB topology information
