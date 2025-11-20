# OpenVPN C++ Wrapper Demo - Usage Documentation

## Overview

The `openvpn_cpp_demo` is a JSON-based OpenVPN client wrapper built on top of the ur-openvpn-library. It provides a programmatic interface to OpenVPN's core functionality with real-time event streaming and connection monitoring through JSON output.

## Features

- **JSON Event Streaming**: All events, status updates, and errors are output as JSON
- **Real-time Connection Monitoring**: Periodic status updates every 5 seconds
- **Automatic Reconnection**: Attempts to reconnect after connection errors
- **Signal Handling**: Graceful shutdown on SIGINT (Ctrl+C) and SIGTERM
- **Statistics Tracking**: Real-time network statistics and connection metrics
- **Standard OpenVPN Config Support**: Works with standard .ovpn configuration files

## Binary Location

After building, the binary is located at:
```
ur-openvpn-library/src/build/openvpn_cpp_demo
```

## Usage

### Basic Usage

```bash
./openvpn_cpp_demo <config_file_path>
```

### Example

```bash
./openvpn_cpp_demo /path/to/your/config.ovpn
```

## OpenVPN Configuration File

The wrapper accepts standard OpenVPN configuration files (.ovpn format). Here's an example configuration:

```ovpn
# Client configuration
client
dev tun
proto udp
remote vpn.example.com 1194
resolv-retry infinite
nobind
persist-key
persist-tun

# Cryptographic settings
ca ca.crt
cert client.crt
key client.key
cipher AES-256-CBC
auth SHA256

# Logging
verb 3
```

### Required Privileges

Since OpenVPN needs to create network interfaces, the binary must be run with appropriate privileges:

```bash
# Option 1: Run as root (not recommended for production)
sudo ./openvpn_cpp_demo config.ovpn

# Option 2: Use capabilities (recommended)
sudo setcap cap_net_admin+eip ./openvpn_cpp_demo
./openvpn_cpp_demo config.ovpn
```

## JSON Output Format

All output is in JSON format, making it easy to parse and integrate with other tools.

### Event Types

#### 1. Startup Event
```json
{
  "type": "startup",
  "message": "OpenVPN wrapper starting",
  "config_file": "/path/to/config.ovpn",
  "pid": 12345
}
```

#### 2. Connection Events
```json
{
  "type": "connecting",
  "message": "Initiating connection to VPN server",
  "state": "CONNECTING",
  "timestamp": 1234567890
}
```

```json
{
  "type": "connected",
  "message": "Successfully connected to VPN",
  "state": "CONNECTED",
  "timestamp": 1234567890,
  "data": {
    "local_ip": "10.8.0.2",
    "remote_ip": "192.168.1.1"
  }
}
```

#### 3. Status Updates (every 5 seconds)
```json
{
  "type": "status",
  "state": "CONNECTED",
  "connected": true,
  "local_ip": "10.8.0.2",
  "remote_ip": "192.168.1.1",
  "server_ip": "vpn.example.com"
}
```

#### 4. Statistics Events (periodic)
```json
{
  "type": "stats",
  "bytes_sent": 1048576,
  "bytes_received": 2097152,
  "tun_read_bytes": 1048576,
  "tun_write_bytes": 2097152,
  "ping_ms": 45,
  "local_ip": "10.8.0.2",
  "remote_ip": "192.168.1.1",
  "server_ip": "vpn.example.com",
  "uptime_seconds": 3600
}
```

#### 5. Error Events
```json
{
  "type": "error",
  "message": "Config file not found",
  "file": "/invalid/path/config.ovpn"
}
```

```json
{
  "type": "initialization_error",
  "error": "Failed to parse configuration",
  "message": "Invalid option in config file"
}
```

```json
{
  "type": "runtime_error",
  "error": "Connection lost",
  "message": "Network unreachable"
}
```

#### 6. Signal Events
```json
{
  "type": "signal",
  "signal": 2,
  "message": "Received signal, shutting down..."
}
```

#### 7. Reconnection Events
```json
{
  "type": "reconnecting",
  "message": "Attempting to reconnect after error"
}
```

#### 8. Shutdown Event
```json
{
  "type": "shutdown",
  "message": "OpenVPN wrapper stopped successfully"
}
```

## Integration Examples

### Shell Script Integration

