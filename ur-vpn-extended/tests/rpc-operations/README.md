# RPC Operations Test Suite

Comprehensive Python test suite for ur-vpn-manager RPC operations, testing all 22 supported operations via MQTT JSON-RPC 2.0 requests.

## 📋 Overview

This test suite validates the functionality of all RPC operations supported by ur-vpn-manager:

### 🔧 Basic VPN Instance Management
- **parse** - Parse VPN configuration without making system changes
- **add** - Add a new VPN instance
- **delete** - Delete an existing VPN instance
- **update** - Update an existing VPN instance configuration
- **start** - Start a VPN instance
- **stop** - Stop a VPN instance
- **restart** - Restart a VPN instance
- **enable** - Enable and start a VPN instance
- **disable** - Disable and stop a VPN instance

### 📊 Status and Information
- **status** - Get status of VPN instances
- **list** - List all VPN instances
- **stats** - Get aggregated statistics for all VPN instances

### 🛣️ Custom Routing Management
- **add-custom-route** - Add a custom routing rule
- **update-custom-route** - Update an existing custom routing rule
- **delete-custom-route** - Delete a custom routing rule
- **list-custom-routes** - List all custom routing rules
- **get-custom-route** - Get a specific custom routing rule

### 🔀 Instance-Specific Routing
- **get-instance-routes** - Get routing rules for a specific instance
- **add-instance-route** - Add a route rule to a specific instance
- **delete-instance-route** - Delete a route rule from a specific instance
- **apply-instance-routes** - Apply routing rules for a specific instance
- **detect-instance-routes** - Detect routes for a specific instance

## 🚀 Quick Start

### Prerequisites

1. **Build ur-vpn-manager**:
   ```bash
   cd /home/fyousfi/Pictures/Ultima-Robotics-Stack/ur-vpn-extended/build
   make -j4
   ```

2. **Install Python dependencies**:
   ```bash
   cd /home/fyousfi/Pictures/Ultima-Robotics-Stack/ur-vpn-extended/tests/rpc-operations
   pip install -r requirements.txt
   ```

3. **Start ur-vpn-manager**:
   ```bash
   cd /home/fyousfi/Pictures/Ultima-Robotics-Stack/ur-vpn-extended
   ./build/ur-vpn-manager -pkg_config config/master-config.json -rpc_config config/ur-rpc-config.json
   ```

4. **MQTT Broker**:
   - Tests connect to existing MQTT broker at `127.0.0.1:1899`
   - Broker is started by ur-vpn-manager when it runs

### Running Tests

#### Run Individual Test Files
```bash
# Basic operations
python3 test_list_operation.py
python3 test_add_operation.py my_instance /path/to/config.ovpn
python3 test_status_operation.py my_instance

# All operations support command-line arguments
python3 test_start_operation.py my_instance
python3 test_stop_operation.py my_instance
python3 test_restart_operation.py my_instance
python3 test_delete_operation.py my_instance
python3 test_update_operation.py my_instance /path/to/new_config.ovpn
python3 test_enable_operation.py my_instance
python3 test_disable_operation.py my_instance
python3 test_stats_operation.py
python3 test_parse_operation.py /path/to/config.ovpn
```

## 📁 Test Structure

```
tests/rpc-operations/
├── base_rpc_test.py              # Base test class with MQTT client only
├── requirements.txt              # Python dependencies
├── README.md                     # This file
├── test_list_operation.py        # List operation test
├── test_add_operation.py         # Add operation test
├── test_delete_operation.py      # Delete operation test
├── test_start_operation.py       # Start operation test
├── test_stop_operation.py        # Stop operation test
├── test_restart_operation.py     # Restart operation test
├── test_update_operation.py      # Update operation test
├── test_status_operation.py      # Status operation test
├── test_stats_operation.py       # Stats operation test
├── test_parse_operation.py       # Parse operation test
├── test_enable_operation.py      # Enable operation test
└── test_disable_operation.py     # Disable operation test
```

## 🧪 Test Architecture

### BaseRPCTest Class

The `BaseRPCTest` class provides:

- **MQTT Client Management**: Connection to existing MQTT broker
- **Request/Response Handling**: JSON-RPC 2.0 request sending and response validation
- **Assertion Helpers**: Common test assertion methods
- **Cleanup**: Automatic MQTT disconnection after each test

### Test Pattern

Each test follows this simple pattern:

1. **Setup**: Connect to MQTT broker only
2. **Execute**: Send RPC request with specific parameters
3. **Validate**: Check response format and content
4. **Cleanup**: Disconnect from MQTT

### Key Changes

- ✅ **No Process Management**: Tests no longer start/stop ur-vpn-manager
- ✅ **Lightweight**: Only handles MQTT communication
- ✅ **Independent**: Can run concurrently without interference
- ✅ **CLI Interface**: All tests accept command-line arguments
- ✅ **JSON Focus**: Pure JSON-RPC request/response testing

### Example Test

