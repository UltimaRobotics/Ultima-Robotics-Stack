# ur-mavrouter Startup Mechanism Implementation

## Overview

This document describes the complete implementation of the heartbeat-triggered startup mechanism for ur-mavrouter. The mechanism automatically detects when ur-mavdiscovery is available, performs device discovery, and starts the mainloop and extensions when MAVLink devices are found.

## Architecture

### Components

1. **Heartbeat Detection** - Monitors `clients/ur-mavdiscovery/heartbeat` topic
2. **Device Discovery** - Requests device list from ur-mavdiscovery using RPC
3. **Service Startup** - Starts mainloop thread and loads extensions
4. **Status Monitoring** - Provides startup status via HTTP API
5. **Error Handling** - Comprehensive error handling and retry mechanisms

### Implementation Files

- **RpcControllerNew.hpp** - Added startup mechanism method declarations
- **RpcControllerNew.cpp** - Implemented core startup logic
- **main.cpp** - Added HTTP API endpoints for startup control
- **ur-rpc-config.json** - Configuration for heartbeat subscription

## Startup Sequence

### Phase 1: Initialization
```cpp
// main.cpp initializes RPC client and subscribes to heartbeat topic
rpcController->initializeRpcIntegration(rpcConfigPath, "ur-mavrouter");
rpcController->startRpcClient();
```

### Phase 2: Heartbeat Detection
```cpp
void RpcController::handleHeartbeatMessage(const std::string& payload) {
    // Parse heartbeat: {"client":"ur-mavdiscovery","status":"alive","service":"device_discovery"}
    // Trigger device discovery if conditions are met
}
```

### Phase 3: Device Discovery
```cpp
void RpcController::triggerDeviceDiscovery() {
    // Create device list request using ur-mavdiscovery-shared structures
    MavlinkShared::MavlinkRpcRequest discoveryRequest("device-list", "ur-mavdiscovery");
    // Send RPC request with unique transaction ID
}
```

### Phase 4: Service Startup
```cpp
void RpcController::handleDeviceDiscoveryResponse(const std::string& payload) {
    // Parse device list, validate devices, trigger mainloop startup
    // Uses existing mavlink_device_added RPC method
}
```

## Key Features

### 1. Heartbeat Monitoring
- **Topic**: `clients/ur-mavdiscovery/heartbeat`
- **Frequency**: Every 5 seconds (configurable)
- **Timeout**: 30 seconds before warning
- **Validation**: Checks client, status, and service fields

### 2. Device Discovery Request
```json
{
  "jsonrpc": "2.0",
  "method": "device-list",
  "params": {
    "include_unverified": false,
    "include_usb_info": true,
    "timeout_seconds": 10
  },
  "id": "device_discovery_1634567890123"
}
```

### 3. Device Validation
- Only starts services with verified/connected devices
- Validates device state before proceeding
- Supports retry mechanism for unverified devices

### 4. Error Handling
- Comprehensive exception handling throughout
- Automatic retry on failures
- Detailed logging with [STARTUP] prefix
- Timeout monitoring for all operations

### 5. HTTP API Endpoints

#### GET /api/startup/status
Returns current startup status:
```json
{
  "discovery_triggered": true,
  "mainloop_started": true,
  "mainloop_running": true,
  "seconds_since_last_heartbeat": 2,
  "heartbeat_timeout_seconds": 30,
  "heartbeat_active": true,
  "rpc_client_running": true,
  "rpc_client_connected": true,
  "extension_count": 3,
  "extensions": [
    {
      "name": "mavlink_logger",
      "loaded": true,
      "running": true
    }
  ],
  "overall_status": "running"
}
```

#### POST /api/startup/trigger
Manually triggers device discovery:
```json
{
  "status": "success",
  "message": "Device discovery triggered manually"
}
```

## State Management

### Startup States
- **waiting** - No heartbeat received yet
- **ready** - Heartbeat active, waiting for discovery
- **discovering** - Device discovery in progress
- **running** - Mainloop and extensions started successfully