```bash
#!/bin/bash

# Parse JSON output and monitor connection
./openvpn_cpp_demo config.ovpn | while IFS= read -r line; do
    type=$(echo "$line" | jq -r '.type')
    
    case "$type" in
        "connected")
            echo "VPN Connected!"
            local_ip=$(echo "$line" | jq -r '.data.local_ip')
            echo "Local IP: $local_ip"
            ;;
        "error"|"runtime_error"|"initialization_error")
            echo "Error occurred: $(echo "$line" | jq -r '.message')"
            ;;
        "stats")
            bytes_sent=$(echo "$line" | jq -r '.bytes_sent')
            bytes_recv=$(echo "$line" | jq -r '.bytes_received')
            echo "Sent: $bytes_sent bytes, Received: $bytes_recv bytes"
            ;;
    esac
done
```

### Python Integration

```python
#!/usr/bin/env python3

import subprocess
import json
import sys

def monitor_vpn(config_file):
    process = subprocess.Popen(
        ['./openvpn_cpp_demo', config_file],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    
    try:
        for line in process.stdout:
            event = json.loads(line.strip())
            event_type = event.get('type')
            
            if event_type == 'connected':
                print(f"✓ Connected! IP: {event['data'].get('local_ip')}")
            elif event_type in ['error', 'runtime_error']:
                print(f"✗ Error: {event.get('message')}", file=sys.stderr)
            elif event_type == 'stats':
                print(f"↑ {event['bytes_sent']} bytes  ↓ {event['bytes_received']} bytes")
            elif event_type == 'shutdown':
                print("VPN disconnected")
                break
                
    except KeyboardInterrupt:
        process.terminate()
        process.wait()

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python3 monitor.py <config.ovpn>")
        sys.exit(1)
    
    monitor_vpn(sys.argv[1])
```

### Node.js Integration

```javascript
const { spawn } = require('child_process');
const readline = require('readline');

function monitorVPN(configFile) {
    const vpn = spawn('./openvpn_cpp_demo', [configFile]);
    
    const rl = readline.createInterface({
        input: vpn.stdout,
        crlfDelay: Infinity
    });
    
    rl.on('line', (line) => {
        try {
            const event = JSON.parse(line);
            
            switch (event.type) {
                case 'connected':
                    console.log(`✓ Connected! IP: ${event.data.local_ip}`);
                    break;
                case 'error':
                case 'runtime_error':
                    console.error(`✗ Error: ${event.message}`);
                    break;
                case 'stats':
                    console.log(`Stats: ↑${event.bytes_sent} ↓${event.bytes_received}`);
                    break;
                case 'shutdown':
                    console.log('VPN disconnected');
                    break;
            }
        } catch (err) {
            console.error('Failed to parse JSON:', line);
        }
    });
    
    vpn.stderr.on('data', (data) => {
        console.error(`stderr: ${data}`);
    });
    
    vpn.on('close', (code) => {
        console.log(`Process exited with code ${code}`);
    });
    
    // Handle graceful shutdown
    process.on('SIGINT', () => {
        vpn.kill('SIGTERM');
    });
}

if (process.argv.length !== 3) {
    console.error('Usage: node monitor.js <config.ovpn>');
    process.exit(1);
}

monitorVPN(process.argv[2]);
```

## Monitoring and Logging

### Save All Events to File

```bash
./openvpn_cpp_demo config.ovpn | tee vpn_events.jsonl
```

### Filter Specific Event Types

```bash
# Only show errors
./openvpn_cpp_demo config.ovpn | jq 'select(.type | contains("error"))'

# Only show statistics
./openvpn_cpp_demo config.ovpn | jq 'select(.type == "stats")'

# Only show connection state changes
./openvpn_cpp_demo config.ovpn | jq 'select(.type == "connected" or .type == "disconnected")'
```

### Real-time Dashboard

```bash
# Use watch to create a simple dashboard
watch -n 1 'tail -1 vpn_events.jsonl | jq .'
```

## Troubleshooting

### Common Issues

#### 1. Permission Denied
**Error**: `Cannot create TUN/TAP device: Permission denied`

**Solution**:
```bash
# Run with sudo
sudo ./openvpn_cpp_demo config.ovpn

# Or set capabilities
sudo setcap cap_net_admin+eip ./openvpn_cpp_demo
```

#### 2. Config File Not Found
**Error**: `{"type":"error","message":"Config file not found","file":"..."}`

**Solution**: Ensure the config file path is correct and the file exists.

#### 3. Connection Timeout
**Error**: `{"type":"runtime_error","message":"Connection timeout"}`

**Solution**: 
- Check network connectivity
- Verify server address and port in config
- Check firewall settings

#### 4. Certificate Errors
**Error**: `{"type":"initialization_error","message":"Certificate validation failed"}`

