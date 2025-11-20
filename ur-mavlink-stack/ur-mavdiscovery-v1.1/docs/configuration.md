
# Configuration System

## Overview

The ur-mavdiscovery project uses JSON-based configuration with sensible defaults, allowing runtime customization without recompilation.

## Configuration Structure

### DeviceConfig Struct

```cpp
struct DeviceConfig {
    std::vector<std::string> devicePathFilters;
    std::vector<int> baudrates;
    int readTimeoutMs;
    int packetTimeoutMs;
    int maxPacketSize;
    bool enableHTTP;
    std::string httpEndpoint;
    int httpTimeoutMs;
    std::string logFile;
    std::string logLevel;
};
```

## JSON Format

### Example Configuration

```json
{
  "devicePathFilters": ["/dev/ttyUSB", "/dev/ttyACM", "/dev/ttyS"],
  "baudrates": [57600, 115200, 921600, 500000, 1500000, 9600, 19200, 38400],
  "readTimeoutMs": 100,
  "packetTimeoutMs": 1000,
  "maxPacketSize": 280,
  "enableHTTP": false,
  "httpEndpoint": "http://localhost:8080/api/devices",
  "httpTimeoutMs": 5000,
  "logFile": "mavdiscovery.log",
  "logLevel": "INFO"
}
```

## Configuration Parameters

### Device Path Filters

**Type**: Array of strings  
**Purpose**: Device path prefixes to monitor  
**Default**: `["/dev/ttyUSB", "/dev/ttyACM", "/dev/ttyS"]`

Common device types:
- `/dev/ttyUSB*`: USB-to-serial adapters
- `/dev/ttyACM*`: USB CDC ACM devices
- `/dev/ttyS*`: Native serial ports

### Baudrates

**Type**: Array of integers  
**Purpose**: Baudrates to test, in order  
**Default**: `[57600, 115200, 921600, 500000, 1500000, 9600, 19200, 38400]`

Order matters:
- Common rates first for faster detection
- Less common rates later

Supported rates:
- 9600, 19200, 38400, 57600, 115200
- 230400, 460800, 500000, 576000
- 921600, 1000000, 1152000, 1500000, 2000000

### Read Timeout

**Type**: Integer (milliseconds)  
**Purpose**: Serial port read timeout  
**Default**: 100  
**Range**: 10-1000ms recommended

Affects:
- Responsiveness of serial reads
- CPU usage during verification

### Packet Timeout

**Type**: Integer (milliseconds)  
**Purpose**: Maximum wait per baudrate  
**Default**: 1000  
**Range**: 500-5000ms recommended

Determines:
- How long to wait for valid packets
- Total verification time

### Max Packet Size

**Type**: Integer (bytes)  
**Purpose**: Read buffer size  
**Default**: 280  
**Range**: 280-512 bytes

MAVLink packet sizes:
- v1 max: 263 bytes
- v2 max: 280 bytes

### HTTP Integration

**enableHTTP**: Boolean, enable remote notifications  
**httpEndpoint**: String, URL for POST requests  
**httpTimeoutMs**: Integer, HTTP request timeout

### Logging

**logFile**: String, log file path  
**logLevel**: String, minimum log level (DEBUG, INFO, WARNING, ERROR)

## Loading Configuration

### From File

```cpp
ConfigLoader config;
if (!config.loadFromFile("config.json")) {
    // Falls back to defaults
}
```

### Default Values

If file not found or parse error:
- Uses built-in defaults
- Logs warning
- Continues operation

## Access Pattern

### Get Complete Config

```cpp
DeviceConfig config = loader.getConfig();
```

### Use in Components

```cpp
DeviceMonitor monitor(config.device, threadManager);
DeviceVerifier verifier(devicePath, config.device, threadManager);
```

## Validation

The system validates:
- JSON syntax
- Field types
- Value ranges (where applicable)

Invalid configurations:
- Log error
- Fall back to defaults
- Continue operation

## Runtime Behavior

Configuration is loaded once at startup:
- Changes require restart
- No hot reload support
- Thread-safe read access
