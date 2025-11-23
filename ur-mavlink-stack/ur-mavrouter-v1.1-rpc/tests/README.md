# RPC Thread State Tests

This directory contains Python tests for collecting thread states, mainloop, and extensions via RPC requests to the ur-mavrouter in the Ultima Robotics Stack.

## Overview

The test suite connects to the MQTT broker used by the UR-RPC system and sends JSON-RPC requests to:
- Get status of all threads
- Get status of specific threads (mainloop, extensions)
- Start/stop threads via RPC operations
- Monitor thread state changes over time

## Files

- `test_rpc_thread_states.py` - Main test suite with comprehensive RPC testing
- `run_tests.py` - Simple test runner script
- `requirements.txt` - Python dependencies
- `README.md` - This documentation

## Requirements

- Python 3.6+
- paho-mqtt library (automatically installed by test runner)

### Quick Start

#### 1. Install Dependencies
```bash
pip install -r requirements.txt
```

#### 2. Run All Tests
```bash
./run_tests.py
```

#### 3. Run Individual Test Modes

#### Basic Tests
```bash
python3 test_rpc_thread_states.py
```

#### Runtime Info Demo (NEW!)
```bash
python3 demo_runtime_info.py
```

#### Runtime Info Tests
```bash
python3 test_runtime_info.py
```

#### Interactive Mode
```bash
python3 test_rpc_thread_states.py interactive
```
Available commands in interactive mode:
- `all` - Get all thread status
- `mainloop` - Get mainloop status
- `<thread_name>` - Get specific thread status
- `start <name>` - Start a thread
- `stop <name>` - Stop a thread
- `monitor` - Monitor thread states for 60 seconds
- `quit` - Exit

#### Extension Monitoring
```bash
python3 test_rpc_thread_states.py extensions
```

#### Continuous Monitoring
```bash
python3 test_rpc_thread_states.py monitor 120  # Monitor for 2 minutes
```

## Test Features

### Thread State Collection
- Retrieves status of all registered threads
- Displays thread information in formatted tables
- Shows thread ID, state, alive status, and attachment ID

### Mainloop Testing
- Checks mainloop thread status
- Can start mainloop if not running
- Monitors mainloop state changes

### Extension Monitoring
- Detects extension threads (UDP, TCP, etc.)
- Tests extension status retrieval
- Can control extension lifecycle

### Real-time Monitoring
- Continuous monitoring mode with configurable intervals
- Timestamp tracking for state changes
- Graceful interrupt handling

## RPC Methods Tested

### thread_status
Get status of threads:
```json
{
  "jsonrpc": "2.0",
  "id": "request-id",
  "method": "thread_status",
  "params": {"thread_name": "all"}  // or specific thread name
}
```

### thread_operation
Perform operations on threads:
```json
{
  "jsonrpc": "2.0", 
  "id": "request-id",
  "method": "thread_operation",
  "params": {
    "thread_name": "mainloop",
    "operation": "start"  // start, stop, pause, resume, restart, status
  }
}
```

### runtime-info (NEW!)
Get comprehensive runtime information including thread types, nature, and system status:
```json
{
  "jsonrpc": "2.0",
  "id": "request-id", 
  "method": "runtime-info",
  "params": {}
}
```

**Response includes:**
- **Thread Details**: Name, ID, state, nature (core/extension/service/monitoring), type, criticality
- **Extension Information**: All loaded extensions with their configuration and status
- **System Status**: RPC initialization, discovery status, uptime, heartbeat info
- **Thread Classification**: Automatic categorization of threads by nature and purpose

**Example Response:**
```json
{
  "status": "success",
  "message": "Runtime information retrieved successfully",
  "timestamp": "Sat Nov 22 05:59:15 2025",
  "uptime_seconds": 41,
  "total_threads": 1,
  "running_threads": 1,
  "threads": [
    {
      "name": "mainloop",
      "thread_id": 1,
      "attachment_id": "mainloop", 
      "state": 1,
      "state_name": "Running",
      "is_alive": true,
      "nature": "core",
      "type": "mainloop",
      "description": "Main event loop for MAVLink message processing",
      "critical": true
    }
  ],
  "extensions": [
    {
      "name": "test_extension_1",
      "type": 2,
      "config_file": "test_extension_1",
      "address": "127.0.0.1",
      "port": 44100,
      "extension_point": "udp-extension-point-1",
      "loaded": true,
      "thread_id": 3,
      "is_running": true,
      "thread_running": false
    }
  ],
  "system": {
    "rpc_initialized": true,
    "discovery_triggered": false,
    "mainloop_started": true,
    "client_id": "ur-mavrouter",
    "config_path": "config/ur-rpc-config.json"
  },
  "device_discovery": {
    "heartbeat_active": false,
    "heartbeat_timeout_seconds": 30,
    "last_heartbeat": 0
  }
}
```

## Configuration

The test connects to the MQTT broker using the default configuration:
- Host: 127.0.0.1
- Port: 1899
- Client ID: test_rpc_client
- Request topic: direct_messaging/ur-mavrouter/requests
- Response topic: direct_messaging/test_rpc_client/responses

To modify these settings, edit the `RpcThreadStateCollector.__init__()` method in the test file.

## Example Output

```
🚀 RPC Thread State Test Suite for Ultima Robotics Stack
============================================================

🧪 Testing Basic Thread Operations
==================================================
✓ Connected to MQTT broker at 127.0.0.1:1899
✓ Subscribed to response topic: direct_messaging/test_rpc_client/responses

1️⃣ Testing get_all_thread_status...
→ Sent thread_status request (ID: 1701234567890-1)
✓ Received response for request 1701234567890-1

================================================================================
THREAD STATES
================================================================================
Thread Name          ID       State    Alive  Attachment ID   Timestamp
--------------------------------------------------------------------------------
mainloop             12345    Running  Yes    mainloop        14:30:25
http_server          12346    Running  Yes    http_server     14:30:25
================================================================================
✓ get_all_thread_status test passed

2️⃣ Testing get_thread_status for mainloop...
→ Sent thread_status request (ID: 1701234567890-2)
✓ Received response for request 1701234567890-2
✓ get_thread_status(mainloop) test passed
```

## Troubleshooting

### Connection Issues
- Ensure ur-mavrouter is running and accessible
- Check MQTT broker configuration in ur-rpc-config.json
- Verify network connectivity to the broker

### Thread Not Found Errors
- Some threads may not be started yet
- Use the interactive mode to start threads manually
- Check the ur-mavrouter logs for thread registration issues

### Timeout Issues
- Increase the response timeout in the test if needed
- Check if the MQTT broker is overloaded
- Verify the ur-mavrouter is processing requests correctly

## Integration with CI/CD

The tests can be integrated into CI/CD pipelines:

```bash
# Run tests with timeout
timeout 300 python3 test_rpc_thread_states.py

# Run with specific test mode
python3 test_rpc_thread_states.py extensions
```

## Contributing

When adding new tests:
1. Follow the existing code style
2. Add proper error handling
3. Include documentation for new features
4. Test with both running and stopped ur-mavrouter instances
