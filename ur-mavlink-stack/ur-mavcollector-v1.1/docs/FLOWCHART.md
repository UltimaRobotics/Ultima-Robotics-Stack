
# ur-mavcollector Detailed Flowchart

## Overview
This document provides a comprehensive flowchart description of the MAVLink data collection mechanisms implemented in ur-mavcollector, including request packet generation and response handling for each data type.

---

## Main Application Flow

```mermaid
graph TD
    A[Application Start] --> B[Load Configuration]
    B --> C[Initialize MAVLinkCollector]
    C --> D[Setup UDP Socket]
    D --> E[Load/Create JSON Log]
    E --> F[Start Collection Thread]
    F --> G[Enter Collection Loop]
    G --> H{Running?}
    H -->|Yes| I[Check Request Timer]
    H -->|No| Z[Stop & Save Log]
    I --> J{Time to Request?}
    J -->|Yes| K[Request Data]
    J -->|No| L[Process Messages]
    K --> L
    L --> M[Check Log Timer]
    M --> N{Time to Log?}
    N -->|Yes| O[Log Data to File]
    N -->|No| P[Sleep 10ms]
    O --> P
    P --> H
    Z --> ZZ[Exit]
```

---

## UDP Socket Setup Flow

```mermaid
graph TD
    A[setupUdpSocket] --> B[Create UDP Socket]
    B --> C{Socket Created?}
    C -->|No| D[Return False]
    C -->|Yes| E[Bind to 0.0.0.0:udp_port]
    E --> F{Bind Success?}
    F -->|No| G[Close Socket]
    G --> D
    F -->|Yes| H[Set Socket Timeout 100ms]
    H --> I{Timeout Set?}
    I -->|No| G
    I -->|Yes| J[Setup Server Address]
    J --> K[Parse Target IP]
    K --> L{IP Valid?}
    L -->|No| G
    L -->|Yes| M[Return True]
```

---

## Request Data Flow (Top Level)

```mermaid
graph TD
    A[requestData] --> B[Request Heartbeat]
    B --> C[Request Data Stream]
    C --> D[Request Specific Messages]
    D --> E[Return]
```

---

## 1. GCS Heartbeat Request Generation

```mermaid
graph TD
    A[requestHeartbeat] --> B[Create mavlink_message_t]
    B --> C[Pack Heartbeat Message]
    C --> D[Source: config.request_sysid default 255]
    D --> E[Source Component: config.request_compid default 0]
    E --> F[Type: MAV_TYPE_GCS]
    F --> G[Autopilot: MAV_AUTOPILOT_INVALID]
    G --> H[Base Mode: 0]
    H --> I[Custom Mode: 0]
    I --> J[System Status: MAV_STATE_ACTIVE]
    J --> K[Call sendMavlinkMessage]
    K --> L{Send Success?}
    L -->|Yes| M[Return True]
    L -->|No| N[Return False]
    
    style F fill:#90EE90
    style G fill:#90EE90
```

**Packet Structure:**
- **Message ID:** 0 (HEARTBEAT)
- **Payload:** 9 bytes
  - `type` (uint8): 6 = MAV_TYPE_GCS
  - `autopilot` (uint8): 8 = MAV_AUTOPILOT_INVALID
  - `base_mode` (uint8): 0
  - `custom_mode` (uint32): 0
  - `system_status` (uint8): 4 = MAV_STATE_ACTIVE
  - `mavlink_version` (uint8): 3

---

## 2. Legacy Data Stream Request Generation

```mermaid
graph TD
    A[requestDataStream] --> B{vehicle_data.system_id == 0?}
    B -->|Yes| C[Create REQUEST_DATA_STREAM]
    B -->|No| Z[Skip - Use SET_MESSAGE_INTERVAL]
    C --> D[Pack Message]
    D --> E[Target System: 0 broadcast]
    E --> F[Target Component: 0 broadcast]
    F --> G[Stream ID: MAV_DATA_STREAM_ALL]
    G --> H[Rate: 1 Hz]
    H --> I[Start/Stop: 1 start]
    I --> J[Call sendMavlinkMessage]
    J --> K{Send Success?}
    K -->|Yes| L[Return]
    K -->|No| L
    
    style C fill:#FFB6C1
    style G fill:#FFB6C1
```

