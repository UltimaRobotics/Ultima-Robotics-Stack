
# MAVLink Collector - Project Scope

## Overview

The MAVLink Collector is a C++ application that establishes UDP connections with MAVLink-enabled vehicles (drones, rovers, etc.) to collect telemetry data and log it to JSON format. It implements the MAVLink protocol to request, receive, and decode flight data.

## Connection Establishment

### 1. Network Architecture

The collector acts as a **Ground Control Station (GCS)** in the MAVLink network:

```
┌─────────────────────┐         UDP          ┌──────────────────────┐
│  MAVLink Vehicle    │ <──────────────────> │  MAVLink Collector   │
│  (Autopilot)        │   Port 14550         │  (GCS)               │
│  - ArduPilot/PX4    │                      │  - System ID: 255    │
│  - System ID: 1     │                      │  - Component ID: 0   │
└─────────────────────┘                      └──────────────────────┘
```

### 2. Socket Initialization Process

The connection is established through these steps:

#### Step 1: UDP Socket Creation
```cpp
udp_socket_ = socket(PF_INET, SOCK_DGRAM, 0);
```
- Creates a UDP datagram socket
- Protocol family: IPv4 (PF_INET)
- Type: SOCK_DGRAM (connectionless)

#### Step 2: Socket Binding
```cpp
struct sockaddr_in local_addr;
local_addr.sin_family = AF_INET;
local_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
local_addr.sin_port = htons(config_.udp_port);  // Default: 14550

bind(udp_socket_, (struct sockaddr*)&local_addr, sizeof(local_addr));
```
- Binds to `0.0.0.0` (all network interfaces)
- Listens on configured port (default: 14550)
- Allows receiving from any source

#### Step 3: Socket Configuration
```cpp
struct timeval tv;
tv.tv_sec = 0;
tv.tv_usec = 100000;  // 100ms timeout
setsockopt(udp_socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
```
- Sets 100ms receive timeout to prevent blocking
- Allows periodic request sending while waiting for data

#### Step 4: Target Address Setup
```cpp
server_addr_.sin_family = AF_INET;
server_addr_.sin_port = htons(config_.udp_port);
inet_pton(AF_INET, config_.udp_address.c_str(), &server_addr_.sin_addr);
```
- Configures destination address for requests
- Typically `127.0.0.1:14550` for SITL (simulation)
- Or actual vehicle IP for hardware connections

### 3. Connection Discovery

The collector uses **MAVLink heartbeat exchange** for discovery:

```
Collector                          Vehicle
   |                                  |
   |------ HEARTBEAT (GCS) --------->|  
   |                                  |
   |<----- HEARTBEAT (Vehicle) ------|
   |                                  |
   |  (Connection Established)        |
```

- Collector sends GCS heartbeat every request interval
- Vehicle responds with its own heartbeat
- First valid heartbeat establishes the connection
- Source address from first message updates `server_addr_`

## Data Collection Mechanisms

### 1. Request Strategy

The collector uses **three complementary strategies** to request data:

#### Strategy 1: GCS Presence Announcement
```cpp
void requestHeartbeat() {
    mavlink_msg_heartbeat_pack(
        255,                        // GCS system ID
        0,                          // GCS component ID
        &msg,
        MAV_TYPE_GCS,              // Type: Ground Control Station
        MAV_AUTOPILOT_INVALID,     // No autopilot (we're GCS)
        MAV_MODE_FLAG_SAFETY_ARMED,
        0,
        MAV_STATE_ACTIVE
    );
    sendMavlinkMessage(msg);
}
```
- Announces presence as GCS
- Triggers vehicle to start sending telemetry
- Sent every `request_interval_ms` (default: 1000ms)

#### Strategy 2: Legacy Data Stream Request (ArduPilot)
```cpp
void requestDataStream() {
    mavlink_msg_request_data_stream_pack(
        255, 0, &msg,
        0, 0,                      // Broadcast to all
        MAV_DATA_STREAM_ALL,       // Request all data streams
        1,                         // Rate: 1 Hz
        1                          // Start streaming
    );
    sendMavlinkMessage(msg);
}
```
- Legacy method for ArduPilot compatibility
- Requests continuous data streams
- `MAV_DATA_STREAM_ALL` includes all telemetry