```python
def test_add_operation():
    """Test adding a VPN instance"""
    test = BaseRPCTest()
    
    try:
        if not test.setup():
            return False
            
        # Read config from file
        config_content = read_config_file("/path/to/config.ovpn")
        
        response = test.send_rpc_request("add", {
            "instance_name": "test_instance",
            "config_content": config_content,
            "vpn_type": "openvpn",
            "auto_start": False
        })
        
        # Validate response
        if 'result' in response and response['result'].get('success'):
            print("✓ Add operation completed successfully")
            return True
        else:
            print("✗ Add operation failed")
            return False
            
    finally:
        test.teardown()
```

### Command Line Usage

```bash
# Usage: python test_add_operation.py <instance_name> <config_file_path> [vpn_type] [auto_start]
python test_add_operation.py test_vpn /path/to/config.ovpn openvpn false
```

## 📊 Test Coverage

### Positive Tests
- ✅ Valid operation parameters
- ✅ Successful operation execution
- ✅ Proper response format
- ✅ Data consistency

### Negative Tests
- ✅ Missing required parameters
- ✅ Invalid parameter values
- ✅ Non-existent resources
- ✅ Duplicate operations

### Edge Cases
- ✅ Large configurations
- ✅ Empty parameters
- ✅ Concurrent operations
- ✅ Resource cleanup

## 🔧 Configuration

### MQTT Settings
- **Host**: `127.0.0.1`
- **Port**: `1899`
- **Request Topic**: `direct_messaging/ur-vpn-manager/requests`
- **Response Topic**: `direct_messaging/ur-vpn-manager/responses`
- **Heartbeat Topic**: `clients/ur-vpn-manager/heartbeat`

### Test Parameters
- **Response Timeout**: 10 seconds
- **Heartbeat Timeout**: 35 seconds
- **Test Instance Prefix**: `test_`
- **Cleanup**: Automatic after each test

## 📝 Output Format

### Success Output
```
✓ Using MQTT client with API version 2.0
🔧 Setting up test environment...
✓ Connected to MQTT broker at 127.0.0.1:1899
✓ Subscribed to response topic: direct_messaging/ur-vpn-manager/responses
Testing: List All VPN Instances
📤 Sending RPC request: {"jsonrpc":"2.0","method":"list","params":{},"id":"test_123"}
📨 Received message on direct_messaging/ur-vpn-manager/responses: {"result":{"success":true,...}}
Response:
{
  "id": "test_123",
  "jsonrpc": "2.0",
  "result": {
    "success": true,
    "instances": [...]
  }
}
✓ List operation completed successfully
🧹 Cleaning up test environment...
```

### Error Output
```
✗ Test failed: Timeout waiting for RPC response
```

### Usage Error Output
```
Usage: python test_add_operation.py <instance_name> <config_file_path> [vpn_type] [auto_start]
```

## 🐛 Troubleshooting

### Common Issues

1. **MQTT Connection Failed**
   ```
   ✗ Failed to connect to MQTT broker: Connection refused
   ```
   **Solution**: Ensure ur-vpn-manager is running and MQTT broker is accessible

2. **Timeout Waiting for Response**
   ```
   ✗ Test failed: Timeout waiting for RPC response
   ```
   **Solution**: 
   - Verify ur-vpn-manager is running with RPC enabled
   - Check network connectivity to MQTT broker
   - Ensure correct MQTT topics are configured

3. **Permission Denied**
   ```
   ❌ Permission denied
   ```
   **Solution**: Make test files executable with `chmod +x *.py`

4. **Missing Dependencies**
   ```
   ❌ paho-mqtt not installed
   ```
   **Solution**: Install with `pip install -r requirements.txt`

5. **File Not Found**
   ```
   Error: File '/path/to/config.ovpn' not found
   ```
   **Solution**: Provide correct file path for configuration files

### Manual Testing

Test individual operations manually:
```bash
# Start ur-vpn-manager
./build/ur-vpn-manager -pkg_config config/master-config.json -rpc_config config/ur-rpc-config.json

# Send test request
mosquitto_pub -h 127.0.0.1 -p 1899 -t "direct_messaging/ur-vpn-manager/requests" -m '{
  "jsonrpc": "2.0",
  "method": "list",
  "params": {},
  "id": "manual_test"
}'

# Listen for responses
mosquitto_sub -h 127.0.0.1 -p 1899 -t "direct_messaging/ur-vpn-manager/responses" -v
```

## 🤝 Contributing

### Adding New Tests

1. Create new test file: `test_new_operation.py`
2. Inherit from `BaseRPCTest`
3. Implement test methods following the naming pattern `test_*`
4. Add `main()` function that runs all tests in the file
5. Update this README

### Test Naming Convention

- **Files**: `test_<operation>.py`
- **Methods**: `test_<scenario>()`
- **Instances**: `test_<operation>_<description>`

### Best Practices

- Use descriptive test names
- Test both positive and negative cases
- Clean up resources after each test
- Include assertions for response format
- Add helpful error messages

## 📄 License

This test suite is part of the ur-vpn-extended project and follows the same license terms.
