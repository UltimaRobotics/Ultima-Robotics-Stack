# Quick Start Guide - openvpn_cpp_demo

## 5-Minute Setup

### 1. Build the Binary

```bash
cd ur-openvpn-library/src
make
```

The binary will be available at: `build/openvpn_cpp_demo`

### 2. Prepare Your Config File

Create or use an existing OpenVPN config file (`.ovpn`):

```ovpn
client
dev tun
proto udp
remote your-vpn-server.com 1194
ca ca.crt
cert client.crt
key client.key
cipher AES-256-CBC
```

### 3. Run the VPN Client

```bash
# Set network capabilities (one time)
sudo setcap cap_net_admin+eip build/openvpn_cpp_demo

# Run the client
./build/openvpn_cpp_demo /path/to/your/config.ovpn
```

### 4. Monitor the JSON Output

All events are JSON-formatted on stdout:

```bash
# Save to file
./build/openvpn_cpp_demo config.ovpn > events.jsonl

# Filter with jq
./build/openvpn_cpp_demo config.ovpn | jq 'select(.type == "stats")'
```

## Common Commands

### Basic Usage
```bash
# Run VPN client
./openvpn_cpp_demo config.ovpn

# Run in background and save logs
./openvpn_cpp_demo config.ovpn > vpn.log 2>&1 &

# Stop VPN (send SIGTERM)
kill -TERM <pid>
```

### Monitoring
```bash
# Real-time connection stats
./openvpn_cpp_demo config.ovpn | jq 'select(.type == "stats")'

# Watch for errors
./openvpn_cpp_demo config.ovpn | jq 'select(.type | contains("error"))'

# Monitor connection state
./openvpn_cpp_demo config.ovpn | jq '.type, .message'
```

### Integration Examples

**Shell Script**:
```bash
#!/bin/bash
./openvpn_cpp_demo config.ovpn | while read -r line; do
    echo "$line" | jq -r '"\(.type): \(.message)"'
done
```

**Python**:
```python
import subprocess, json

proc = subprocess.Popen(['./openvpn_cpp_demo', 'config.ovpn'], 
                        stdout=subprocess.PIPE, text=True)
for line in proc.stdout:
    event = json.loads(line)
    print(f"{event['type']}: {event.get('message', '')}")
```

## JSON Event Types

| Event Type | Description | When Emitted |
|------------|-------------|--------------|
| `startup` | Application started | Once at start |
| `connecting` | Connection initiated | During connection |
| `connected` | VPN connected | On successful connection |
| `status` | Connection status | Every 5 seconds |
| `stats` | Network statistics | Periodically when connected |
| `error` | Error occurred | On errors |
| `reconnecting` | Attempting reconnect | After connection failure |
| `signal` | Signal received | On SIGINT/SIGTERM |
| `shutdown` | Clean shutdown | On exit |

## Troubleshooting

### Permission Denied
```bash
# Solution: Add network admin capability
sudo setcap cap_net_admin+eip ./openvpn_cpp_demo
```

### Config File Not Found
```bash
# Solution: Use absolute path
./openvpn_cpp_demo /absolute/path/to/config.ovpn
```

### Connection Timeout
```bash
# Check server connectivity first
ping your-vpn-server.com

# Verify config file
cat config.ovpn | grep -E "remote|proto|port"
```

## Next Steps

- Read the [full documentation](USAGE.md) for detailed usage
- Check [API documentation](API.md) for C++ integration
- Review [architecture docs](../replit.md) for technical details

## Example Output

**Successful Connection**:
```json
{"type":"startup","message":"OpenVPN wrapper starting","config_file":"config.ovpn","pid":12345}
{"type":"connecting","message":"Initiating connection","state":"CONNECTING"}
{"type":"connected","message":"VPN connected","data":{"local_ip":"10.8.0.2"}}
{"type":"status","state":"CONNECTED","connected":true,"local_ip":"10.8.0.2"}
{"type":"stats","bytes_sent":1024,"bytes_received":2048,"ping_ms":45}
```

**On Error**:
```json
{"type":"error","message":"Config file not found","file":"missing.ovpn"}
```

**On Shutdown** (Ctrl+C):
```json
{"type":"signal","signal":2,"message":"Received signal, shutting down..."}
{"type":"shutdown","message":"OpenVPN wrapper stopped successfully"}
```