**Packet Structure:**
- **Message ID:** 66 (REQUEST_DATA_STREAM)
- **Payload:** 6 bytes
  - `target_system` (uint8): 0 (broadcast)
  - `target_component` (uint8): 0 (broadcast)
  - `req_stream_id` (uint8): 0 = MAV_DATA_STREAM_ALL
  - `req_message_rate` (uint16): 1 Hz
  - `start_stop` (uint8): 1 = start

**Note:** Only used initially before vehicle system_id is known. Modern approach uses SET_MESSAGE_INTERVAL.

---

## 3. Specific Message Requests Generation

```mermaid
graph TD
    A[requestSpecificMessages] --> B{vehicle_data.system_id known?}
    B -->|No| C[Set Target: 0, 0 broadcast]
    B -->|Yes| D[Set Target: vehicle sys/comp]
    C --> E[Request AUTOPILOT_VERSION]
    D --> E
    E --> F[Pack COMMAND_LONG]
    F --> G[Command: MAV_CMD_REQUEST_MESSAGE]
    G --> H[Param1: MAVLINK_MSG_ID_AUTOPILOT_VERSION 148]
    H --> I[Other Params: 0.0f]
    I --> J[Call sendMavlinkMessage]
    J --> K{vehicle_data.system_id known?}
    K -->|No| Z[Return]
    K -->|Yes| L[Request SYS_STATUS Interval]
    L --> M[Pack COMMAND_LONG]
    M --> N[Command: MAV_CMD_SET_MESSAGE_INTERVAL]
    N --> O[Param1: MAVLINK_MSG_ID_SYS_STATUS 1]
    O --> P[Param2: 100000.0f μs 10Hz]
    P --> Q[Call sendMavlinkMessage]
    Q --> R[Request BATTERY_STATUS Interval]
    R --> S[Pack COMMAND_LONG]
    S --> T[Command: MAV_CMD_SET_MESSAGE_INTERVAL]
    T --> U[Param1: MAVLINK_MSG_ID_BATTERY_STATUS 147]
    U --> V[Param2: 1000000.0f μs 1Hz]
    V --> W[Call sendMavlinkMessage]
    W --> Z[Return]
    
    style N fill:#87CEEB
    style T fill:#87CEEB
```

### 3a. AUTOPILOT_VERSION Request Packet

**Packet Structure:**
- **Message ID:** 76 (COMMAND_LONG)
- **Payload:** 33 bytes
  - `target_system` (uint8): 0 or vehicle_data.system_id
  - `target_component` (uint8): 0 or vehicle_data.component_id
  - `command` (uint16): 512 = MAV_CMD_REQUEST_MESSAGE
  - `confirmation` (uint8): 0
  - `param1` (float): 148.0 = MAVLINK_MSG_ID_AUTOPILOT_VERSION
  - `param2-7` (float): 0.0

### 3b. SYS_STATUS Interval Request Packet

**Packet Structure:**
- **Message ID:** 76 (COMMAND_LONG)
- **Payload:** 33 bytes
  - `target_system` (uint8): vehicle_data.system_id
  - `target_component` (uint8): vehicle_data.component_id
  - `command` (uint16): 511 = MAV_CMD_SET_MESSAGE_INTERVAL
  - `confirmation` (uint8): 0
  - `param1` (float): 1.0 = MAVLINK_MSG_ID_SYS_STATUS
  - `param2` (float): 100000.0 (100ms = 10Hz)
  - `param3-7` (float): 0.0

### 3c. BATTERY_STATUS Interval Request Packet

