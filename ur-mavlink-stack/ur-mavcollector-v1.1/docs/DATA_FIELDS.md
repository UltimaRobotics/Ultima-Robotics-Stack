
# MAVLink Collector Data Fields Documentation

This document describes each data field collected by the MAVLink collector, including the request process, response handling, and data extraction methods.

## Overview

The MAVLink collector actively requests data from a MAVLink vehicle via UDP and processes incoming messages to extract telemetry information. Each field has a specific request mechanism and response handler.

---

## Data Fields

### 1. `system_id`
**Type:** `integer`  
**Source:** `HEARTBEAT` message (ID: 0)  
**Example:** `1`

#### Request Process:
- **Method:** Passive (no explicit request needed)
- **Trigger:** Sent by requestHeartbeat() as part of GCS presence announcement
- **Frequency:** Every request_interval_ms (default: 1000ms)

#### Response Handling:
```cpp
void handleHeartbeat(const mavlink_message_t& msg) {
    vehicle_data_.system_id = msg.sysid;  // Extract from message header
}
```

#### Data Extraction:
- Extracted directly from the MAVLink message header `msg.sysid`
- Uniquely identifies the vehicle in a MAVLink network
- Value range: 1-255

---

### 2. `component_id`
**Type:** `integer`  
**Source:** `HEARTBEAT` message (ID: 0)  
**Example:** `1`

#### Request Process:
- **Method:** Passive (received with heartbeat)
- **Trigger:** Same as system_id
- **Frequency:** Every heartbeat (typically 1Hz)

#### Response Handling:
```cpp
void handleHeartbeat(const mavlink_message_t& msg) {
    vehicle_data_.component_id = msg.compid;  // Extract from message header
}
```

#### Data Extraction:
- Extracted from MAVLink message header `msg.compid`
- Identifies specific component (1 = autopilot, 200 = camera, etc.)
- Value range: 1-255

---

### 3. `armed`
**Type:** `boolean`  
**Source:** `HEARTBEAT` message (ID: 0)  
**Example:** `false`

#### Request Process:
- **Method:** Passive (received with heartbeat)
- **Message:** HEARTBEAT sent every request_interval_ms

#### Response Handling:
```cpp
void handleHeartbeat(const mavlink_message_t& msg) {
    mavlink_heartbeat_t heartbeat;
    mavlink_msg_heartbeat_decode(&msg, &heartbeat);
    
    // Decode armed state from base_mode bitmap
    vehicle_data_.armed = (heartbeat.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
}
```

#### Data Extraction:
- Extracted from `base_mode` field using bitwise AND with `MAV_MODE_FLAG_SAFETY_ARMED` (128)
- `true` = motors armed, `false` = motors disarmed
- Critical safety information

---

### 4. `flight_mode`
**Type:** `string`  
**Source:** `HEARTBEAT` message (ID: 0)  
**Example:** `"STABILIZE"`, `"UNKNOWN_50593792"`

#### Request Process:
- **Method:** Passive (received with heartbeat)
- **Message:** HEARTBEAT received automatically

#### Response Handling:
```cpp
void handleHeartbeat(const mavlink_message_t& msg) {
    mavlink_heartbeat_t heartbeat;
    mavlink_msg_heartbeat_decode(&msg, &heartbeat);
    
    vehicle_data_.flight_mode = getFlightModeName(heartbeat.base_mode, heartbeat.custom_mode);
}

std::string getFlightModeName(uint8_t base_mode, uint32_t custom_mode) {
    // ArduPilot Copter modes mapping
    const char* copter_modes[] = {
        "STABILIZE", "ACRO", "ALT_HOLD", "AUTO", "GUIDED", "LOITER",
        "RTL", "CIRCLE", "LAND", "DRIFT", "SPORT", "FLIP", "AUTOTUNE",
        "POSHOLD", "BRAKE", "THROW", "AVOID_ADSB", "GUIDED_NOGPS", "SMART_RTL"
    };
    
    if (custom_mode < sizeof(copter_modes) / sizeof(copter_modes[0])) {
        return copter_modes[custom_mode];
    }
    
    return "UNKNOWN_" + std::to_string(custom_mode);
}
```

#### Data Extraction:
- Uses `custom_mode` field from HEARTBEAT
- Maps numeric mode to string name using firmware-specific table
- Falls back to "UNKNOWN_<number>" if mode not in table
- Mode interpretation depends on vehicle type (Copter/Plane/Rover)

---

### 5. `firmware`
**Type:** `string`  
**Source:** `HEARTBEAT` (ID: 0) + `AUTOPILOT_VERSION` (ID: 148)  
**Example:** `"PX4"`, `"ArduPilot 4.3.8"`

#### Request Process:
- **Initial:** From HEARTBEAT autopilot field
- **Detailed:** Requested via MAV_CMD_REQUEST_MESSAGE command

```cpp
void requestSpecificMessages() {
    mavlink_message_t msg;
    
    mavlink_msg_command_long_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        0, 0,  // target system/component (broadcast)
        MAV_CMD_REQUEST_MESSAGE,
        0,  // confirmation
        MAVLINK_MSG_ID_AUTOPILOT_VERSION,  // message to request
        0, 0, 0, 0, 0, 0
    );
    sendMavlinkMessage(msg);
}
```

