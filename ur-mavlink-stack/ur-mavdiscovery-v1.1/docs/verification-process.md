
# Verification Process

## Overview

The DeviceVerifier performs sequential baudrate testing to identify MAVLink-capable devices and extract their configuration details.

## Verification Workflow

```
Device Detected
    ↓
Create DeviceVerifier
    ↓
Start Verification Thread
    ↓
For each baudrate:
    ├── Open Serial Port
    ├── Configure Port Settings
    ├── Read Data (with timeout)
    ├── Parse for MAVLink Magic Bytes
    ├── Parse Complete Packets
    └── Success? → Store Info & Exit
    
    ↓
Update State: VERIFIED or NON_MAVLINK
    ↓
Trigger Callbacks
    ↓
HTTP Notification (if enabled)
```

## Baudrate Testing

### Sequential Approach

Baudrates are tested in order from configuration:

```json
{
  "baudrates": [57600, 115200, 921600, 500000, 1500000, 9600, 19200, 38400]
}
```

Benefits:
- Predictable testing order
- Common rates tested first
- Configurable priority

### Per-Baudrate Process

For each baudrate:

1. **Port Opening**: Open serial device at specified baudrate
2. **Magic Byte Detection**: Look for MAVLink start markers (0xFE or 0xFD)
3. **Packet Timeout**: Continue reading until timeout (default: 1000ms)
4. **Success Criteria**: One valid MAVLink packet = verified

## MAVLink Detection

### Magic Bytes

- MAVLink v1: 0xFE (254)
- MAVLink v2: 0xFD (253)

The parser identifies these markers before attempting full packet parsing.

### Packet Parsing

Uses the MAVLinkParser to:
- Validate packet structure
- Extract system ID (sysid)
- Extract component ID (compid)
- Determine MAVLink version
- Identify message types

## Information Extraction

Upon successful verification, the following is captured:

```cpp
struct DeviceInfo {
    std::string devicePath;        // e.g., /dev/ttyUSB0
    int baudrate;                  // Working baudrate
    uint8_t sysid;                 // MAVLink system ID
    uint8_t compid;                // MAVLink component ID
    uint8_t mavlinkVersion;        // 1 or 2
    std::vector<MAVLinkMessage> messages;  // Received messages
    std::string timestamp;         // Discovery time
    std::atomic<DeviceState> state;  // Current state
};
```

## Thread Management

### Thread Creation

Each verifier runs in its own thread:
- Created via ThreadManager
- Registered with device path as identifier
- Independent execution

### Thread Cleanup

On stop():
1. Set shouldStop flag
2. Join thread with 5-second timeout
3. Forceful stop if timeout exceeded
4. Unregister from ThreadManager

## State Transitions

```
UNKNOWN → VERIFYING → VERIFIED
                   → NON_MAVLINK
                   
Device removed: → REMOVED
```

All state changes are atomic and thread-safe.

## Timeout Configuration

- **readTimeoutMs**: Serial port read timeout (default: 100ms)
- **packetTimeoutMs**: Maximum wait per baudrate (default: 1000ms)
- **maxPacketSize**: Buffer size for reads (default: 280 bytes)

## Error Conditions

- **Port open failure**: Skip to next baudrate
- **Timeout on all baudrates**: Mark as NON_MAVLINK
- **Thread stop requested**: Abort immediately