**Packet Structure:**
- **Message ID:** 76 (COMMAND_LONG)
- **Payload:** 33 bytes
  - `target_system` (uint8): vehicle_data.system_id
  - `target_component` (uint8): vehicle_data.component_id
  - `command` (uint16): 511 = MAV_CMD_SET_MESSAGE_INTERVAL
  - `confirmation` (uint8): 0
  - `param1` (float): 147.0 = MAVLINK_MSG_ID_BATTERY_STATUS
  - `param2` (float): 1000000.0 (1s = 1Hz)
  - `param3-7` (float): 0.0

---

## Message Sending Flow

```mermaid
graph TD
    A[sendMavlinkMessage] --> B[Create Buffer 263 bytes]
    B --> C[mavlink_msg_to_send_buffer]
    C --> D[Get Message Length]
    D --> E[sendto UDP Socket]
    E --> F[Destination: server_addr_]
    F --> G{Bytes Sent == Length?}
    G -->|Yes| H[Return True]
    G -->|No| I[Log Error]
    I --> J[Return False]
```

---

## Message Reception & Processing Flow

```mermaid
graph TD
    A[processMessages] --> B[Create Buffer 2048 bytes]
    B --> C[recvfrom UDP Socket]
    C --> D{Data Received?}
    D -->|Timeout/Error| E[Return]
    D -->|Yes| F[Parse Each Byte]
    F --> G[mavlink_parse_char]
    G --> H{Valid Message?}
    H -->|No| I[Continue Parsing]
    H -->|Yes| J[Increment messages_received]
    J --> K[Update last_activity timestamp]
    K --> L{First Message?}
    L -->|Yes| M[Update server_addr_ with source]
    L -->|No| N[Check Message ID]
    M --> N
    N --> O{Message Type?}
    O -->|HEARTBEAT 0| P[handleHeartbeat]
    O -->|AUTOPILOT_VERSION 148| Q[handleAutopilotVersion]
    O -->|SYS_STATUS 1| R[handleSysStatus]
    O -->|BATTERY_STATUS 147| S[handleBatteryStatus]
    O -->|Other| T[Ignore]
    P --> I
    Q --> I
    R --> I
    S --> I
    T --> I
    I --> U{More Bytes?}
    U -->|Yes| F
    U -->|No| E
```

---

## Response Handling: HEARTBEAT (ID: 0)

```mermaid
graph TD
    A[handleHeartbeat] --> B[Decode HEARTBEAT]
    B --> C[Extract msg.sysid]
    C --> D[vehicle_data.system_id = msg.sysid]
    D --> E[Extract msg.compid]
    E --> F[vehicle_data.component_id = msg.compid]
    F --> G[Update last_heartbeat timestamp]
    G --> H[Check base_mode bit 7]
    H --> I{MAV_MODE_FLAG_SAFETY_ARMED?}
    I -->|Yes| J[vehicle_data.armed = true]
    I -->|No| K[vehicle_data.armed = false]
    J --> L{Check autopilot type}
    K --> L
    L --> M{autopilot == MAV_AUTOPILOT_ARDUPILOTMEGA?}
    M -->|Yes| N[Decode ArduPilot Mode]
    M -->|No| O{autopilot == MAV_AUTOPILOT_PX4?}
    O -->|Yes| P[Decode PX4 Mode]
    O -->|No| Q[Set UNKNOWN_ + custom_mode]
    N --> R[Map custom_mode to string]
    P --> S[Extract main_mode & sub_mode]
    S --> T[Map to PX4 mode string]
    R --> U{First Heartbeat?}
    T --> U
    Q --> U
    U -->|Yes| V[Set firmware from autopilot]
    U -->|No| W[Log Heartbeat Info]
    V --> W
    W --> X[Return]
    
    style I fill:#FFD700
    style M fill:#FFD700
    style O fill:#FFD700
```