#### Response Handling:
```cpp
void handleHeartbeat(const mavlink_message_t& msg) {
    mavlink_heartbeat_t heartbeat;
    mavlink_msg_heartbeat_decode(&msg, &heartbeat);
    
    if (vehicle_data_.firmware.empty()) {
        switch (heartbeat.autopilot) {
            case MAV_AUTOPILOT_ARDUPILOTMEGA:
                vehicle_data_.firmware = "ArduPilot";
                break;
            case MAV_AUTOPILOT_PX4:
                vehicle_data_.firmware = "PX4";
                break;
            // ... other autopilot types
        }
    }
}

void handleAutopilotVersion(const mavlink_message_t& msg) {
    mavlink_autopilot_version_t version;
    mavlink_msg_autopilot_version_decode(&msg, &version);
    
    // Decode version: major<<24 | minor<<16 | patch<<8
    uint8_t major = (version.flight_sw_version >> 24) & 0xFF;
    uint8_t minor = (version.flight_sw_version >> 16) & 0xFF;
    uint8_t patch = (version.flight_sw_version >> 8) & 0xFF;
    
    std::stringstream ss;
    if (!vehicle_data_.firmware.empty()) {
        ss << vehicle_data_.firmware << " ";
    }
    ss << (int)major << "." << (int)minor << "." << (int)patch;
    
    vehicle_data_.firmware = ss.str();
}
```

#### Data Extraction:
- **Stage 1:** Extract autopilot type from HEARTBEAT.autopilot field
- **Stage 2:** Request AUTOPILOT_VERSION message for version details
- **Stage 3:** Decode version from flight_sw_version bitfield
- Not included in JSON if unavailable (empty string)

---

### 6. `battery_voltage`
**Type:** `float`  
**Source:** `SYS_STATUS` (ID: 1) or `BATTERY_STATUS` (ID: 147)  
**Example:** `12.4`

#### Request Process:
- **Method:** Request message interval via MAV_CMD_SET_MESSAGE_INTERVAL

```cpp
void requestSpecificMessages() {
    mavlink_message_t msg;
    
    // Request SYS_STATUS at 10Hz
    mavlink_msg_command_long_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        0, 0,
        MAV_CMD_SET_MESSAGE_INTERVAL,
        0,
        MAVLINK_MSG_ID_SYS_STATUS,  // message ID
        100000,  // interval in microseconds (100ms = 10Hz)
        0, 0, 0, 0, 0
    );
    sendMavlinkMessage(msg);
    
    // Request BATTERY_STATUS at 1Hz
    mavlink_msg_command_long_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        0, 0,
        MAV_CMD_SET_MESSAGE_INTERVAL,
        0,
        MAVLINK_MSG_ID_BATTERY_STATUS,
        1000000,  // 1 second interval
        0, 0, 0, 0, 0
    );
    sendMavlinkMessage(msg);
}
```

#### Response Handling:
```cpp
void handleSysStatus(const mavlink_message_t& msg) {
    mavlink_sys_status_t sys_status;
    mavlink_msg_sys_status_decode(&msg, &sys_status);
    
    // voltage_battery is in millivolts, convert to volts
    if (sys_status.voltage_battery != UINT16_MAX) {
        vehicle_data_.battery_voltage = sys_status.voltage_battery / 1000.0f;
    }
}

void handleBatteryStatus(const mavlink_message_t& msg) {
    mavlink_battery_status_t battery;
    mavlink_msg_battery_status_decode(&msg, &battery);
    
    // voltages[0] is total voltage in millivolts
    if (battery.voltages[0] != UINT16_MAX) {
        vehicle_data_.battery_voltage = battery.voltages[0] / 1000.0f;
    }
}
```

#### Data Extraction:
- Primary source: SYS_STATUS.voltage_battery (uint16, millivolts)
- Secondary source: BATTERY_STATUS.voltages[0] (uint16, millivolts)
- Convert from millivolts to volts by dividing by 1000
- UINT16_MAX (65535) indicates value unavailable
- Initialized to -1.0, only included in JSON if >= 0.0

---

### 7. `model`
**Type:** `string`  
**Source:** Currently not extracted  
**Example:** `""` (empty if unavailable)

#### Current Status:
- **Not implemented:** No MAVLink message currently provides vehicle model name
- **Future enhancement:** Could be extracted from:
  - AUTOPILOT_VERSION.flight_custom_version (custom text field)
  - Component metadata messages
  - Custom MAVLink messages

#### Placeholder Removed:
- Previously set to "Unknown", now empty string if unavailable
- Not included in JSON output if empty

---

### 8. `heartbeat`
**Type:** `string` (formatted duration)  
**Source:** Calculated from last HEARTBEAT timestamp  
**Example:** `"2.9s ago"`

