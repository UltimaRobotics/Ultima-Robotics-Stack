# MAVLink Device Discovery - Usage Guide

## Overview

The `ur-mavdiscovery` service discovers and verifies MAVLink devices, providing real-time RPC notifications to other services in the UR ecosystem.

## Command Line Usage

### Required Arguments

- `-rpc_config, --rpc-config FILE` - Path to RPC configuration JSON file
- `-package_config, --package-config FILE` - Path to package configuration JSON file

### Optional Arguments

- `-h, --help` - Display help message and exit

### Examples

```bash
# Using short options
./mavdiscovery -rpc_config rpc-config.json -package_config config.json

# Using long options  
./mavdiscovery --rpc-config /etc/mavdiscovery/rpc.json --package-config /etc/mavdiscovery/config.json

# Get help
./mavdiscovery -h
./mavdiscovery --help
```

## Configuration Files

### RPC Configuration (`rpc-config.json`)

Contains MQTT broker settings, topics, and client configuration for RPC communication:

```json
{
  "client_id": "ur-mavdiscovery",
  "broker_host": "127.0.0.1",
  "broker_port": 1899,
  "keepalive": 60,
  "qos": 1,
  "auto_reconnect": true,
  "reconnect_delay_min": 1,
  "reconnect_delay_max": 60,
  "use_tls": false,
  "connect_timeout": 10,
  "message_timeout": 30,
  "heartbeat": {
    "enabled": true,
    "interval_seconds": 5,
    "topic": "clients/ur-mavdiscovery/heartbeat",
    "payload": "{\"client\":\"ur-mavdiscovery\",\"status\":\"alive\",\"service\":\"device_discovery\"}"
  },
  "json_added_pubs": {
    "topics": [
      "direct_messaging/ur-mavdiscovery/responses",
      "direct_messaging/ur-mavrouter/requests",
      "direct_messaging/ur-mavcollector/requests"
    ]
  },
  "json_added_subs": {
    "topics": [
      "direct_messaging/ur-mavdiscovery/requests"
    ]
  }
}
```

### Package Configuration (`config.json`)

Contains device discovery settings, baudrates, and logging configuration:

```json
{
  "devicePathFilters": [
    "/dev/ttyUSB",
    "/dev/ttyACM"
  ],
  "baudrates": [
    57600,
    115200,
    921600,
    500000,
    1500000
  ],
  "readTimeoutMs": 100,
  "packetTimeoutMs": 1000,
  "maxPacketSize": 280,
  "enableHTTP": true,
  "httpConfig": {
    "serverAddress": "0.0.0.0",
    "serverPort": 5000,
    "timeoutMs": 5000
  },
  "logFile": "mavdiscovery.log",
  "logLevel": "INFO",
  "runtimeDeviceFile": "current-runtime-device.json"
}
```

## RPC Interface

### Device Notifications

When devices are added/verified, the service sends `mavlink_added` notifications to:
- `direct_messaging/ur-mavrouter/requests`
- `direct_messaging/ur-mavcollector/requests`

When devices are removed, the service sends `device_removed` notifications to the same topics.

### Request Handling

The service listens for requests on `direct_messaging/ur-mavdiscovery/requests` and supports:

- `device-list` - Returns list of currently verified devices
- `device-info` - Returns information about a specific device
- `device_verify` - Triggers verification of a device
- `system-info` - Returns system information

All responses are sent to `direct_messaging/ur-mavdiscovery/responses`.

## Configuration Validation

The service validates both configuration files before starting:

- **File existence** - Files must exist and be readable
- **JSON format** - Files must contain valid JSON
- **Required fields** - Essential configuration fields must be present
- **Field types** - Fields must have correct data types

If validation fails, the service will exit with an error message describing the issue.

## Error Handling

The service provides comprehensive error handling:

- Missing or invalid arguments
- Configuration file errors
- Network connectivity issues
- Device access problems
- RPC communication failures

All errors are logged and appropriate error messages are displayed on stderr.

## Logging

Logs are written to both:
- Console output (with timestamps and log levels)
- Log file specified in package configuration (default: `mavdiscovery.log`)

Log levels: DEBUG, INFO, WARNING, ERROR
