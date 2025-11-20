# ur-openvpn-library Documentation

Welcome to the ur-openvpn-library documentation. This library provides a modern C++ wrapper around OpenVPN 2.x core functionality with JSON-based event streaming and programmatic control.

## Documentation Index

### Getting Started
- **[Quick Start Guide](QUICKSTART.md)** - Get up and running in 5 minutes
- **[Full Usage Documentation](USAGE.md)** - Comprehensive guide for the openvpn_cpp_demo binary

### Technical Documentation
- **[Project Architecture](../replit.md)** - System architecture and design decisions
- **[API Reference](API.md)** - C++ API documentation (coming soon)

## What's Included

The ur-openvpn-library project provides:

1. **OpenVPN Core Library** (`libopenvpn.a` / `libopenvpn.so`)
   - Static and shared libraries built from OpenVPN 2.x source
   - Includes all core VPN functionality
   - Support for modern crypto (OpenSSL 3.x), compression (LZO, LZ4), and DCO

2. **C Bridge Layer** (`libopenvpn_c_bridge.a`)
   - Isolates C headers from C++ code
   - Provides clean C interface to OpenVPN internals
   - Handles context management and initialization

3. **C++ Wrapper** (`libopenvpn_cpp_wrapper.a`)
   - Modern C++ interface with RAII semantics
   - Event callbacks for connection state changes
   - Statistics and monitoring APIs
   - JSON output support

4. **Demo Application** (`openvpn_cpp_demo`)
   - Ready-to-use VPN client with JSON output
   - Perfect for automation and integration
   - Signal handling and auto-reconnect

## Quick Example

```bash
# Build the project
cd ur-openvpn-library/src
make

# Run VPN client with JSON output
./build/openvpn_cpp_demo config.ovpn | jq '.'
```

## Documentation Quick Links

### For Users
- [How to use the binary](USAGE.md#usage)
- [JSON output format](USAGE.md#json-output-format)
- [Integration examples](USAGE.md#integration-examples)
- [Troubleshooting](USAGE.md#troubleshooting)

### For Developers
- [Architecture overview](../replit.md#system-architecture)
- [Build system](../replit.md#build-system-architecture)
- [C++ wrapper usage](USAGE.md#advanced-usage)
- [Dependencies](../replit.md#external-dependencies)

### For DevOps
- [Systemd integration](USAGE.md#background-execution-with-systemd)
- [Docker deployment](USAGE.md#docker-integration)
- [Metrics collection](USAGE.md#metrics-collection-with-prometheus)
- [Monitoring](USAGE.md#monitoring-and-logging)

## Key Features

### JSON Event Streaming
All events are output as structured JSON for easy parsing:
```json
{"type":"connected","message":"VPN connected","data":{"local_ip":"10.8.0.2"}}
{"type":"stats","bytes_sent":1024,"bytes_received":2048,"ping_ms":45}
```

### Real-time Monitoring
- Connection state tracking
- Network statistics (bytes sent/received, ping latency)
- Automatic reconnection on errors
- Event callbacks for integration

### Standard OpenVPN Compatibility
- Works with standard .ovpn config files
- Supports all OpenVPN connection modes
- Compatible with OpenVPN servers

### Modern Build System
- CMake-based with modular configuration
- Platform detection (Linux, FreeBSD, etc.)
- Optional dependency management
- Nix environment compatible

## Project Structure

```
ur-openvpn-library/
├── src/
│   ├── source-port/              # OpenVPN core library
│   │   ├── lib-src/              # OpenVPN C source
│   │   ├── cmake/                # Build configuration
│   │   └── CMakeLists.txt
│   ├── openvpn_c_bridge.c/h      # C bridge layer
│   ├── openvpn_wrapper.cpp/hpp   # C++ wrapper
│   ├── main.cpp                  # Demo application
│   ├── Makefile                  # Build system
│   └── build/                    # Build output
├── docs/                         # Documentation
│   ├── README.md                 # This file
│   ├── QUICKSTART.md            # Quick start guide
│   ├── USAGE.md                 # Full usage docs
│   └── API.md                   # API reference
└── replit.md                    # Architecture docs
```

## System Requirements

- **OS**: Linux (tested on Ubuntu/Debian/Nix), FreeBSD support
- **Compiler**: GCC 11.4+ or Clang with C++17 support
- **Build**: CMake 3.10+, Make
- **Runtime**: 
  - OpenSSL 3.0.2+
  - libcap-ng (for privilege dropping)
  - libnl-3.0 (for DCO support on Linux)
  - LZO 2.10+ and/or LZ4 1.9.3+ (optional compression)

## Build Artifacts

After building, you'll find:

```
build/
├── source-port/
│   ├── libopenvpn.a              # Static library
│   ├── libopenvpn.so             # Shared library
│   └── openvpn-bin               # OpenVPN CLI (with main)
├── libopenvpn_c_bridge.a         # C bridge library
├── libopenvpn_cpp_wrapper.a      # C++ wrapper library
└── openvpn_cpp_demo              # Demo application ⭐
```

## License

This project builds upon OpenVPN 2.x core library. Please refer to OpenVPN's GPL-2.0 license for terms and conditions.

## Getting Help

- **Quick questions**: See [QUICKSTART.md](QUICKSTART.md)
- **Detailed usage**: See [USAGE.md](USAGE.md)
- **Architecture**: See [replit.md](../replit.md)
- **Build issues**: Check [replit.md - Build System](../replit.md#build-system-architecture)
- **Runtime issues**: See [USAGE.md - Troubleshooting](USAGE.md#troubleshooting)

## Contributing

This project is part of the ur-openvpn-library effort to provide modern programmatic access to OpenVPN functionality. Contributions are welcome!

## Changelog

### Latest Build (Current)
- ✅ Successfully built all libraries and demo application
- ✅ C bridge layer with tunnel entry points
- ✅ C++ wrapper with JSON output support
- ✅ Demo application with event streaming
- ✅ Full documentation suite