### Thread Safety
- Uses `std::mutex` for startup state synchronization
- Atomic variables for boolean flags
- Thread-safe status reporting

## Configuration

### RPC Configuration (ur-rpc-config.json)
```json
{
  "client_id": "ur-mavrouter",
  "broker_host": "127.0.0.1",
  "broker_port": 1899,
  "json_added_subs": {
    "topics": [
      "direct_messaging/ur-mavrouter/requests",
      "clients/ur-mavdiscovery/heartbeat"
    ]
  }
}
```

### Heartbeat Format
```json
{
  "client": "ur-mavdiscovery",
  "status": "alive",
  "service": "device_discovery"
}
```

## Logging

All startup-related logs use `[STARTUP]` prefix:
```
[STARTUP] Received heartbeat from: ur-mavdiscovery, status: alive, service: device_discovery
[STARTUP] ur-mavdiscovery is alive - triggering device discovery
[STARTUP] Device discovery request sent with transaction ID: device_discovery_1634567890123
[STARTUP] Found 1 devices - starting mainloop and extensions
[STARTUP] Startup sequence completed successfully
```

## Error Recovery

### Automatic Retry
- Resets discovery trigger on failures
- Retries when heartbeat resumes after timeout
- Validates device state before proceeding

### Common Error Scenarios
1. **RPC Client Unavailable** - Logs error, resets trigger
2. **Device Discovery Timeout** - Warning logged, allows retry
3. **No Verified Devices** - Waits for device verification
4. **Mainloop Start Failure** - Error logged, services not started

## Integration Points

### With ur-mavdiscovery
- Subscribes to heartbeat topic
- Uses device-list RPC method
- Validates device states

### With Extension Manager
- Loads extension configurations from `config/` directory
- Starts all loaded extensions
- Provides extension status in API

### With Thread Manager
- Starts mainloop thread via RPC operations
- Monitors thread status
- Ensures proper thread lifecycle

## Usage Examples

### Monitor Startup Status
```bash
curl http://localhost:5000/api/startup/status
```

### Manual Device Discovery
```bash
curl -X POST http://localhost:5000/api/startup/trigger
```

### Monitor Logs
```bash
tail -f /var/log/ur-mavrouter.log | grep STARTUP
```

## Testing

### Unit Tests
Test the startup mechanism components:
- Heartbeat message parsing
- Device discovery request generation
- Response handling and validation
- Error handling scenarios

### Integration Tests
Test the complete flow:
1. Start ur-mavrouter
2. Start ur-mavdiscovery
3. Verify heartbeat detection
4. Verify device discovery
5. Verify service startup
6. Test error scenarios

### Manual Testing
```bash
# Terminal 1: Start ur-mavrouter
./ur-mavrouter

# Terminal 2: Monitor startup status
watch -n 1 'curl -s http://localhost:5000/api/startup/status | jq .overall_status'

# Terminal 3: Start ur-mavdiscovery (should trigger startup)
./ur-mavdiscovery
```

## Performance Considerations

### Resource Usage
- Minimal CPU overhead for heartbeat monitoring
- Single thread for startup sequence
- Efficient JSON parsing and validation

### Scalability
- Supports multiple device discoveries
- Handles extension loading efficiently
- Non-blocking HTTP API endpoints

## Future Enhancements

### Potential Improvements
1. **Device Hot-plugging** - Automatic restart on device changes
2. **Configuration Profiles** - Different startup modes
3. **Health Monitoring** - Continuous service health checks
4. **Metrics Collection** - Startup timing and success metrics
5. **Graceful Degradation** - Fallback modes for missing components

### Extension Points
- Custom device validation logic
- Additional startup triggers
- Enhanced error recovery strategies
- Integration with external monitoring systems

## Conclusion

The implemented startup mechanism provides a robust, event-driven approach to initializing ur-mavrouter services. It ensures that resources are only allocated when compatible devices are available, while providing comprehensive monitoring and control capabilities through the HTTP API.

The implementation follows best practices for error handling, logging, and thread safety, making it suitable for production deployment in autonomous robotics systems.
