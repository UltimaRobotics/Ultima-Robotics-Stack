
# RPC Tests for ur-licence-mann

This directory contains Python test scripts for testing all operations of `ur-licence-mann` via RPC messaging over MQTT.

## Prerequisites

1. **MQTT Broker**: Ensure an MQTT broker (e.g., Mosquitto) is running on `localhost:1883`
2. **ur-licence-mann**: The `ur-licence-mann` binary must be running and listening to RPC requests
3. **Python Dependencies**: Install required Python packages:
   ```bash
   pip install paho-mqtt
   ```

## Test Structure

### Base Utilities
- `rpc_test_base.py`: Contains the `RpcTestClient` class for RPC communication

### Individual Test Scripts
- `test_rpc_generate.py`: Test license generation
- `test_rpc_verify.py`: Test license verification
- `test_rpc_update.py`: Test license updates
- `test_rpc_get_info.py`: Test getting license information
- `test_rpc_get_plan.py`: Test getting license plan
- `test_rpc_get_definitions.py`: Test getting license definitions
- `test_rpc_update_definitions.py`: Test updating license definitions

### Test Runner
- `test_rpc_all.py`: Runs all tests in sequence

## Running Tests

### Run Individual Test
```bash
cd ur-licence-mann/tests
python3 test_rpc_generate.py
```

### Run All Tests
```bash
cd ur-licence-mann/tests
python3 test_rpc_all.py
```

## Test Flow

1. Each test script:
   - Connects to the MQTT broker
   - Subscribes to the response topic (`direct_messaging/ur-licence-mann/responses`)
   - Publishes requests to the request topic (`direct_messaging/ur-licence-mann/requests`)
   - Waits for responses
   - Prints results

2. Request format follows the RPC message structure:
   ```json
   {
     "jsonrpc": "2.0",
     "id": "unique-request-id",
     "method": "operation_name",
     "params": {
       "operation": "operation_name",
       "parameters": {
         "param1": "value1",
         "param2": "value2"
       }
     }
   }
   ```

3. Response format:
   ```json
   {
     "jsonrpc": "2.0",
     "id": "unique-request-id",
     "result": {
       "success": true,
       "exit_code": 0,
       "message": "Operation completed",
       "data": {}
     }
   }
   ```

## Starting the Test Environment

1. **Start MQTT Broker**:
   ```bash
   mosquitto -v
   ```

2. **Start ur-licence-mann**:
   ```bash
   cd ur-licence-mann
   ./build/ur-licence-mann
   ```

3. **Run Tests**:
   ```bash
   cd tests
   python3 test_rpc_all.py
   ```

## Expected Output

Each test will print:
- Connection status
- Request sent confirmation
- Response received confirmation
- Operation results with success/failure status
- Detailed data for successful operations

## Troubleshooting

### Connection Issues
- Ensure MQTT broker is running on `localhost:1883`
- Check firewall settings
- Verify `ur-licence-mann` is running

### No Responses
- Check that `ur-licence-mann` is subscribed to the request topic
- Verify the RPC configuration in `config/ur-rpc-config.json`
- Check MQTT broker logs for message delivery

### Operation Failures
- Verify file paths exist (for verify/update operations)
- Check license definition files are present
- Ensure encryption keys are configured correctly

## Customization

You can customize the tests by modifying:
- Broker host/port in `RpcTestClient` initialization
- Request parameters in each test script
- Timeout values for operations
- Output file paths
