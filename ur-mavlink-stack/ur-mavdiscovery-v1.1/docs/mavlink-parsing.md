
# MAVLink Parsing

## Overview

The MAVLinkParser provides MAVLink v1 and v2 protocol parsing using the official c_library_v2.

## MAVLink Library

### Integration

Uses official MAVLink C library:
- Located in `thirdparty/c_library_v2`
- ArduPilot dialect
- Supports both v1 and v2 protocols

### Include

```cpp
#include <common/mavlink.h>
```

## Parser Architecture

### State Machine

```cpp
mavlink_message_t msg_;
mavlink_status_t status_;
```

The parser maintains:
- Message buffer
- Parsing state
- Protocol version info

### Initialization

```cpp
MAVLinkParser() {
    memset(&msg_, 0, sizeof(msg_));
    memset(&status_, 0, sizeof(status_));
}
```

## Parsing Process

### Byte-by-Byte Parsing

```cpp
ParsedPacket parse(const uint8_t* data, size_t length) {
    ParsedPacket result;
    result.valid = false;

    for (size_t i = 0; i < length; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, data[i], &msg_, &status_)) {
            result.valid = true;
            result.sysid = msg_.sysid;
            result.compid = msg_.compid;
            result.msgid = msg_.msgid;
            result.mavlinkVersion = 
                (status_.flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1) ? 1 : 2;
            result.messageName = getMessageName(msg_.msgid);
            return result;
        }
    }
    return result;
}
```

### Magic Byte Detection

```cpp
bool isMagicByte(uint8_t byte) {
    return (byte == MAVLINK_STX || byte == MAVLINK_STX_MAVLINK1);
}
```

Magic bytes:
- MAVLink v1: 0xFE (254)
- MAVLink v2: 0xFD (253)

## Parsed Packet Structure

```cpp
struct ParsedPacket {
    bool valid;                  // Parsing successful?
    uint8_t sysid;              // System ID
    uint8_t compid;             // Component ID
    uint8_t msgid;              // Message ID
    uint8_t mavlinkVersion;     // 1 or 2
    std::string messageName;    // Message name
};
```

## Protocol Detection

### Automatic Version Detection

The parser automatically detects:
- MAVLink v1 (magic: 0xFE)
- MAVLink v2 (magic: 0xFD)

Based on:
- Start marker byte
- Status flags

### Version Flag

```cpp
result.mavlinkVersion = 
    (status_.flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1) ? 1 : 2;
```

## Message Identification

### Message ID

Extracted from parsed message:

```cpp
result.msgid = msg_.msgid;
```

Common message IDs:
- 0: HEARTBEAT
- 1: SYS_STATUS
- 24: GPS_RAW_INT
- 30: ATTITUDE
- 33: GLOBAL_POSITION_INT

### Message Name

```cpp
std::string getMessageName(uint8_t msgid) {
    return "MSG_" + std::to_string(msgid);
}
```

Note: Full message name lookup requires message definition access.

## Validation

### CRC Checking

MAVLink library performs:
- CRC calculation
- Packet validation
- Checksum verification

Invalid packets:
- Not returned as valid
- Parser continues to next byte

### Sequence Checking

The parser tracks:
- Packet sequence numbers
- Lost packets
- Out-of-order delivery

## Usage Pattern

### In DeviceVerifier

```cpp
MAVLinkParser parser;
std::vector<uint8_t> buffer(280);

int bytesRead = port.read(buffer.data(), buffer.size(), 100);
if (bytesRead > 0) {
    // Check for magic bytes first
    for (int i = 0; i < bytesRead; i++) {
        if (parser.isMagicByte(buffer[i])) {
            foundMagic = true;
        }
    }
    
    // Try to parse complete packet
    ParsedPacket packet = parser.parse(buffer.data(), bytesRead);
    if (packet.valid) {
        // Device verified!
        info.sysid = packet.sysid;
        info.compid = packet.compid;
        info.mavlinkVersion = packet.mavlinkVersion;
        return true;
    }
}
```

## Packet Structure

### MAVLink v1

```
| STX | LEN | SEQ | SYS | COMP | MSG | PAYLOAD | CRC |
|  1  |  1  |  1  |  1  |  1   |  1  | 0-255   |  2  |
```

Max size: 263 bytes

### MAVLink v2

```
| STX | LEN | INC | CMP | SEQ | SYS | COMP | MSG(3) | PAYLOAD | CRC | SIG |
|  1  |  1  |  1  |  1  |  1  |  1  |  1   |   3    | 0-255   |  2  | 13  |
```

Max size: 280 bytes (with signature)

## Error Handling

### Parse Failures

Invalid data:
- Returns invalid ParsedPacket
- Parser continues
- No exceptions thrown

### Buffer Overflow

Protected by:
- Fixed buffer size (280 bytes)
- Library bounds checking
- Safe memory operations

## Performance

### Efficiency

- Single-pass parsing
- No memory allocation
- Minimal CPU overhead
- Suitable for real-time processing

### Communication Channel

Uses MAVLINK_COMM_0:
- Single channel
- No channel multiplexing needed
- Simplified state management
