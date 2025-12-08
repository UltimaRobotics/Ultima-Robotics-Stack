
# Traceroute API - Usage Guide

This guide provides detailed instructions on how to use the Traceroute API both as a C++ library and as a command-line binary.

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
- CMake 3.14 or higher
- Root privileges or `CAP_NET_RAW` capability (for raw socket access)
- nlohmann/json header library

### Build Steps

```bash
# Navigate to the traceroute-api directory
cd net-bench-apis/traceroute-api

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

The traceroute utility requires raw socket access. You have two options:

**Option 1: Run with sudo (temporary)**
```bash
sudo ./build/traceroute_app config.json
```

**Option 2: Set capabilities (permanent)**
```bash
sudo setcap cap_net_raw+ep ./build/traceroute_app
./build/traceroute_app config.json
```

---

## Using the Binary

### Basic Usage

The `traceroute_app` binary requires a JSON configuration file as input.

```bash
sudo ./build/traceroute_app config.json
```

### Command-Line Options

The binary accepts a single argument: the path to a JSON configuration file.

```bash
sudo ./build/traceroute_app <config_file.json>
```

### Example Commands

**Trace route to Google DNS:**
```bash
sudo ./build/traceroute_app examples/traceroute_with_export.json
```

**Trace route to a specific IP:**
Create a config file `trace_ip.json`:
```json
{
  "target": "1.1.1.1",
  "max_hops": 30,
  "timeout_ms": 5000,
  "packet_size": 60,
  "num_queries": 3,
  "export_file_path": ""
}
```

Then run:
```bash
sudo ./build/traceroute_app trace_ip.json
```

**Trace with real-time export to file:**
```bash
sudo ./build/traceroute_app examples/traceroute_with_export.json
```

### Output Format

The binary produces two types of output:

#### 1. Real-time Hop Results

As each hop is discovered, you'll see:

```
Tracing route to google.com...

Hop 1: 192.168.1.1 (192.168.1.1) - 1.234 ms
Hop 2: 10.0.0.1 (gateway.isp.com) - 5.678 ms
Hop 3: 172.16.1.1 - 12.345 ms
Hop 4: * * * (timeout)
Hop 5: 142.250.185.46 (lga34s34-in-f14.1e100.net) - 23.456 ms
```

#### 2. Final Summary (JSON)

After the traceroute completes:

```json
{
  "error_message": "",
  "hops": [
    {
      "hop": 1,
      "hostname": "192.168.1.1",
      "ip": "192.168.1.1",
      "rtt_ms": 1.234,
      "timeout": false
    },
    {
      "hop": 2,
      "hostname": "gateway.isp.com",
      "ip": "10.0.0.1",
      "rtt_ms": 5.678,
      "timeout": false
    },
    {
      "hop": 3,
      "hostname": "172.16.1.1",
      "ip": "172.16.1.1",
      "rtt_ms": 12.345,
      "timeout": false
    },
    {
      "hop": 4,
      "hostname": "*",
      "ip": "*",
      "rtt_ms": 0.0,
      "timeout": true
    },
    {
      "hop": 5,
      "hostname": "lga34s34-in-f14.1e100.net",
      "ip": "142.250.185.46",
      "rtt_ms": 23.456,
      "timeout": false
    }
  ],
  "resolved_ip": "142.250.185.46",
  "success": true,
  "target": "google.com"
}
```

---

## Using the API in Your Code

### Basic Integration

#### 1. Include the Header

```cpp
#include "traceroute.hpp"
```

#### 2. Create and Configure

```cpp
using namespace traceroute;

// Create traceroute instance
Traceroute tracer;

// Configure the traceroute
TracerouteConfig config;
config.target = "google.com";
config.max_hops = 30;
config.timeout_ms = 5000;
config.packet_size = 60;
config.num_queries = 3;
config.export_file_path = "";
```

#### 3. Execute the Traceroute

```cpp
TracerouteResult result = tracer.execute(config);

