
# WireGuard C++ Wrapper Library

A modern C++ wrapper library and CLI application for WireGuard VPN with JSON-based event streaming, real-time statistics, and comprehensive connection management.

## Architecture

### Layers

1. **C Bridge Layer** (`wireguard_c_bridge.c/h`)
   - Interfaces with WireGuard kernel module/userspace implementation
   - Provides C API for configuration and control
   - Thread-safe operations with mutex protection

2. **C++ Wrapper Layer** (`wireguard_wrapper.cpp/hpp`)
   - Object-oriented interface
   - Event callback system
   - JSON serialization
   - Thread-safe statistics tracking

3. **CLI Application** (`main.cpp`)
   - JSON event streaming to stdout
   - Signal handling (SIGINT, SIGTERM)
   - Automatic reconnection
   - Real-time statistics output

## Features

- ✅ File-based and programmatic configuration
- ✅ Real-time connection statistics
- ✅ JSON event streaming
- ✅ Thread-safe operations
- ✅ Callback-based event system
- ✅ Automatic state management
- ✅ Multi-peer support
- ✅ Cross-platform compatible

## Building

```bash
cd ur-wg_library/wireguard-wrapper
mkdir build && cd build
cmake ..
make
```

## Usage

### CLI Application

```bash
./wireguard_cpp_cli /etc/wireguard/wg0.conf
```

### Library Integration

```cpp
#include "wireguard_wrapper.hpp"

wireguard::WireGuardWrapper wg;

wg.setEventCallback([](const wireguard::VPNEvent& event) {
    std::cout << event.type << ": " << event.message << std::endl;
});

wg.initializeFromFile("/etc/wireguard/wg0.conf");
wg.connect();
```

## JSON Event Types

- `startup` - Application initialization
- `initialized` - WireGuard initialized
- `configuring` - Setting up interface/peers
- `handshaking` - Cryptographic handshake
- `connected` - Tunnel established
- `stats` - Periodic statistics
- `status` - Connection status
- `reconnecting` - Reconnection attempt
- `error` - Error occurred
- `shutdown` - Clean shutdown

## API Reference

### WireGuardWrapper Class

#### Methods

- `bool initializeFromFile(const std::string& config_file)`
- `bool initializeFromConfig(...)`
- `bool connect()`
- `bool disconnect()`
- `bool reconnect()`
- `ConnectionState getState() const`
- `VPNStats getStats() const`
- `bool isConnected() const`
- `void setEventCallback(EventCallback callback)`
- `void setStatsCallback(StatsCallback callback)`
- `json getStatusJson() const`
- `json getStatsJson() const`

## License

MIT License - See COPYING file
