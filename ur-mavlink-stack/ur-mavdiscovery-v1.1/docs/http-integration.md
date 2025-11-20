
# HTTP Integration

## Overview

The HTTPClient provides optional remote notification capabilities, allowing verified devices to be reported to external systems.

## Configuration

### Enable HTTP

```json
{
  "enableHTTP": true,
  "httpEndpoint": "http://localhost:8080/api/devices",
  "httpTimeoutMs": 5000
}
```

Parameters:
- **enableHTTP**: Boolean, enable/disable notifications
- **httpEndpoint**: URL for POST requests
- **httpTimeoutMs**: Request timeout in milliseconds

## HTTP Client

### Initialization

```cpp
HTTPClient client(config_.httpEndpoint, config_.httpTimeoutMs);
```

Uses libcurl for HTTP operations.

### POST Request

```cpp
bool postDeviceInfo(const DeviceInfo& info);
```

Returns:
- true: Success
- false: Failure (logged)

## Request Format

### Method

POST request with JSON body

### Headers

```
Content-Type: application/json
```

### JSON Payload

```json
{
  "devicePath": "/dev/ttyUSB0",
  "state": 2,
  "baudrate": 57600,
  "sysid": 1,
  "compid": 1,
  "mavlinkVersion": 2,
  "timestamp": "2025-01-10 12:34:56",
  "messages": [
    {
      "msgid": 0,
      "name": "MSG_0"
    }
  ]
}
```

### State Values

```cpp
enum class DeviceState {
    UNKNOWN = 0,
    VERIFYING = 1,
    VERIFIED = 2,
    NON_MAVLINK = 3,
    REMOVED = 4
};
```

## Integration Flow

```
DeviceVerifier completes
    ↓
Device state: VERIFIED?
    ↓ Yes
HTTP enabled?
    ↓ Yes
Create HTTPClient
    ↓
Convert DeviceInfo to JSON
    ↓
POST to endpoint
    ↓
Log result
```

## JSON Serialization

### Device Info

```cpp
std::string deviceInfoToJSON(const DeviceInfo& info) {
    json j;
    j["devicePath"] = info.devicePath;
    j["state"] = static_cast<int>(info.state.load());
    j["baudrate"] = info.baudrate;
    j["sysid"] = info.sysid;
    j["compid"] = info.compid;
    j["mavlinkVersion"] = info.mavlinkVersion;
    j["timestamp"] = info.timestamp;
    
    json messages = json::array();
    for (const auto& msg : info.messages) {
        json msgObj;
        msgObj["msgid"] = msg.msgid;
        msgObj["name"] = msg.name;
        messages.push_back(msgObj);
    }
    j["messages"] = messages;
    
    return j.dump();
}
```

## Error Handling

### Connection Failures

```cpp
if (res != CURLE_OK) {
    LOG_ERROR("HTTP POST failed: " + std::string(curl_easy_strerror(res)));
    return false;
}
```

Errors logged:
- Connection refused
- Timeout
- DNS resolution
- SSL/TLS errors

### Non-blocking

HTTP operations don't block device discovery:
- Executed after verification
- Independent of state updates
- Failures logged only

## Use Cases

### Central Monitoring

Send device discoveries to central server:
- Fleet management
- Device inventory
- Telemetry aggregation

### Automation

Trigger actions on device discovery:
- Configuration deployment
- Software updates
- Integration with other systems

### Analytics

Collect discovery metrics:
- Device types
- Connection patterns
- Verification success rates

## Security Considerations

### HTTPS Support

Use HTTPS endpoints for secure transmission:

```json
{
  "httpEndpoint": "https://api.example.com/devices"
}
```

### Authentication

Add authentication headers if needed (requires code modification):

```cpp
headers = curl_slist_append(headers, "Authorization: Bearer token");
```

### Timeouts

Configure appropriate timeouts:
- Prevent indefinite waits
- Balance between reliability and responsiveness

## Testing

### Mock Server

Test with local HTTP server:

```bash
python3 -m http.server 8080
```

### cURL Equivalent

Manual testing:

```bash
curl -X POST http://localhost:8080/api/devices \
  -H "Content-Type: application/json" \
  -d '{"devicePath":"/dev/ttyUSB0","state":2,"baudrate":57600}'
```

## Debugging

Enable verbose logging:
- Check HTTP client logs
- Verify endpoint accessibility
- Monitor network traffic
- Review server logs