if (result.success) {
    std::cout << "Traceroute successful!\n";
    std::cout << "Reached " << result.resolved_ip << "\n";
    std::cout << "Total hops: " << result.hops.size() << "\n";
} else {
    std::cerr << "Traceroute failed: " << result.error_message << "\n";
}
```

### Advanced Usage: Real-time Callbacks

Monitor hop results as they are discovered:

```cpp
using namespace traceroute;

Traceroute tracer;
TracerouteConfig config;
config.target = "google.com";
config.max_hops = 30;
config.timeout_ms = 5000;
config.packet_size = 60;
config.num_queries = 3;

// Set up real-time callback
auto hop_callback = [](const HopInfo& hop) {
    std::cout << "Hop " << hop.hop_number << ": ";
    if (hop.timeout) {
        std::cout << "* * * (timeout)" << std::endl;
    } else {
        std::cout << hop.ip_address;
        if (hop.hostname != hop.ip_address) {
            std::cout << " (" << hop.hostname << ")";
        }
        std::cout << " - " << hop.rtt_ms << " ms" << std::endl;
    }
};

TracerouteResult result = tracer.execute(config, hop_callback);
```

### Complete Example

```cpp
#include "traceroute.hpp"
#include <iostream>

int main() {
    using namespace traceroute;
    
    // Create traceroute instance
    Traceroute tracer;
    
    // Configure traceroute
    TracerouteConfig config;
    config.target = "8.8.8.8";
    config.max_hops = 30;
    config.timeout_ms = 5000;
    config.packet_size = 60;
    config.num_queries = 3;
    config.export_file_path = "traceroute_results.json";
    
    // Set real-time callback
    auto hop_callback = [](const HopInfo& hop) {
        std::cout << "Hop " << hop.hop_number << ": ";
        if (hop.timeout) {
            std::cout << "* * * (timeout)\n";
        } else {
            std::cout << hop.ip_address << " (" << hop.hostname << ") - " 
                      << hop.rtt_ms << " ms\n";
        }
    };
    
    // Execute
    std::cout << "Tracing route to " << config.target << "...\n\n";
    TracerouteResult result = tracer.execute(config, hop_callback);
    
    // Display summary
    std::cout << "\n=== Summary ===\n";
    std::cout << "Target: " << result.target << "\n";
    std::cout << "Resolved IP: " << result.resolved_ip << "\n";
    std::cout << "Total hops: " << result.hops.size() << "\n";
    std::cout << "Success: " << (result.success ? "Yes" : "No") << "\n";
    
    if (!result.error_message.empty()) {
        std::cout << "Error: " << result.error_message << "\n";
    }
    
    return result.success ? 0 : 1;
}
```

### Linking Against the Library

When building your project:

```cmake
# CMakeLists.txt
add_executable(my_app main.cpp)
target_sources(my_app PRIVATE 
    /path/to/traceroute-api/src/traceroute.cpp
)
target_include_directories(my_app PRIVATE 
    /path/to/traceroute-api/include
)
```

Or with g++ directly:

```bash
g++ -std=c++17 -I/path/to/traceroute-api/include \
    my_app.cpp /path/to/traceroute-api/src/traceroute.cpp \
    -o my_app
