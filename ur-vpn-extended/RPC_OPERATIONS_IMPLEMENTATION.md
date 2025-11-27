# RPC Operations Implementation - ur-vpn-extended

## Overview

The RPC client in ur-vpn-extended now supports all the same operations as the HTTP server, providing identical functionality through JSON-RPC 2.0 requests on the `direct_messaging/ur-vpn-manager/requests` topic.

## RPC Request Format

All RPC operations follow the JSON-RPC 2.0 standard:

```json
{
  "jsonrpc": "2.0",
  "method": "operation_name",
  "params": {
    "parameter1": "value1",
    "parameter2": "value2"
  },
  "id": "transaction_id"
}
```

## Supported Operations

### 🔧 Basic VPN Instance Management

#### 1. **`parse`**
- **Description**: Parse VPN configuration without making system changes
- **Required Parameters**: `config_content`
- **Response**: Parsed configuration details
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "parse",
  "params": {
    "config_content": "client\nremote 1.2.3.4 1194\n..."
  },
  "id": "req_001"
}
```

#### 2. **`add`**
- **Description**: Add a new VPN instance
- **Required Parameters**: `instance_name`, `config_content`, `vpn_type`
- **Optional Parameters**: `auto_start` (default: true)
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "add",
  "params": {
    "instance_name": "vpn0",
    "config_content": "client\nremote 1.2.3.4 1194\n...",
    "vpn_type": "openvpn",
    "auto_start": true
  },
  "id": "req_002"
}
```

#### 3. **`delete`**
- **Description**: Delete an existing VPN instance
- **Required Parameters**: `instance_name`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "delete",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_003"
}
```

#### 4. **`update`**
- **Description**: Update an existing VPN instance configuration
- **Required Parameters**: `instance_name`, `config_content`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "update",
  "params": {
    "instance_name": "vpn0",
    "config_content": "client\nremote 2.3.4.5 1194\n..."
  },
  "id": "req_004"
}
```

#### 5. **`start`**
- **Description**: Start a VPN instance
- **Required Parameters**: `instance_name`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "start",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_005"
}
```

#### 6. **`stop`**
- **Description**: Stop a VPN instance
- **Required Parameters**: `instance_name`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "stop",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_006"
}
```

#### 7. **`restart`**
- **Description**: Restart a VPN instance
- **Required Parameters**: `instance_name`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "restart",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_007"
}
```

#### 8. **`enable`**
- **Description**: Enable and start a VPN instance
- **Required Parameters**: `instance_name`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "enable",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_008"
}
```

#### 9. **`disable`**
- **Description**: Disable and stop a VPN instance
- **Required Parameters**: `instance_name`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "disable",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_009"
}
```

### 📊 Status and Information

#### 10. **`status`**
- **Description**: Get status of VPN instances
- **Optional Parameters**: `instance_name` (if empty, returns all instances)
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "status",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_010"
}
```

#### 11. **`list`**
- **Description**: List all VPN instances
- **Optional Parameters**: `vpn_type` (filter by type: "openvpn", "wireguard")
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "list",
  "params": {
    "vpn_type": "openvpn"
  },
  "id": "req_011"
}
```

#### 12. **`stats`**
- **Description**: Get aggregated statistics for all VPN instances
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "stats",
  "params": {},
  "id": "req_012"
}
```

### 🛣️ Custom Routing Management

#### 13. **`add-custom-route`**
- **Description**: Add a custom routing rule
- **Required Parameters**: `id`, `vpn_instance`, `destination`
- **Optional Parameters**: `name`, `vpn_profile`, `source_type`, `source_value`, `gateway`, `protocol`, `type`, `priority`, `enabled`, `log_traffic`, `apply_to_existing`, `description`, etc.
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "add-custom-route",
  "params": {
    "id": "route_001",
    "vpn_instance": "vpn0",
    "destination": "192.168.1.0/24",
    "gateway": "VPN Server",
    "priority": 100,
    "enabled": true
  },
  "id": "req_013"
}
```

#### 14. **`update-custom-route`**
- **Description**: Update an existing custom routing rule
- **Required Parameters**: `id`
- **Optional Parameters**: Same fields as add-custom-route
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "update-custom-route",
  "params": {
    "id": "route_001",
    "priority": 200,
    "enabled": false
  },
  "id": "req_014"
}
```

