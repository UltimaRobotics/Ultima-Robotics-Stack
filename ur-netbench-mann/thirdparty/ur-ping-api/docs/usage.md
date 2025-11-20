
# UR Ping API - Usage Guide

This guide provides detailed instructions on how to use the UR Ping API both as a C++ library and as a command-line binary.

## Table of Contents

1. [Building the Project](#building-the-project)
2. [Using the Binary](#using-the-binary)
3. [Using the API in Your Code](#using-the-api-in-your-code)
4. [Configuration Reference](#configuration-reference)
5. [Output Format](#output-format)
6. [Real-time Export Feature](#real-time-export-feature)

---

## Building the Project

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+)
- CMake 3.10 or higher
- Root privileges or `CAP_NET_RAW` capability
- nlohmann/json header library

### Build Steps

```bash
# Navigate to the ur-ping-api directory
cd net-bench-apis/ur-ping-api

# Download the nlohmann json header
cd include
wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
cd ..

# Build using CMake
mkdir -p build
cd build
cmake ..
make -j4

# Or use the provided Makefile
cd ..
make
```

### Setting Permissions

The ping utility requires raw socket access. You have two options:

**Option 1: Run with sudo (temporary)**
```bash
sudo ./build/ping_cli --config config.json
```

**Option 2: Set capabilities (permanent)**
```bash
sudo setcap cap_net_raw+ep ./build/ping_cli
./build/ping_cli --config config.json
```

---

## Using the Binary

### Basic Usage

The `ping_cli` binary accepts configuration via JSON file or JSON string.

#### Using a Configuration File

```bash
sudo ./build/ping_cli --config examples/ping_google.json
```

#### Using a JSON String

```bash
sudo ./build/ping_cli --json '{"destination":"8.8.8.8","count":5}'
```

### Command-Line Options

| Option | Short | Description |
|--------|-------|-------------|
| `--config <file>` | `-c` | Load configuration from a JSON file |
| `--json <string>` | `-j` | Load configuration from a JSON string |
| `--help` | `-h` | Display help message |
| `--example` | `-e` | Show example JSON configuration |

### Example Commands

**Ping Google DNS with 10 packets:**
```bash
sudo ./build/ping_cli --json '{"destination":"8.8.8.8","count":10,"interval_ms":500}'
```

**Ping a hostname with custom timeout:**
```bash
sudo ./build/ping_cli --json '{"destination":"google.com","count":5,"timeout_ms":2000}'
```

**Ping with real-time export to file:**
```bash
sudo ./build/ping_cli --config examples/ping_with_export.json
```

### Output Format

The binary produces two types of output:

#### 1. Real-time Results (per packet)

As each ping is sent and received, you'll see:

```json
PING_RESULT: {"sequence":0,"success":true,"rtt_ms":14.523,"ttl":117}
PING_RESULT: {"sequence":1,"success":true,"rtt_ms":13.892,"ttl":117}
PING_RESULT: {"sequence":2,"success":false,"error":"Timeout waiting for reply"}
```

#### 2. Final Summary

After all pings complete:

```json
FINAL_SUMMARY: {
  "destination": "google.com",
  "ip_address": "142.250.185.46",
  "packets_sent": 5,
  "packets_received": 4,
  "packets_lost": 1,
  "loss_percentage": 20.0,
  "success": true,
  "rtt_min_ms": 12.5,
  "rtt_max_ms": 15.8,
  "rtt_avg_ms": 14.2,
  "rtt_stddev_ms": 1.3,
  "ping_results": [
    {
      "sequence": 0,
      "rtt_ms": 14.5,
      "ttl": 117
    }
  ]
}
```

---

## Using the API in Your Code

### Basic Integration

#### 1. Include the Header

```cpp
#include "PingAPI.hpp"
```

#### 2. Create and Configure

```cpp
// Create ping instance
PingAPI ping;

// Configure the ping
PingConfig config;
config.destination = "8.8.8.8";
config.count = 5;
config.timeout_ms = 1000;
config.interval_ms = 1000;
config.packet_size = 56;
config.ttl = 64;
config.resolve_hostname = true;

ping.setConfig(config);
```

#### 3. Execute the Ping

```cpp
PingResult result = ping.execute();

if (result.success) {
    std::cout << "Ping successful!\n";
    std::cout << "Average RTT: " << result.avg_rtt_ms << " ms\n";
    std::cout << "Packet loss: " << result.loss_percentage << "%\n";
} else {
    std::cerr << "Ping failed: " << result.error_message << "\n";
}
```

### Advanced Usage: Real-time Callbacks

Monitor ping results as they arrive:

```cpp
PingAPI ping;
PingConfig config;
config.destination = "google.com";
config.count = 10;

// Set up real-time callback
ping.setRealtimeCallback([](const PingRealtimeResult& rt_result) {
    if (rt_result.success) {
        std::cout << "Seq " << rt_result.sequence 
                  << ": RTT=" << rt_result.rtt_ms << "ms "
                  << "TTL=" << rt_result.ttl << "\n";
    } else {
        std::cerr << "Seq " << rt_result.sequence 
                  << ": " << rt_result.error_message << "\n";
    }
});

ping.setConfig(config);
PingResult result = ping.execute();
```

### Complete Example

```cpp
#include "PingAPI.hpp"
#include <iostream>

int main() {
    PingAPI ping;
    
    // Configure ping
    PingConfig config;
    config.destination = "8.8.8.8";
    config.count = 5;
    config.timeout_ms = 1000;
    config.interval_ms = 1000;
    config.export_file_path = "ping_results.json";
    
    ping.setConfig(config);
    
    // Set real-time callback
    ping.setRealtimeCallback([](const PingRealtimeResult& rt) {
        std::cout << "Ping #" << rt.sequence << ": ";
        if (rt.success) {
            std::cout << rt.rtt_ms << " ms\n";
        } else {
            std::cout << "Failed - " << rt.error_message << "\n";
        }
    });
    
    // Execute
    PingResult result = ping.execute();
    
    // Display summary
    std::cout << "\n=== Summary ===\n";
    std::cout << "Packets sent: " << result.packets_sent << "\n";
    std::cout << "Packets received: " << result.packets_received << "\n";
    std::cout << "Loss: " << result.loss_percentage << "%\n";
    
    if (result.success) {
        std::cout << "Min RTT: " << result.min_rtt_ms << " ms\n";
        std::cout << "Max RTT: " << result.max_rtt_ms << " ms\n";
        std::cout << "Avg RTT: " << result.avg_rtt_ms << " ms\n";
        std::cout << "StdDev: " << result.stddev_rtt_ms << " ms\n";
    }
    
    return result.success ? 0 : 1;
}
```

### Linking Against the Library

When building your project:

```cmake
# CMakeLists.txt
add_executable(my_app main.cpp)
target_link_libraries(my_app ping_api)
```

Or with g++ directly:

```bash
g++ -std=c++17 -I/path/to/ur-ping-api/include \
    my_app.cpp /path/to/ur-ping-api/build/libping_api.a \
    -o my_app
```

---

## Configuration Reference

### PingConfig Structure

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `destination` | `string` | *required* | Target hostname or IP address |
| `count` | `int` | 4 | Number of ping packets to send |
| `timeout_ms` | `int` | 1000 | Timeout in milliseconds per packet |
| `interval_ms` | `int` | 1000 | Interval between pings in milliseconds |
| `packet_size` | `int` | 56 | ICMP data size in bytes (excluding header) |
| `ttl` | `int` | 64 | Time-To-Live value for packets |
| `resolve_hostname` | `bool` | true | Resolve hostname to IP address |
| `export_file_path` | `string` | "" | Path to export real-time JSON results |

### JSON Configuration Example

```json
{
  "destination": "google.com",
  "count": 10,
  "timeout_ms": 2000,
  "interval_ms": 500,
  "packet_size": 64,
  "ttl": 128,
  "resolve_hostname": true,
  "export_file_path": "results.json"
}
```

---

## Output Format

### PingResult Structure

| Field | Type | Description |
|-------|------|-------------|
| `destination` | `string` | Original destination (hostname or IP) |
| `ip_address` | `string` | Resolved IP address |
| `packets_sent` | `int` | Total packets transmitted |
| `packets_received` | `int` | Total packets received |
| `packets_lost` | `int` | Total packets lost |
| `loss_percentage` | `double` | Packet loss percentage |
| `min_rtt_ms` | `double` | Minimum round-trip time |
| `max_rtt_ms` | `double` | Maximum round-trip time |
| `avg_rtt_ms` | `double` | Average round-trip time |
| `stddev_rtt_ms` | `double` | Standard deviation of RTT |
| `rtt_times` | `vector<double>` | All RTT measurements |
| `sequence_numbers` | `vector<int>` | Sequence numbers of received packets |
| `ttl_values` | `vector<int>` | TTL values from responses |
| `success` | `bool` | Overall success status |
| `error_message` | `string` | Error message if failed |

### PingRealtimeResult Structure

| Field | Type | Description |
|-------|------|-------------|
| `sequence` | `int` | Packet sequence number |
| `rtt_ms` | `double` | Round-trip time in milliseconds |
| `ttl` | `int` | Time-To-Live from response |
| `success` | `bool` | Success status for this packet |
| `error_message` | `string` | Error message if failed |

---

## Real-time Export Feature

When `export_file_path` is specified, the API writes results to a JSON file in real-time.

### Export File Structure

```json
{
  "results": [
    {
      "sequence": 0,
      "success": true,
      "rtt_ms": 14.523,
      "ttl": 117
    },
    {
      "sequence": 1,
      "success": false,
      "error": "Timeout waiting for reply"
    }
  ],
  "summary": {
    "packets_sent": 5,
    "packets_received": 4,
    "packets_lost": 1,
    "loss_percentage": 20.0,
    "rtt_min_ms": 12.5,
    "rtt_max_ms": 15.8,
    "rtt_avg_ms": 14.2,
    "rtt_stddev_ms": 1.3
  }
}
```

### Export Behavior

- **File Creation**: If the file doesn't exist, it's created automatically
- **File Cleanup**: If the file exists, it's truncated (cleared) before writing
- **Real-time Updates**: Each ping result is written immediately and flushed
- **Summary Statistics**: Written at the end of the test with all metrics
- **Error Handling**: If the test fails early, a summary with error information is still written

### Monitoring Export Files

You can monitor the export file in real-time:

```bash
# Run ping with export
sudo ./build/ping_cli --config examples/ping_with_export.json &

# Watch the file update in real-time
watch -n 0.5 cat ping_export_results.json
```

Or use `tail -f` for live updates:

```bash
tail -f ping_export_results.json
```

---

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Failed to create socket. Need root/CAP_NET_RAW privileges" | Insufficient permissions | Run with sudo or set capabilities |
| "Failed to resolve hostname" | Invalid hostname or DNS issue | Check hostname and network connectivity |
| "Timeout waiting for reply" | No response received | Check network, firewall, or increase timeout |
| "Failed to send packet" | Network interface issue | Check network configuration |
| "Failed to open export file" | Permission or path issue | Verify file path and write permissions |

### Checking for Errors

```cpp
PingResult result = ping.execute();

if (!result.success) {
    std::cerr << "Error: " << result.error_message << "\n";
    std::cerr << "Last API error: " << ping.getLastError() << "\n";
}
```

---

## Performance Considerations

- **Interval**: Minimum recommended interval is 100ms to avoid overwhelming the network
- **Timeout**: Should be less than interval to prevent overlapping requests
- **Packet Size**: Larger packets (up to 65507 bytes) may fragment across networks
- **Count**: High counts (>1000) may take significant time; consider using callbacks for progress

---

## Examples

See the `examples/` directory for ready-to-use configurations:

- `ping_google.json` - Ping Google's DNS server
- `ping_ip.json` - Ping a specific IP address
- `ping_with_export.json` - Ping with real-time export to file

Run any example:

```bash
sudo ./build/ping_cli --config examples/ping_google.json
```
