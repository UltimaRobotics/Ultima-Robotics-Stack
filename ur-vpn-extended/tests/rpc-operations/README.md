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
   cd /home/fyou/Desktop/Ultima-Robotics-Stack/ur-vpn-extended/build
   make -j4
   ```

2. **Install Python dependencies**:
   ```bash
   cd /home/fyou/Desktop/Ultima-Robotics-Stack/ur-vpn-extended/tests/rpc-operations
   pip install -r requirements.txt
   ```

3. **MQTT Broker** (optional):
   - Tests will automatically start ur-vpn-manager which includes MQTT broker functionality
   - Default broker: `127.0.0.1:1899`

### Running Tests

#### Run All Tests
```bash
python3 run_all_tests.py
```

#### Run Individual Test Files
```bash
# Basic operations
python3 test_parse.py
python3 test_add.py
python3 test_delete.py
python3 test_start_stop_restart.py
python3 test_enable_disable.py

# Status and information
python3 test_status_list_stats.py

# Routing operations
python3 test_custom_routes.py
python3 test_instance_routes.py
```

## 📁 Test Structure

```
tests/rpc-operations/
├── base_rpc_test.py              # Base test class with MQTT client
├── run_all_tests.py              # Test suite runner
├── requirements.txt              # Python dependencies
├── README.md                     # This file
├── test_parse.py                 # Parse operation tests
├── test_add.py                   # Add operation tests
├── test_delete.py                # Delete operation tests
├── test_start_stop_restart.py    # Lifecycle operation tests
├── test_enable_disable.py        # Enable/disable operation tests
├── test_status_list_stats.py     # Status/list/stats operation tests
├── test_custom_routes.py         # Custom routing operation tests
└── test_instance_routes.py       # Instance routing operation tests
```

## 🧪 Test Architecture

### BaseRPCTest Class

The `BaseRPCTest` class provides:

- **MQTT Client Management**: Automatic connection to MQTT broker
- **Process Management**: Starts/stops ur-vpn-manager binary
- **Request/Response Handling**: JSON-RPC 2.0 request sending and response validation
- **Assertion Helpers**: Common test assertion methods
- **Cleanup**: Automatic resource cleanup after each test

### Test Pattern

Each test follows this pattern:

1. **Setup**: Start ur-vpn-manager and connect to MQTT
2. **Execute**: Send RPC request with specific parameters
3. **Validate**: Check response format and content
4. **Cleanup**: Stop processes and disconnect

### Example Test

```python
def test_add_openvpn_instance(self):
    """Test adding an OpenVPN instance"""
    config_content = """
client
dev tun
proto udp
remote 10.0.0.1 1194
    """.strip()
    
    response = self.send_rpc_request("add", {
        "instance_name": "test_ovpn0",
        "config_content": config_content,
        "vpn_type": "openvpn",
        "auto_start": False
    })
    
    self.assert_success(response)
    self.assert_contains_fields(response, "success", "message")
    
    result = response['result']
    assert result['success'] == True
    assert "added successfully" in result['message']
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
🧪 Running test: test_add_openvpn_instance
🔧 Setting up test environment...
✓ Connected to MQTT broker at 127.0.0.1:1899
✓ Subscribed to response topic: direct_messaging/ur-vpn-manager/responses
✓ ur-vpn-manager is already running
📤 Sending RPC request: {"jsonrpc":"2.0","method":"add","params":{...},"id":"test_123"}
📨 Received message on direct_messaging/ur-vpn-manager/responses: {"result":{"success":true,...}}
✓ OpenVPN instance added: VPN instance added and started successfully
🧹 Cleaning up test environment...
✅ test_add_openvpn_instance passed
```

### Error Output
```
❌ test_add_missing_instance_name failed: Missing 'instance_name' for add operation
```

### Summary Output
```
📊 Add Tests Results: 5/6 passed
📊 TEST SUITE SUMMARY
==================================================
Total Tests: 8
Passed: 7
Failed: 1
Duration: 45.23 seconds
Success Rate: 87.5%
```

## 🐛 Troubleshooting

### Common Issues

1. **MQTT Connection Failed**
   ```
   ✗ Failed to connect to MQTT broker: Connection refused
   ```
   **Solution**: Ensure MQTT broker is running or ur-vpn-manager can start it

2. **ur-vpn-manager Not Found**
   ```
   ❌ ur-vpn-manager binary not found
   ```
   **Solution**: Build the project first with `make -j4`

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

### Debug Mode

Enable verbose output by modifying the test files:
```python
# In BaseRPCTest.__init__
self.verbose = True  # Enable debug output
```

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
