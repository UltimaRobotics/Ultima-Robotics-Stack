# Endpoint Monitoring Test

This test validates that the endpoint monitoring system in ur-mavrouter is publishing data correctly to the MQTT topic.

## Purpose

- Subscribe to `ur-shared-bus/ur-mavlink-stack/ur-mavrouter/notification` topic
- Collect endpoint monitoring messages published by the endpoint monitor
- Verify that data is published as raw JSON (not response/request structure)
- Confirm non-blocking operation in the RPC client thread
- Save collected data for analysis

## Prerequisites

1. **ur-mavrouter must be running** with endpoint monitoring enabled
2. **MQTT broker must be accessible** on 127.0.0.1:1899
3. **RPC client must be configured** for endpoint monitoring publishing
4. **Python dependencies installed** (paho-mqtt>=1.6.0)

## Running the Test

### Quick Start
```bash
cd /home/fyou/Desktop/Ultima-Robotics-Stack/ur-mavlink-stack/ur-mavrouter-v1.1-rpc/tests
./run_endpoint_monitoring_test.py
```

### Direct Execution
```bash
python3 test_endpoint_monitoring.py
```

### Custom Parameters
```python
from test_endpoint_monitoring import EndpointMonitoringCollector

# Create collector with custom settings
collector = EndpointMonitoringCollector(
    broker_host="127.0.0.1",
    broker_port=1899,
    client_id="custom_test_client"
)

# Collect for 60 seconds or up to 100 messages
collector.start_collection(duration_seconds=60, max_messages=100)

# Print summary and save to file
collector.print_summary()
collector.save_to_file("my_endpoint_data.json")
```

## Expected Output

When successful, you should see:

1. **Connection confirmation** to MQTT broker
2. **Subscription confirmation** to the monitoring topic
3. **Received messages** showing:
   - Message count and sequence numbers
   - Endpoint summary (total/occupied endpoints)
   - Individual endpoint details
4. **Collection summary** with statistics
5. **Data file** saved with timestamp

### Sample Message Format

```json
{
  "timestamp": "2025-11-23T14:44:00.123Z",
  "sequence": 42,
  "endpoints": {
    "main_router": [
      {
        "name": "TCP:127.0.0.1:5760",
        "type": "TCP",
        "occupied": true,
        "connection_state": 2,
        "active_connections": 1
      }
    ]
  },
  "summary": {
    "total_endpoints": 1,
    "occupied_endpoints": 1,
    "monitoring_enabled": true,
    "connection_tracking": true
  }
}
```

## Troubleshooting

### No Messages Received
- **Check ur-mavrouter is running**: `ps aux | grep ur-mavrouter`
- **Verify endpoint monitoring**: Look for "Endpoint monitoring started" in logs
- **Check RPC client**: Look for "RPC client configured for endpoint monitoring" in logs
- **Verify MQTT broker**: `telnet 127.0.0.1 1899`

### Connection Errors
- **MQTT broker not running**: Start the MQTT service
- **Wrong broker/port**: Check configuration files
- **Firewall blocking**: Ensure port 1899 is accessible

### Invalid JSON Messages
- **Check endpoint monitor implementation**: Ensure `publishRawData` is used
- **Verify RPC client**: Confirm it's properly initialized

## Test Configuration

Default test parameters:
- **Broker**: 127.0.0.1:1899
- **Topic**: `ur-shared-bus/ur-mavlink-stack/ur-mavrouter/notification`
- **Duration**: 30 seconds
- **Max messages**: 50
- **QoS**: 1

## Output Files

The test saves data to JSON files with format:
```
endpoint_monitoring_data_YYYYMMDD_HHMMSS.json
```

Each file contains:
- Collection metadata (topic, message count, timestamps)
- All received messages with full payloads
- Timestamps and sequence numbers

## Integration with System

This test validates the complete data flow:
1. **EndpointMonitor** detects endpoint changes
2. **RPC client** publishes raw JSON via `publishRawData()`
3. **MQTT broker** distributes messages
4. **Test client** receives and validates messages

The test confirms that:
- Data is published as **raw JSON** (not wrapped in response/request)
- Publishing is **non-blocking** (RPC client thread)
- Messages contain **endpoint status information**
- **Sequence numbers** increment properly
- **Timestamps** are included for timing analysis