**Response Data Extraction:**
- **system_id:** From message header `msg.sysid`
- **component_id:** From message header `msg.compid`
- **armed:** Bit 7 of `heartbeat.base_mode` (128 = 0x80)
- **flight_mode:** 
  - ArduPilot: `heartbeat.custom_mode` mapped to mode array
  - PX4: Upper byte (main_mode) and lower byte (sub_mode)
- **firmware:** Initial identification from `heartbeat.autopilot` enum

**Validation:**
- No placeholder data used
- `armed` always set (boolean, defaults to false)
- `flight_mode` set only if valid mapping exists, otherwise "UNKNOWN_" + number
- `firmware` set only if autopilot type recognized

---

## Response Handling: AUTOPILOT_VERSION (ID: 148)

```mermaid
graph TD
    A[handleAutopilotVersion] --> B[Decode AUTOPILOT_VERSION]
    B --> C[Extract flight_sw_version]
    C --> D[Major = version >> 24 & 0xFF]
    D --> E[Minor = version >> 16 & 0xFF]
    E --> F[Patch = version >> 8 & 0xFF]
    F --> G{firmware already set?}
    G -->|Yes| H[Use existing firmware name]
    G -->|No| I[firmware = empty]
    H --> J[Build version string]
    I --> J
    J --> K[firmware = name + major.minor.patch]
    K --> L{flight_custom_version0 != 0?}
    L -->|Yes| M[Extract 8-byte custom string]
    L -->|No| N[Skip model]
    M --> O[Null-terminate string]
    O --> P[vehicle_data.model = custom_str]
    P --> Q[Log Version Info]
    N --> Q
    Q --> R[Return]
    
    style D fill:#98FB98
    style E fill:#98FB98
    style F fill:#98FB98
```

**Response Data Extraction:**
- **flight_sw_version:** 32-bit bitfield
  - Bits 24-31: Major version
  - Bits 16-23: Minor version
  - Bits 8-15: Patch version
  - Bits 0-7: Version type (ignored)
- **flight_custom_version:** 8-byte array for vehicle model name
- **firmware:** Combines existing firmware type + version number

**Validation:**
- Version extracted only if fields are non-zero
- `flight_custom_version[0] != 0` checked before extracting model
- Null-termination ensured for string safety

---

## Response Handling: SYS_STATUS (ID: 1)

```mermaid
graph TD
    A[handleSysStatus] --> B[Decode SYS_STATUS]
    B --> C[Extract voltage_battery]
    C --> D{voltage_battery == UINT16_MAX?}
    D -->|Yes| E[Skip - Unavailable]
    D -->|No| F{voltage_battery > 0?}
    F -->|No| E
    F -->|Yes| G[Convert mV to V]
    G --> H[battery_voltage = voltage / 1000.0f]
    H --> I[Log Battery Voltage]
    I --> J[Return]
    E --> J
    
    style D fill:#FFA07A
    style F fill:#FFA07A
```

**Response Data Extraction:**
- **voltage_battery:** uint16_t in millivolts
  - Valid range: 1 - 65534
  - 65535 (UINT16_MAX) = unavailable
  - 0 = unavailable/error

**Conversion:**
- mV → V: `voltage_battery / 1000.0f`

**Validation:**
- Check for UINT16_MAX (65535)
- Check for zero value
- Only update if both checks pass
- No placeholder/fallback value used

---

## Response Handling: BATTERY_STATUS (ID: 147)

```mermaid
graph TD
    A[handleBatteryStatus] --> B[Decode BATTERY_STATUS]
    B --> C[Extract voltages0]
    C --> D{voltages0 == UINT16_MAX?}
    D -->|Yes| E[Skip - Unavailable]
    D -->|No| F{voltages0 > 0?}
    F -->|No| E
    F -->|Yes| G[Convert mV to V]
    G --> H[battery_voltage = voltages0 / 1000.0f]
    H --> I{battery_remaining != -1?}
    I -->|Yes| J[Log Voltage & Remaining %]
    I -->|No| K[Log Voltage Only]
    J --> L[Return]
    K --> L
    E --> L
    
    style D fill:#FFA07A
    style F fill:#FFA07A
    style I fill:#87CEFA
```

