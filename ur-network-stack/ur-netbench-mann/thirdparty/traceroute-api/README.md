
# Traceroute API

A C++ implementation of traceroute functionality with JSON configuration and output.

## Features

- JSON-based configuration
- JSON output format
- Customizable parameters (max hops, timeout, packet size, queries)
- Hostname resolution and reverse DNS lookup

## Building

```bash
cd traceroute-api
mkdir build && cd build
cmake ..
make
```

## Usage

The application requires root/sudo privileges to create raw sockets.

```bash
sudo ./traceroute_app ../example_config.json
```

## Configuration Format

```json
{
  "target": "google.com",
  "max_hops": 30,
  "timeout_ms": 5000,
  "packet_size": 60,
  "num_queries": 3
}
```

## Output Format

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
      "hostname": "router.local",
      "rtt_ms": 2.345,
      "timeout": false
    }
  ]
}
```

## Requirements

- C++17 compiler
- CMake 3.14+
- Root/sudo privileges for raw socket operations
- Internet connection for nlohmann/json download during build
