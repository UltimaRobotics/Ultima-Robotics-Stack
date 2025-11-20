
# UR Ping API

A C++ library and CLI tool for performing ICMP ping operations without using terminal/shell execution.

## Features

- Pure C++ implementation using raw sockets
- ICMP Echo Request/Reply handling
- JSON configuration input
- JSON formatted results output
- RTT statistics (min, max, avg, stddev)
- Hostname resolution
- Configurable timeout, interval, packet size, TTL

## Requirements

- C++17 compiler
- CMake 3.10+
- Root privileges or CAP_NET_RAW capability (for raw socket creation)
- nlohmann/json single header library

## Building

```bash
# Download nlohmann json header first
cd ur-ping-api/include
wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp

# Build the project
cd ..
mkdir -p build
cd build
cmake ..
make
```

## Usage

### Command Line

```bash
# Using config file
sudo ./ping_cli --config ../examples/ping_google.json

# Using JSON string
sudo ./ping_cli --json '{"destination":"8.8.8.8","count":5}'

# Show help
./ping_cli --help

# Show example configuration
./ping_cli --example
```

### JSON Configuration Format

```json
{
  "destination": "google.com",
  "count": 4,
  "timeout_ms": 1000,
  "interval_ms": 1000,
  "packet_size": 56,
  "ttl": 64,
  "resolve_hostname": true
}
```

### JSON Output Format

```json
{
  "destination": "google.com",
  "ip_address": "142.250.185.46",
  "packets_sent": 4,
  "packets_received": 4,
  "packets_lost": 0,
  "loss_percentage": 0.0,
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
    },
    ...
  ]
}
```

## API Usage

```cpp
#include "PingAPI.hpp"

PingConfig config;
config.destination = "google.com";
config.count = 5;
config.timeout_ms = 1000;

PingAPI ping;
ping.setConfig(config);

PingResult result = ping.execute();

if (result.success) {
    std::cout << "Average RTT: " << result.avg_rtt_ms << " ms\n";
}
```

## Permissions

The program requires root privileges or CAP_NET_RAW capability:

```bash
# Run with sudo
sudo ./ping_cli --config config.json

# Or set capabilities (run once)
sudo setcap cap_net_raw+ep ./ping_cli
./ping_cli --config config.json
```