**Response Data Extraction:**
- **voltages[0]:** uint16_t total voltage in millivolts
  - Valid range: 1 - 65534
  - 65535 (UINT16_MAX) = unavailable
  - 0 = unavailable/error
- **battery_remaining:** int8_t percentage (0-100)
  - -1 = unavailable
  - Used for logging only, not stored

**Conversion:**
- mV → V: `voltages[0] / 1000.0f`

**Validation:**
- Check for UINT16_MAX (65535)
- Check for zero value
- Only update if both checks pass
- `battery_remaining` logged if != -1 but not stored

**Priority:**
- BATTERY_STATUS has higher precision than SYS_STATUS
- Both can update `battery_voltage`, last received wins

---

## Data Logging Flow

```mermaid
graph TD
    A[logData] --> B[Get Current Time]
    B --> C[Format Timestamp]
    C --> D[Create/Update JSON]
    D --> E{model available?}
    E -->|Yes| F[log_jsonmodel = model]
    E -->|No| G[Skip model]
    F --> H{system_id != 0?}
    G --> H
    H -->|Yes| I[log_jsonsystem_id = id]
    H -->|No| J[Skip system_id]
    I --> K{component_id != 0?}
    J --> K
    K -->|Yes| L[log_jsoncomponent_id = id]
    K -->|No| M[Skip component_id]
    L --> N{flight_mode available?}
    M --> N
    N -->|Yes| O[log_jsonflight_mode = mode]
    N -->|No| P[Skip flight_mode]
    O --> Q[log_jsonarmed = armed]
    P --> Q
    Q --> R{battery_voltage >= 0?}
    R -->|Yes| S[log_jsonbattery_voltage = voltage]
    R -->|No| T[Skip battery_voltage]
    S --> U{last_heartbeat set?}
    T --> U
    U -->|Yes| V[log_jsonheartbeat = duration]
    U -->|No| W[Skip heartbeat]
    V --> X{firmware available?}
    W --> X
    X -->|Yes| Y[log_jsonfirmware = firmware]
    X -->|No| Z[Skip firmware]
    Y --> AA{last_activity set?}
    Z --> AA
    AA -->|Yes| AB[log_jsonlast_activity = duration]
    AA -->|No| AC[Skip last_activity]
    AB --> AD[log_jsonmessages_per_sec = rate]
    AC --> AD
    AD --> AE[log_jsonlast_update = timestamp]
    AE --> AF[saveLogToFile]
    AF --> AG[Log to Console]
    AG --> AH[Return]
    
    style Q fill:#90EE90
    style E fill:#FFE4B5
    style H fill:#FFE4B5
    style R fill:#FFE4B5
```

**Conditional Logging Rules:**
1. **Always included:**
   - `armed` (boolean, always valid)
   - `messages_per_sec` (calculated value)
   - `last_update` (current timestamp)

2. **Conditionally included:**
   - `model`: Only if not empty
   - `system_id`: Only if != 0
   - `component_id`: Only if != 0
   - `flight_mode`: Only if not empty
   - `battery_voltage`: Only if >= 0.0
   - `heartbeat`: Only if timestamp is set
   - `firmware`: Only if not empty
   - `last_activity`: Only if timestamp is set

---

## Timing & Intervals

```mermaid
graph TD
    A[Collection Loop Timing] --> B[Request Interval]
    A --> C[Log Interval]
    A --> D[Socket Timeout]
    A --> E[Loop Sleep]
    
    B --> B1[config.request_interval_ms]
    B1 --> B2[Default: 1000ms]
    B2 --> B3[Sends all requests every 1s]
    
    C --> C1[Hard-coded: 1000ms]
    C1 --> C2[Saves JSON every 1s]
    
    D --> D1[Hard-coded: 100ms]
    D1 --> D2[Non-blocking receive]
    
    E --> E1[Hard-coded: 10ms]
    E1 --> E2[Prevents CPU spinning]
    
    style B2 fill:#FFD700
    style C1 fill:#FFD700
    style D1 fill:#FFD700
    style E1 fill:#FFD700
```