```

---

## Configuration Reference

### TracerouteConfig Structure

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `target` | `string` | *required* | Target hostname or IP address |
| `max_hops` | `int` | 30 | Maximum number of hops to trace |
| `timeout_ms` | `int` | 5000 | Timeout in milliseconds per hop query |
| `packet_size` | `int` | 60 | UDP packet size in bytes |
| `num_queries` | `int` | 3 | Number of query attempts per hop |
| `export_file_path` | `string` | "" | Path to export real-time JSON results |

### JSON Configuration Example

```json
{
  "target": "google.com",
  "max_hops": 30,
  "timeout_ms": 5000,
  "packet_size": 60,
  "num_queries": 3,
  "export_file_path": "traceroute_export.json"
}
```

### Configuration Best Practices

- **max_hops**: Usually 30 is sufficient for most internet routes. Increase if tracing very distant targets.
- **timeout_ms**: 5000ms (5 seconds) is recommended. Lower values may miss slower hops.
- **num_queries**: 3 queries per hop provides good reliability. Increase for unstable routes.
- **packet_size**: Default 60 bytes is standard. Larger sizes may help detect MTU issues.

---

## Output Format

### HopInfo Structure

| Field | Type | Description |
|-------|------|-------------|
| `hop_number` | `int` | The hop sequence number (1-based) |
| `ip_address` | `string` | IP address of the hop (or "*" if timeout) |
| `hostname` | `string` | Resolved hostname (or IP if no reverse DNS) |
| `rtt_ms` | `double` | Round-trip time in milliseconds |
| `timeout` | `bool` | Whether this hop timed out |

### TracerouteResult Structure

| Field | Type | Description |
|-------|------|-------------|
| `target` | `string` | Original target (hostname or IP) |
| `resolved_ip` | `string` | Resolved IP address of the target |
| `hops` | `vector<HopInfo>` | List of all discovered hops |
| `success` | `bool` | Whether the destination was reached |
| `error_message` | `string` | Error message if failed |

### JSON Output Example

```json
{
  "target": "google.com",
  "resolved_ip": "142.250.185.46",
  "success": true,
  "error_message": "",
  "hops": [
    {
      "hop": 1,
      "ip": "192.168.1.1",
      "hostname": "192.168.1.1",
      "rtt_ms": 1.234,
      "timeout": false
    },
    {
      "hop": 2,
      "ip": "10.0.0.1",
      "hostname": "gateway.isp.com",
      "rtt_ms": 5.678,
      "timeout": false
    }
  ]
}
```

---

## Real-time Export Feature

When `export_file_path` is specified, the API writes results to a JSON file in real-time as each hop is discovered.

### Export File Structure

```json
{
  "trace": {
    "target": "google.com",
    "max_hops": 30,
    "timeout_ms": 5000,
    "packet_size": 60,
    "num_queries": 3
  },
  "hops": [
    {
      "hop": 1,
      "ip": "192.168.1.1",
      "hostname": "192.168.1.1",
      "rtt_ms": 1.234,
      "timeout": false
    },
    {
      "hop": 2,
      "ip": "10.0.0.1",
      "hostname": "gateway.isp.com",
      "rtt_ms": 5.678,
      "timeout": false
    },
    {
      "hop": 3,
      "ip": "*",
      "hostname": "*",
      "rtt_ms": 0.0,
      "timeout": true
    }
  ],
  "summary": {
    "resolved_ip": "142.250.185.46",
    "success": true,
    "total_hops": 15
  }
}
```

### Export Behavior

- **File Creation**: If the file doesn't exist, it's created automatically
- **File Cleanup**: If the file exists, it's truncated (cleared) before writing
- **Real-time Updates**: Each hop result is written immediately and flushed
- **Summary Statistics**: Written at the end with final results
- **Error Handling**: If the trace fails early, a summary with error information is still written

### Monitoring Export Files

You can monitor the export file in real-time:

```bash
# Run traceroute with export
sudo ./build/traceroute_app examples/traceroute_with_export.json &

# Watch the file update in real-time
watch -n 0.5 cat traceroute_export_results.json
```

Or use `tail -f` for live updates:

```bash
tail -f traceroute_export_results.json
```

---

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Failed to create sockets. Run with sudo/root privileges." | Insufficient permissions | Run with sudo or set CAP_NET_RAW capability |
| "Failed to resolve hostname" | Invalid hostname or DNS issue | Check hostname and DNS configuration |
| "Maximum hops reached without reaching destination" | Target not reachable within max_hops | Increase max_hops or check network connectivity |
| "Failed to open export file" | Permission or path issue | Verify file path and write permissions |

### Checking for Errors

```cpp
TracerouteResult result = tracer.execute(config);

