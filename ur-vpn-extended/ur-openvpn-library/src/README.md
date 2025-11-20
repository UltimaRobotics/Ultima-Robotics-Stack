
# OpenVPN C++ Wrapper

A modern C++ wrapper for OpenVPN with JSON-based status reporting and statistics collection.

## Features

- Clean C++ interface to OpenVPN core functionality
- Real-time JSON event streaming
- Connection statistics monitoring
- Error handling and reporting
- Automatic reconnection support

## Building

```bash
cd ur-openvpn-library/src
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
./openvpn_cpp /path/to/config.ovpn
```

The wrapper outputs JSON events to stdout:

### Event Types

- `startup` - Wrapper initialization
- `initialized` - OpenVPN initialized successfully
- `connecting` - Connection attempt started
- `authenticating` - Authentication in progress
- `connected` - Successfully connected
- `stats` - Periodic statistics update
- `status` - Connection status update
- `error` - Runtime error occurred
- `disconnecting` - Disconnection started
- `disconnected` - Connection closed
- `shutdown` - Wrapper shutdown

### Example Output

```json
{"type":"startup","message":"OpenVPN wrapper starting","config_file":"config.ovpn","pid":12345}
{"type":"initialized","message":"OpenVPN wrapper initialized successfully"}
{"type":"connecting","message":"Starting VPN connection"}
{"type":"authenticating","message":"Authenticating with VPN server"}
{"type":"connected","message":"VPN connection established successfully"}
{"type":"stats","bytes_sent":1024,"bytes_received":2048,"ping_ms":25,"local_ip":"10.8.0.2"}
```

## Integration Example

```cpp
#include "openvpn_wrapper.hpp"

using namespace openvpn;

int main() {
    OpenVPNWrapper vpn;
    
    // Set event callback
    vpn.setEventCallback([](const VPNEvent& event) {
        std::cout << "Event: " << event.type << " - " << event.message << std::endl;
    });
    
    // Initialize and connect
    vpn.initialize("config.ovpn");
    vpn.connect();
    
    // Get status
    auto status = vpn.getStatusJson();
    auto stats = vpn.getStatsJson();
    
    return 0;
}
```

## Statistics Collection

The wrapper automatically collects and reports:
- Bytes sent/received
- Packets sent/received
- Connection uptime
- Ping latency
- IP addresses (local, remote, server)

## Error Handling

All errors are reported as JSON events with type "error" or "runtime_error":

```json
{"type":"error","message":"Authentication failed","timestamp":1234567890}
```