#### 15. **`delete-custom-route`**
- **Description**: Delete a custom routing rule
- **Required Parameters**: `id`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "delete-custom-route",
  "params": {
    "id": "route_001"
  },
  "id": "req_015"
}
```

#### 16. **`list-custom-routes`**
- **Description**: List all custom routing rules
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "list-custom-routes",
  "params": {},
  "id": "req_016"
}
```

#### 17. **`get-custom-route`**
- **Description**: Get a specific custom routing rule
- **Required Parameters**: `id`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "get-custom-route",
  "params": {
    "id": "route_001"
  },
  "id": "req_017"
}
```

### 🔀 Instance-Specific Routing

#### 18. **`get-instance-routes`**
- **Description**: Get routing rules for a specific instance
- **Required Parameters**: `instance_name`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "get-instance-routes",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_018"
}
```

#### 19. **`add-instance-route`**
- **Description**: Add a route rule to a specific instance
- **Required Parameters**: `instance_name`, `route_rule`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "add-instance-route",
  "params": {
    "instance_name": "vpn0",
    "route_rule": {
      "id": "instance_route_001",
      "destination": "10.0.0.0/24",
      "type": "tunnel_all"
    }
  },
  "id": "req_019"
}
```

#### 20. **`delete-instance-route`**
- **Description**: Delete a route rule from a specific instance
- **Required Parameters**: `instance_name`, `rule_id`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "delete-instance-route",
  "params": {
    "instance_name": "vpn0",
    "rule_id": "instance_route_001"
  },
  "id": "req_020"
}
```

#### 21. **`apply-instance-routes`**
- **Description**: Apply routing rules for a specific instance
- **Required Parameters**: `instance_name`
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "apply-instance-routes",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_021"
}
```

#### 22. **`detect-instance-routes`**
- **Description**: Detect routes for a specific instance
- **Required Parameters**: `instance_name`
- **Response**: Number of detected routes
- **Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "detect-instance-routes",
  "params": {
    "instance_name": "vpn0"
  },
  "id": "req_022"
}
```

## Response Format

All RPC responses follow the JSON-RPC 2.0 standard:

```json
{
  "jsonrpc": "2.0",
  "result": {
    "success": true,
    "message": "Operation completed successfully",
    "data": "operation-specific data"
  },
  "id": "transaction_id"
}
```

### Error Response Format:
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32602,
    "message": "Invalid params: Missing instance_name",
    "data": "Additional error details"
  },
  "id": "transaction_id"
}
```

## Implementation Details

### Thread Safety
- All operations are processed in separate threads using ThreadManager
- Thread-safe synchronization with promises/futures for thread ID tracking
- Proper cleanup of thread resources

### Error Handling
- Comprehensive parameter validation
- JSON parsing error handling
- VPN manager operation error propagation
- Detailed error messages for debugging

### Communication
- Requests received on: `direct_messaging/ur-vpn-manager/requests`
- Responses sent on: `direct_messaging/ur-vpn-manager/responses`
- Heartbeat messages on: `clients/ur-vpn-manager/heartbeat`

### Configuration
- RPC client ID: `ur-vpn-manager`
- MQTT broker: `127.0.0.1:1899`
- Heartbeat interval: 30 seconds (configurable)

## Usage Example

Send RPC request via MQTT client:
```bash
mosquitto_pub -h 127.0.0.1 -p 1899 -t "direct_messaging/ur-vpn-manager/requests" -m '{
  "jsonrpc": "2.0",
  "method": "list",
  "params": {},
  "id": "test_001"
}'
```

Listen for responses:
```bash
mosquitto_sub -h 127.0.0.1 -p 1899 -t "direct_messaging/ur-vpn-manager/responses" -v
```

## Total Operations: 22

The RPC implementation now provides complete parity with the HTTP server, supporting all VPN management operations through JSON-RPC 2.0 protocol.