if (!result.success) {
    std::cerr << "Error: " << result.error_message << "\n";
    
    // Check if partial results are available
    if (!result.hops.empty()) {
        std::cout << "Partial trace available (" 
                  << result.hops.size() << " hops)\n";
    }
}
```

---

## Understanding Traceroute Results

### Interpreting Hops

- **Successful Hop**: Shows IP, hostname, and RTT
- **Timeout Hop**: Indicated by `*` and `timeout: true` - router didn't respond
- **Multiple Timeouts**: May indicate firewall blocking ICMP or rate limiting
- **Increasing RTT**: Normal as distance increases
- **RTT Fluctuations**: May indicate network congestion or varied routing

### Typical Route Patterns

```
Hop 1: Local router (< 5ms)
Hop 2-3: ISP network (5-20ms)
Hop 4-10: Internet backbone (20-100ms)
Hop 11+: Destination network
```

### Troubleshooting with Traceroute

**Problem: Can't reach destination**
```
Solution: Check where the route stops - last responding hop indicates problem area
```

**Problem: High latency at specific hop**
```
Solution: Issue likely at that router or the link to it
```

**Problem: All hops timeout after certain point**
```
Solution: Firewall blocking ICMP, or route filtering
```

---

## Performance Considerations

- **num_queries**: More queries per hop increase accuracy but take longer
- **timeout_ms**: Balance between waiting for slow hops and overall execution time
- **max_hops**: Most routes complete within 15-20 hops; 30 is usually sufficient
- **Execution Time**: Approximately `max_hops * timeout_ms * num_queries / 1000` seconds in worst case

---

## Examples

See the `examples/` directory for ready-to-use configurations:

- `traceroute_with_export.json` - Trace with real-time export to file

Run the example:

```bash
sudo ./build/traceroute_app examples/traceroute_with_export.json
```

### Custom Example Configurations

**Fast trace (reduced queries and timeout):**
```json
{
  "target": "8.8.8.8",
  "max_hops": 20,
  "timeout_ms": 2000,
  "packet_size": 60,
  "num_queries": 1,
  "export_file_path": ""
}
```

**Detailed trace (more queries):**
```json
{
  "target": "google.com",
  "max_hops": 30,
  "timeout_ms": 5000,
  "packet_size": 60,
  "num_queries": 5,
  "export_file_path": "detailed_trace.json"
}
```

---

## Security Considerations

- **Root Privileges**: Required for raw socket access - use capabilities instead of running as root
- **Network Exposure**: Traceroute generates network traffic that may be logged or blocked
- **Firewall Rules**: Some networks block ICMP or UDP probes
- **Rate Limiting**: Aggressive tracing may trigger rate limits or security alerts

---

## Differences from Standard Traceroute

This implementation:
- Uses UDP probes (like standard Unix traceroute)
- Requires raw socket access (ICMP reception)
- Provides JSON output for easy parsing
- Supports real-time export to file
- Includes hostname resolution via reverse DNS
- Configurable via JSON instead of command-line flags

---

## Frequently Asked Questions

**Q: Why do I see timeouts in the middle of the route?**
A: Some routers don't respond to traceroute probes due to security policies, but still forward packets.

**Q: Can I trace IPv6 routes?**
A: Currently, this implementation only supports IPv4. IPv6 support may be added in future versions.

**Q: Why are RTT values sometimes inconsistent?**
A: Network conditions vary. Multiple queries per hop help identify consistent patterns.

**Q: Can I run this without root privileges?**
A: No, raw socket access requires elevated privileges. Use capabilities for a more secure approach.

**Q: How accurate is the RTT measurement?**
A: RTT is measured from probe send to reply receipt, accurate to microseconds on modern systems.