---

## Error Handling & Validation

```mermaid
graph TD
    A[Error Handling Points] --> B[Socket Operations]
    A --> C[Message Parsing]
    A --> D[Data Validation]
    
    B --> B1[Socket creation failure]
    B --> B2[Bind failure]
    B --> B3[Send failure]
    B --> B4[Receive timeout]
    B1 --> B5[Log error, return false]
    B2 --> B5
    B3 --> B5
    B4 --> B6[Silent return - normal]
    
    C --> C1[Invalid MAVLink frame]
    C --> C2[Unknown message ID]
    C1 --> C3[Discard byte, continue]
    C2 --> C4[Ignore message, continue]
    
    D --> D1[UINT16_MAX check]
    D --> D2[Zero value check]
    D --> D3[Empty string check]
    D --> D4[Null timestamp check]
    D1 --> D5[Skip field update]
    D2 --> D5
    D3 --> D5
    D4 --> D5
    
    style D1 fill:#FF6B6B
    style D2 fill:#FF6B6B
    style D3 fill:#FF6B6B
    style D4 fill:#FF6B6B
```

---

## State Transitions

```mermaid
stateDiagram-v2
    [*] --> Uninitialized
    Uninitialized --> SocketSetup: start()
    SocketSetup --> Ready: Success
    SocketSetup --> Failed: Error
    Ready --> Requesting: Enter Loop
    Requesting --> WaitingResponse: Requests Sent
    WaitingResponse --> ProcessingHeartbeat: HEARTBEAT Received
    WaitingResponse --> ProcessingVersion: AUTOPILOT_VERSION Received
    WaitingResponse --> ProcessingSysStatus: SYS_STATUS Received
    WaitingResponse --> ProcessingBattery: BATTERY_STATUS Received
    ProcessingHeartbeat --> VehicleKnown: system_id Extracted
    ProcessingVersion --> VersionKnown: firmware & model Extracted
    ProcessingSysStatus --> BatteryUpdated: voltage Extracted
    ProcessingBattery --> BatteryUpdated: voltage Extracted
    VehicleKnown --> Logging: Timer Triggered
    VersionKnown --> Logging
    BatteryUpdated --> Logging
    Logging --> Requesting: Loop Continue
    Requesting --> Stopped: stop() Called
    Stopped --> [*]
```

---

## Summary

### Request Generation Summary
1. **GCS Heartbeat (ID 0):** Sent every request interval to identify as GCS
2. **Legacy Stream Request (ID 66):** Only sent when vehicle unknown, broadcasts request for all data
3. **AUTOPILOT_VERSION Request (ID 76):** Always sent, uses broadcast or targeted based on vehicle knowledge
4. **SYS_STATUS Interval (ID 76):** Only sent after vehicle identified, requests 10Hz updates
5. **BATTERY_STATUS Interval (ID 76):** Only sent after vehicle identified, requests 1Hz updates

### Response Handling Summary
1. **HEARTBEAT:** Extracts system/component ID, armed state, flight mode, initial firmware
2. **AUTOPILOT_VERSION:** Extracts detailed firmware version and vehicle model
3. **SYS_STATUS:** Extracts battery voltage (lower precision)
4. **BATTERY_STATUS:** Extracts battery voltage (higher precision) and remaining percentage

### Data Quality Guarantees
- **No Placeholder Data:** All fields checked before inclusion in log
- **Validation:** UINT16_MAX, zero values, empty strings filtered out
- **Conditional Logging:** Only available data written to JSON
- **Timestamp Tracking:** All events timestamped for age calculation