#### Calculation:
```cpp
std::string formatDuration(const std::chrono::system_clock::time_point& time_point) {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - time_point);
    
    double seconds = duration.count() / 1000.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << seconds << "s ago";
    return ss.str();
}
```

#### Usage:
- Indicates connection health (should be < 5 seconds for healthy connection)
- Updated from last_heartbeat timestamp
- Not included in JSON if no heartbeat received

---

### 9. `last_activity`
**Type:** `string` (formatted duration)  
**Source:** Calculated from any message reception  
**Example:** `"0.0s ago"`

#### Update Trigger:
```cpp
void processMessages() {
    // ... receive UDP data ...
    
    if (mavlink_parse_char(MAVLINK_COMM_0, buffer[i], &msg, &status) == 1) {
        vehicle_data_.messages_received++;
        vehicle_data_.last_activity = std::chrono::system_clock::now();  // Update on ANY message
        // ... handle specific messages ...
    }
}
```

#### Usage:
- Shows time since ANY MAVLink message received
- More comprehensive than heartbeat (shows all telemetry activity)
- Not included in JSON if no messages received

---

### 10. `messages_per_sec`
**Type:** `float`  
**Source:** Calculated from messages_received counter  
**Example:** `99.0`

#### Calculation:
```cpp
double calculateMessagesPerSecond() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - vehicle_data_.start_time).count();
    return elapsed > 0 ? static_cast<double>(vehicle_data_.messages_received) / elapsed : 0.0;
}
```

#### Usage:
- Average message rate since collector started
- Indicates telemetry stream health
- Typical values: 10-100 messages/sec depending on configuration

---

### 11. `last_update`
**Type:** `string` (timestamp)  
**Source:** System clock  
**Example:** `"2025-10-09 14:45:20"`

#### Generation:
```cpp
auto now = std::chrono::system_clock::now();
auto time_t_now = std::chrono::system_clock::to_time_t(now);

std::stringstream timestamp_ss;
timestamp_ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");

log_json_["last_update"] = timestamp_ss.str();
```

#### Usage:
- Shows when the JSON log file was last updated
- Updated every 1 second (configurable)
- Always included in JSON output

---

## Request Strategy

### Active Requests
The collector uses three strategies to request data:

1. **Heartbeat Exchange** (Legacy compatibility):
   - Sends GCS heartbeat to announce presence
   - Vehicle responds with its own heartbeat

2. **Data Stream Request** (ArduPilot legacy):
   ```cpp
   mavlink_msg_request_data_stream_pack(
       system_id, component_id, &msg,
       0, 0,  // target (broadcast)
       MAV_DATA_STREAM_ALL,
       1,  // rate (Hz)
       1   // start
   );
   ```

3. **Message Interval Commands** (MAVLink v2):
   - Uses MAV_CMD_SET_MESSAGE_INTERVAL to request specific messages
   - Uses MAV_CMD_REQUEST_MESSAGE to request one-time messages

### Message Flow

```
Collector (GCS)                    Vehicle (Autopilot)
     |                                    |
     |------ HEARTBEAT ------------------>|
     |                                    |
     |------ REQUEST_DATA_STREAM -------->|
     |       (MAV_DATA_STREAM_ALL)        |
     |                                    |
     |------ COMMAND_LONG --------------->|
     |       (REQUEST AUTOPILOT_VERSION)  |
     |                                    |
     |------ COMMAND_LONG --------------->|
     |       (SET_MESSAGE_INTERVAL)       |
     |       SYS_STATUS @ 10Hz            |
     |                                    |
     |<----- HEARTBEAT -------------------|
     |<----- AUTOPILOT_VERSION -----------|
     |<----- SYS_STATUS ------------------|
     |<----- BATTERY_STATUS --------------|
     |<----- (other telemetry) -----------|
```

## Error Handling

### Unavailable Data
Fields are handled as follows when data is unavailable:

- **Strings** (model, flight_mode, firmware): Empty string, excluded from JSON
- **Numeric** (battery_voltage): -1.0, excluded from JSON if < 0
- **Timestamps** (heartbeat, last_activity): Zero time_point, excluded from JSON
- **Counters** (system_id, component_id): 0, excluded from JSON if 0
- **Boolean** (armed): false (included in JSON)

### Connection Loss
- `heartbeat` field shows increasing time
- `messages_per_sec` decreases over time
- `last_activity` shows time since last message

## Configuration

Key configuration parameters in `collector_config.json`:

```json
{
  "request_interval_ms": 1000,     // How often to send requests
  "heartbeat_timeout_ms": 5000,    // When to consider connection lost
  "log_file": "mavlink_data.json"  // Output file path
}
```

## Example Complete Output

```json
{
  "system_id": 1,
  "component_id": 1,
  "armed": false,
  "flight_mode": "STABILIZE",
  "firmware": "ArduPilot 4.3.8",
  "battery_voltage": 12.4,
  "heartbeat": "0.8s ago",
  "last_activity": "0.1s ago",
  "messages_per_sec": 99.0,
  "last_update": "2025-10-09 14:45:20"
}
```

Note: Fields like `model` are omitted when unavailable (no placeholder data).
