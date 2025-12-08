
# UR-Wireless-Tools RPC Python Tests

This directory contains Python test scripts for testing each RPC functionality of the ur-wireless-tools binary.

## Prerequisites

1. **MQTT Broker**: Ensure Mosquitto is running on port 1899:
   ```bash
   mosquitto -p 1899
   ```

2. **Python Dependencies**: Install paho-mqtt:
   ```bash
   pip3 install paho-mqtt
   ```

3. **UR-Wireless-Tools**: The binary must be running:
   ```bash
   cd ur-wireless-mann/build
   sudo ./ur-wireless-tools ../config/ur-rpc-config.json
   ```

## Test Files

Each test file targets a specific RPC functionality:

- **test_list_interfaces.py** - Tests listing all wireless interfaces
- **test_get_interface_info.py** - Tests getting detailed interface information
- **test_scan_networks.py** - Tests scanning for wireless networks
- **test_test_connection.py** - Tests connection testing to a network
- **test_get_status.py** - Tests getting system status and statistics
- **test_shutdown.py** - Tests graceful shutdown (requires confirmation)

## Running Tests

### Run Individual Tests

```bash
# List interfaces
python3 test_list_interfaces.py

# Get interface info (specify interface if needed)
python3 test_get_interface_info.py wlan0

# Scan networks (specify interface if needed)
python3 test_scan_networks.py wlan0

# Test connection (specify interface and SSID)
python3 test_test_connection.py wlan0 MyNetwork

# Get status
python3 test_get_status.py

# Shutdown (requires confirmation)
python3 test_shutdown.py
```

### Run All Tests

```bash
python3 run_all_tests.py
```

Note: The master test runner excludes the shutdown test.

## Test Output

Each test provides:
- Connection status to MQTT broker
- Request details (transaction ID, payload)
- Response validation
- Success/failure status

Example output:
```
==============================================================
LIST INTERFACES RPC TEST
==============================================================
✓ Connected to broker at 127.0.0.1:1899
✓ Subscribed to response topic
✓ Request published
✓ Response received
✓ Transaction ID matches
✓ Found 2 interface(s)
  - wlan0: up
  - wlan1: down
✓ TEST PASSED
```

## Configuration

Default settings in tests:
- Broker host: `127.0.0.1`
- Broker port: `1899`
- Request topic: `direct_messaging/ur-wireless-mann/requests`
- Response topic: `direct_messaging/ur-wireless-mann/responses`

Modify these in the test class constructors if needed.

## Troubleshooting

1. **Connection refused**: Ensure MQTT broker is running on port 1899
2. **No response**: Ensure ur-wireless-tools binary is running
3. **Permission denied**: Run with sudo if accessing wireless interfaces
4. **Timeout**: Increase timeout values in test scripts if operations take longer
