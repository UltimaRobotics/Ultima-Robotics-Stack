# Network Data Collector

A comprehensive C++ utility for collecting live network data including VLANs, NAT rules, firewall rules, static routes, and bridges. This tool executes terminal commands and parses their results to provide structured network information.

## Features

- **VLAN Information**: Collects VLAN configuration from multiple sources (vconfig, ip link, /proc/net/vlan)
- **NAT Rules**: Gathers NAT rules from iptables, nftables, and conntrack
- **Firewall Rules**: Collects firewall rules from iptables, nftables, UFW, and firewalld
- **Static Routes**: Retrieves routing information from ip route, route command, and /proc/net/route
- **Bridge Information**: Collects bridge configuration from brctl, ip link, and /proc/net/bridge

## Requirements

- C++17 compatible compiler
- CMake 3.16 or higher
- Linux system with network utilities
- Root privileges for some operations (recommended)

## Dependencies

The utility requires standard Linux network tools:
- `ip` (iproute2)
- `iptables` (optional)
- `nft` (optional)
- `brctl` (optional)
- `route` (net-tools, optional)
- `ufw` (optional)
- `firewall-cmd` (optional)
- `conntrack` (optional)
- `vconfig` (optional)

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Basic Usage

```bash
# Collect all network data
./network_collector

# Collect specific data types
./network_collector --vlan --nat

# Output to JSON format
./network_collector --json

# Save output to file
./network_collector --output network_data.txt

# Quiet mode (errors only)
./network_collector --quiet
```

### Command Line Options

- `-h, --help`: Show help message
- `-v, --vlan`: Collect VLAN information
- `-n, --nat`: Collect NAT rules
- `-f, --firewall`: Collect firewall rules
- `-r, --routes`: Collect static routes
- `-b, --bridges`: Collect bridge information
- `-a, --all`: Collect all network data (default)
- `-j, --json`: Output in JSON format
- `-o, --output FILE`: Save output to file
- `-q, --quiet`: Minimal output (errors only)

### Examples

```bash
# Collect all data and save to JSON file
./network_collector --json --output network_data.json

# Collect only VLAN and bridge information
./network_collector --vlan --bridges

# Collect firewall rules in quiet mode
./network_collector --firewall --quiet

# Get help
./network_collector --help
```

## Output Formats

### Standard Output
The tool outputs formatted tables for each network component type, including:
- VLAN configurations with IDs, names, interfaces, and IP addresses
- NAT rules with source, destination, ports, and targets
- Firewall rules with chains, protocols, interfaces, and packet counters
- Route information with destinations, gateways, and interfaces
- Bridge configurations with STP states and port information

### JSON Output
When using `--json` flag, the output is structured as:
```json
{
  "timestamp": "2025-12-07 15:30:45.123",
  "data": [
    {
      "type": "vlan",
      "timestamp": "2025-12-07 15:30:45.123",
      "data": "VLAN Configuration:\n==================\n..."
    },
    {
      "type": "nat",
      "timestamp": "2025-12-07 15:30:45.456",
      "data": "NAT Rules Configuration:\n========================\n..."
    }
  ]
}
```

## Permissions

Some network operations require root privileges:
- Reading iptables/nftables rules
- Accessing bridge configurations
- Reading detailed network interface information

Run with sudo for complete data collection:
```bash
sudo ./network_collector --all
```

## Error Handling

The utility gracefully handles missing commands or permissions:
- If a specific command is not available, it skips that data source
- If permission is denied, it reports the error and continues with other operations
- All errors are logged with timestamps

## Architecture

The utility is built with a modular architecture:
- `NetworkCollector`: Base class with command execution utilities
- `VlanCollector`: Specialized VLAN data collection
- `NatCollector`: NAT rules parsing from multiple sources
- `FirewallCollector`: Firewall rules aggregation
- `RouteCollector`: Routing information collection
- `BridgeCollector`: Bridge configuration parsing

Each collector uses multiple methods to gather data, ensuring compatibility across different Linux distributions and network configurations.

## Troubleshooting

### Common Issues

1. **Permission Denied**: Run with sudo for full functionality
2. **Command Not Found**: Install required network utilities
3. **Empty Output**: Check if network services are running and configured

### Debug Mode

For detailed error information, run without quiet mode:
```bash
./network_collector --all
```

## License

This utility is part of the Ultima Robotics Stack project.