**Solution**:
- Ensure CA, cert, and key files are accessible
- Verify certificate paths in config file
- Check certificate expiration dates

## Advanced Usage

### Background Execution with Systemd

Create a systemd service file `/etc/systemd/system/openvpn-wrapper.service`:

```ini
[Unit]
Description=OpenVPN C++ Wrapper
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
ExecStart=/path/to/openvpn_cpp_demo /path/to/config.ovpn
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable openvpn-wrapper
sudo systemctl start openvpn-wrapper

# View logs
sudo journalctl -u openvpn-wrapper -f
```

### Docker Integration

```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libcap-ng0 \
    libnl-3-200 \
    libnl-genl-3-200 \
    liblzo2-2 \
    liblz4-1

# Copy binary and config
COPY openvpn_cpp_demo /usr/local/bin/
COPY config.ovpn /etc/openvpn/

# Set capabilities
RUN setcap cap_net_admin+eip /usr/local/bin/openvpn_cpp_demo

CMD ["/usr/local/bin/openvpn_cpp_demo", "/etc/openvpn/config.ovpn"]
```

### Metrics Collection with Prometheus

Parse JSON output and expose metrics:

```python
from prometheus_client import start_http_server, Gauge
import subprocess
import json
import time

# Define metrics
bytes_sent = Gauge('vpn_bytes_sent', 'Bytes sent through VPN')
bytes_received = Gauge('vpn_bytes_received', 'Bytes received through VPN')
connection_uptime = Gauge('vpn_uptime_seconds', 'VPN connection uptime')
ping_ms = Gauge('vpn_ping_milliseconds', 'VPN ping latency')

def collect_metrics():
    process = subprocess.Popen(
        ['./openvpn_cpp_demo', 'config.ovpn'],
        stdout=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    
    for line in process.stdout:
        event = json.loads(line.strip())
        if event.get('type') == 'stats':
            bytes_sent.set(event['bytes_sent'])
            bytes_received.set(event['bytes_received'])
            ping_ms.set(event['ping_ms'])
            if 'uptime_seconds' in event:
                connection_uptime.set(event['uptime_seconds'])

if __name__ == '__main__':
    start_http_server(8000)
    collect_metrics()
```

## Architecture

The binary is built on a layered architecture:

```
┌─────────────────────────────────┐
│        main.cpp                 │
│  (JSON output & signal handling)│
└────────────┬────────────────────┘
             │
┌────────────▼────────────────────┐
│    OpenVPNWrapper (C++)         │
│  (High-level API)               │
└────────────┬────────────────────┘
             │
┌────────────▼────────────────────┐
│    C Bridge Layer               │
│  (openvpn_c_bridge)             │
└────────────┬────────────────────┘
             │
┌────────────▼────────────────────┐
│  OpenVPN Core Library (C)       │
│  (lib-src/ - tunnel functions)  │
└─────────────────────────────────┘
```

## Exit Codes

- **0**: Successful execution and clean shutdown
- **1**: Error occurred (config not found, initialization failed, etc.)

## Performance Considerations

- **Status Updates**: Printed every 5 seconds by default
- **Memory Usage**: Approximately 10-20 MB depending on configuration
- **CPU Usage**: Minimal when idle, increases during data transfer
- **Network Overhead**: Standard OpenVPN protocol overhead (varies by cipher)

## Security Recommendations

1. **Never run as root in production** - Use capabilities instead
2. **Secure config files** - Set appropriate permissions (600)
3. **Protect private keys** - Store in secure locations with restricted access
4. **Log rotation** - Implement log rotation for long-running instances
5. **Monitor for errors** - Set up alerting for error events

## Support and Debugging

### Enable Verbose Output

The wrapper respects OpenVPN's verbosity settings in the config file:

```ovpn
verb 4  # More detailed logging
```

### Debug Mode

To see additional debug information, check the OpenVPN log files or increase verbosity:

```bash
# Run with verbose OpenVPN logging
./openvpn_cpp_demo config.ovpn 2>&1 | tee debug.log
```

### Core Dumps

If the application crashes, enable core dumps for debugging:

```bash
ulimit -c unlimited
./openvpn_cpp_demo config.ovpn
```

## License

This wrapper is built on OpenVPN 2.x core library. Please refer to the OpenVPN license for usage terms.

## See Also

- [OpenVPN Documentation](https://openvpn.net/community-resources/)
- [ur-openvpn-library Architecture](../replit.md)
- [C++ API Documentation](API.md)
