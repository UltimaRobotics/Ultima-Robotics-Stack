
# ur-mavdiscovery Documentation

## Overview

The ur-mavdiscovery project is a robust C++ system for discovering and verifying MAVLink devices on serial ports using udev monitoring. It provides real-time device detection, multi-threaded verification, and configurable MAVLink protocol support.

## Documentation Structure

- [Architecture Overview](architecture.md) - System design and component relationships
- [Device Discovery Mechanism](device-discovery.md) - How devices are detected via udev
- [Verification Process](verification-process.md) - MAVLink device verification workflow
- [Thread Management](thread-management.md) - Multi-threaded architecture using ur-threadder-api
- [Configuration System](configuration.md) - JSON-based configuration and defaults
- [State Management](state-management.md) - Device state database and tracking
- [Callback System](callback-system.md) - Event notification and dispatching
- [HTTP Integration](http-integration.md) - Remote notification capabilities
- [Serial Communication](serial-communication.md) - Serial port handling and configuration
- [MAVLink Parsing](mavlink-parsing.md) - Protocol parsing and message handling
- [Logging System](logging.md) - Application logging infrastructure

## Quick Start

```bash
# Build the project
cd ur-mavdiscovery
mkdir build && cd build
cmake ..
make

# Run with default configuration
./mavdiscovery

# Run with custom configuration
./mavdiscovery ../config.json
```

## Key Features

- **Real-time monitoring** using Linux udev events
- **Multi-threaded verification** - one thread per device
- **Sequential baudrate testing** with configurable rates
- **MAVLink v1 and v2 support** using c_library_v2
- **Thread-safe state management** with atomic operations
- **HTTP integration** for remote notifications
- **Zero-copy operations** where possible