#### Strategy 3: MAVLink v2 Message Interval (Modern)
```cpp
void requestSpecificMessages() {
    // Request AUTOPILOT_VERSION once
    mavlink_msg_command_long_pack(
        255, 0, &msg, 0, 0,
        MAV_CMD_REQUEST_MESSAGE,
        0,
        MAVLINK_MSG_ID_AUTOPILOT_VERSION,  // Message ID 148
        0, 0, 0, 0, 0, 0
    );
    sendMavlinkMessage(msg);
    
    // Set SYS_STATUS interval to 10 Hz
    mavlink_msg_command_long_pack(
        255, 0, &msg, 0, 0,
        MAV_CMD_SET_MESSAGE_INTERVAL,
        0,
        MAVLINK_MSG_ID_SYS_STATUS,    // Message ID 1
        100000,                       // 100ms = 10 Hz
        0, 0, 0, 0, 0
    );
    sendMavlinkMessage(msg);
    
    // Set BATTERY_STATUS interval to 1 Hz
    mavlink_msg_command_long_pack(
        255, 0, &msg, 0, 0,
        MAV_CMD_SET_MESSAGE_INTERVAL,
        0,
        MAVLINK_MSG_ID_BATTERY_STATUS,  // Message ID 147
        1000000,                        // 1 second
        0, 0, 0, 0, 0
    );
    sendMavlinkMessage(msg);
}
```
- Modern MAVLink v2 approach
- Precise control over message rates
- Interval specified in microseconds

### 2. Message Reception & Parsing

#### Reception Loop
```cpp
void processMessages() {
    uint8_t buffer[2048];
    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    
    // Receive UDP datagram
    ssize_t received = recvfrom(udp_socket_, buffer, sizeof(buffer), 0,
                                (struct sockaddr*)&src_addr, &src_len);
    
    if (received > 0) {
        // Parse MAVLink messages byte-by-byte
        mavlink_message_t msg;
        mavlink_status_t status;
        
        for (ssize_t i = 0; i < received; i++) {
            if (mavlink_parse_char(MAVLINK_COMM_0, buffer[i], &msg, &status) == 1) {
                // Valid message decoded
                handleMessage(msg);
            }
        }
    }
}
```

**Key Points:**
- Non-blocking receive with 100ms timeout
- Handles multiple MAVLink messages per UDP packet
- Byte-by-byte parsing handles fragmentation
- Updates activity timestamp on any message

#### Message Routing
```cpp
switch (msg.msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT:           // ID: 0
        handleHeartbeat(msg);
        break;
    case MAVLINK_MSG_ID_AUTOPILOT_VERSION:   // ID: 148
        handleAutopilotVersion(msg);
        break;
    case MAVLINK_MSG_ID_SYS_STATUS:          // ID: 1
        handleSysStatus(msg);
        break;
    case MAVLINK_MSG_ID_BATTERY_STATUS:      // ID: 147
        handleBatteryStatus(msg);
        break;
}
```

### 3. Data Extraction & Decoding

Each message type has a specific handler:

#### HEARTBEAT (ID: 0)
```cpp
void handleHeartbeat(const mavlink_message_t& msg) {
    mavlink_heartbeat_t heartbeat;
    mavlink_msg_heartbeat_decode(&msg, &heartbeat);
    
    vehicle_data_.system_id = msg.sysid;           // From header
    vehicle_data_.component_id = msg.compid;       // From header
    vehicle_data_.armed = (heartbeat.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
    vehicle_data_.flight_mode = getFlightModeName(heartbeat.base_mode, 
                                                   heartbeat.custom_mode);
    vehicle_data_.last_heartbeat = std::chrono::system_clock::now();
}
```
- **System/Component ID**: Extracted from MAVLink message header
- **Armed Status**: Decoded from `base_mode` bitmap
- **Flight Mode**: Decoded from `custom_mode` (firmware-specific)

#### SYS_STATUS (ID: 1)
```cpp
void handleSysStatus(const mavlink_message_t& msg) {
    mavlink_sys_status_t sys_status;
    mavlink_msg_sys_status_decode(&msg, &sys_status);
    
    if (sys_status.voltage_battery != UINT16_MAX) {
        vehicle_data_.battery_voltage = sys_status.voltage_battery / 1000.0f;
    }
}
```
- **Battery Voltage**: Converted from millivolts to volts
- **Validation**: Checks for UINT16_MAX (unavailable indicator)

