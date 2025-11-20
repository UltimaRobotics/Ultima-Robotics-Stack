
# QMI Watchdog - Real-time Modem Health Monitoring

QMI Watchdog is a comprehensive API and application for real-time monitoring of QMI modems. It provides automated failure detection, weighted health scoring, and continuous data collection with JSON output.

## Features

### Real-time Health Monitoring
- **Signal Metrics**: RSSI, RSRP, RSRQ, SNR monitoring
- **Data Statistics**: TX/RX bytes, packet loss detection
- **Network Information**: Registration status, roaming, carrier info
- **RF Band Information**: Frequency band and bandwidth monitoring

### Automated Failure Detection
- Configurable signal strength thresholds
- Packet loss rate monitoring
- Network registration failure detection
- Consecutive collection failure tracking

### Weighted Health Scoring
- Customizable component weights
- Overall health score (0-100)
- Health status classification (EXCELLENT/GOOD/FAIR/POOR/CRITICAL)
- Warning and critical issue identification

### JSON Output
- All metrics output as JSON strings to terminal
- Structured data format for easy parsing
- Timestamped data collection
- Error handling and status reporting

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Basic Monitoring
```bash
# Start continuous monitoring with default 5-second interval
./qmi_watchdog test_device.json

# Custom collection interval (2 seconds)
./qmi_watchdog -i 2000 test_device.json

# Single data collection
./qmi_watchdog --single-shot test_device.json
```

### Configuration Options
```bash
# Show help
./qmi_watchdog --help

# Custom timeout and disable health scoring
./qmi_watchdog -t 15000 --no-health-scoring test_device.json

# Show current status
./qmi_watchdog --status test_device.json
```

## Configuration Files

### Basic Device Configuration
```json
{
  "devices": [
    {
      "device_path": "/dev/cdc-wdm3",
      "imei": "123456789012345",
      "model": "Test LTE Modem",
      "manufacturer": "Test Manufacturer",
      "is_available": true
    }
  ]
}
```

### Advanced Configuration with Monitoring Settings
```json
{
  "devices": [
    {
      "device_path": "/dev/cdc-wdm3",
      "imei": "123456789012345",
      "model": "Test LTE Modem",
      "manufacturer": "Test Manufacturer",
      "is_available": true
    }
  ],
  "monitoring_config": {
    "collection_interval_ms": 3000,
    "timeout_ms": 15000,
    "enable_health_scoring": true,
    "health_weights": {
      "signal_weight": 0.4,
      "network_weight": 0.25,
      "data_weight": 0.25,
      "rf_weight": 0.1
    }
  },
  "failure_detection": {
    "critical_rssi_threshold": -105,
    "warning_rssi_threshold": -90,
    "critical_packet_loss_threshold": 0.03,
    "max_consecutive_failures": 5
  }
}
```

## API Usage

### Basic Implementation
```cpp
#include "qmi_watchdog.h"

QMIWatchdog watchdog;

// Load configuration
watchdog.loadDeviceConfigFromFile("device.json");

// Set up failure callback
watchdog.setFailureDetectionCallback([](const std::string& event, const std::vector<std::string>& failures) {
    std::cout << "Failures detected: " << failures.size() << std::endl;
});

// Start monitoring
watchdog.startMonitoring();

// Single data collection
MonitoringSnapshot snapshot = watchdog.collectFullSnapshot();
std::cout << snapshot.toJson() << std::endl;
```

### Custom Health Weights
```cpp
HealthWeights weights;
weights.signal_weight = 0.5;   // 50% signal importance
weights.network_weight = 0.3;  // 30% network importance
weights.data_weight = 0.15;    // 15% data importance
weights.rf_weight = 0.05;      // 5% RF importance

watchdog.setHealthWeights(weights);
```

## Output Examples

### Signal Metrics JSON
```json
{
  "type": "signal_metrics",
  "timestamp": 1704067200000,
  "status": 0,
  "status_text": "SUCCESS",
  "rssi_dbm": -54.0,
  "rsrq_db": -10.0,
  "rsrp_dbm": -88.0,
  "snr_db": 2.8,
  "radio_interface": "lte"
}
```

### Health Score JSON
```json
{
  "type": "health_score",
  "timestamp": 1704067200000,
  "overall_score": 85.5,
  "signal_score": 90.0,
  "network_score": 100.0,
  "data_score": 95.0,
  "rf_score": 80.0,
  "health_status": "GOOD",
  "warnings": [],
  "critical_issues": []
}
```

### Data Statistics JSON
```json
{
  "type": "data_statistics",
  "timestamp": 1704067200000,
  "status": 0,
  "status_text": "SUCCESS",
  "tx_packets_ok": 1758,
  "rx_packets_ok": 1029,
  "tx_packets_dropped": 0,
  "rx_packets_dropped": 0,
  "tx_bytes_ok": 484609,
  "rx_bytes_ok": 442182,
  "packet_loss_rate": 0.0
}
```

## Error Handling

The API handles various failure scenarios:

- **Collection Failures**: Device timeouts, QMI command errors
- **Parse Failures**: Invalid QMI output parsing
- **Device Errors**: Hardware or driver issues
- **Timeout Errors**: Command execution timeouts

All failures are reported with detailed error messages and proper status codes.

## Health Scoring Algorithm

### Signal Score (40% default weight)
- RSSI: -60dBm = 100%, -100dBm = 20%, <-100dBm = 0%
- RSRP: -80dBm = 100%, -120dBm = 20%, <-120dBm = 0%
- RSRQ: -5dB = 100%, -20dB = 20%, <-20dB = 0%
- SNR: 20dB = 100%, -5dB = 20%, <-5dB = 0%

### Network Score (25% default weight)
- Registration: Registered = 40%, Not registered = 0%
- Attachment: Both CS/PS = 20%, One = 10%, None = 0%
- Roaming: Home = 20%, Roaming = 10%
- Radio Interface: Present = 20%, None = 0%

### Data Score (25% default weight)
- Packet Loss: <1% = 100%, 1-2% = 80%, 2-5% = 50%, >5% = 0%
- Activity Bonus: +10% if data transfer detected

### RF Score (10% default weight)
- Base: 50% for successful collection
- LTE Bonus: +30%
- Bandwidth Bonus: 20MHz+ = +20%, 10MHz+ = +15%, 5MHz+ = +10%

## Dependencies

- **libjsoncpp**: JSON parsing and generation
- **pthread**: Threading support
- **qmicli**: QMI command line interface (runtime)
- **C++14**: Modern C++ features

## Compatibility

Compatible with the same device JSON formats used by `qmi_connection_manager`:
- Basic device format
- Advanced profile format
- Simple device configuration format

This ensures seamless integration with existing QMI connection management workflows.