#### AUTOPILOT_VERSION (ID: 148)
```cpp
void handleAutopilotVersion(const mavlink_message_t& msg) {
    mavlink_autopilot_version_t version;
    mavlink_msg_autopilot_version_decode(&msg, &version);
    
    uint8_t major = (version.flight_sw_version >> 24) & 0xFF;
    uint8_t minor = (version.flight_sw_version >> 16) & 0xFF;
    uint8_t patch = (version.flight_sw_version >> 8) & 0xFF;
    
    vehicle_data_.firmware = firmware_type + " " + 
                            std::to_string(major) + "." +
                            std::to_string(minor) + "." +
                            std::to_string(patch);
}
```
- **Version Decoding**: Extracts semantic version from bitfield
- **Format**: `<type> <major>.<minor>.<patch>` (e.g., "PX4 1.13.2")

### 4. Data Logging Pipeline

#### Periodic Logging
```cpp
void collectionLoop() {
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        
        // Send requests every request_interval_ms
        if (time_since_last_request >= config_.request_interval_ms) {
            requestData();
        }
        
        // Process incoming messages
        processMessages();
        
        // Log data every 1 second
        if (time_since_last_log >= 1000) {
            logData();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
```

#### JSON Generation
```cpp
void logData() {
    // Only include available data (no placeholders)
    if (vehicle_data_.system_id != 0) {
        log_json_["system_id"] = vehicle_data_.system_id;
    }
    if (vehicle_data_.battery_voltage >= 0.0f) {
        log_json_["battery_voltage"] = vehicle_data_.battery_voltage;
    }
    if (!vehicle_data_.firmware.empty()) {
        log_json_["firmware"] = vehicle_data_.firmware;
    }
    
    // Always include timestamp
    log_json_["last_update"] = getCurrentTimestamp();
    
    // Save to file
    saveLogToFile();
}
```

**Output Format:**
```json
{
  "system_id": 1,
  "component_id": 1,
  "armed": false,
  "flight_mode": "STABILIZE",
  "battery_voltage": 12.4,
  "firmware": "PX4 1.13.2",
  "heartbeat": "0.8s ago",
  "last_activity": "0.1s ago",
  "messages_per_sec": 99.0,
  "last_update": "2025-10-09 14:45:20"
}
```

## Threading Model

```
Main Thread
    │
    ├── Initialize Configuration
    ├── Setup UDP Socket
    └── Start Collection Thread
            │
            └── Collection Loop (runs continuously)
                    ├── Send Requests (every 1s)
                    ├── Process Messages (every 10ms)
                    └── Log Data (every 1s)
```

- **Single Collection Thread**: Handles all I/O operations
- **Non-blocking I/O**: 100ms socket timeout prevents hanging
- **Periodic Tasks**: Request, process, log cycle

## Error Handling & Robustness

### Connection Loss Detection
- **Heartbeat Timeout**: Tracks time since last heartbeat
- **Activity Tracking**: Monitors any message reception
- **Timeout Threshold**: Configurable (default: 5000ms)

### Data Validation
- **NULL Checks**: Validates `UINT16_MAX`, empty strings
- **Type Safety**: Uses MAVLink decode functions
- **Conditional Logging**: Only logs available data

### Recovery Mechanisms
- **Continuous Requests**: Keeps requesting even without responses
- **Source Address Update**: Adapts to vehicle IP changes
- **Graceful Degradation**: Partial data logged if available

## Configuration

Key parameters in `collector_config.json`:
- `udp_address`: Target vehicle IP (e.g., "127.0.0.1")
- `udp_port`: MAVLink port (default: 14550)
- `request_sysid`: Collector system ID (default: 255 for GCS)
- `request_interval_ms`: Request frequency (default: 1000ms)
- `heartbeat_timeout_ms`: Connection timeout (default: 5000ms)
- `log_file`: Output JSON file path

## Use Cases

1. **SITL Simulation**: Connect to local simulator (`127.0.0.1:14550`)
2. **Hardware Vehicle**: Connect to physical autopilot via network
3. **Telemetry Logging**: Continuous flight data recording
4. **Integration**: JSON output for external monitoring tools

## Dependencies

- **MAVLink C Library v2**: Message encoding/decoding
- **nlohmann/json**: JSON serialization
- **POSIX Sockets**: UDP networking
- **C++11**: Threading, chrono, memory management
