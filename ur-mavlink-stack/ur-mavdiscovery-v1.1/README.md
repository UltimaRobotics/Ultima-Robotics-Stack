
# MAVLink Device Discovery

A robust C++ system for discovering and verifying MAVLink devices on serial ports using udev monitoring.

## Features

- **Real-time device monitoring** using udev events
- **Multi-threaded verification** - one thread per device
- **Sequential baudrate testing** with configurable rates
- **MAVLink v1 and v2 support** using c_library_v2
- **Thread-safe state management** with atomic operations
- **HTTP integration** for remote notifications
- **Configurable filtering** for device paths
- **No queues or blocking operations** - fully asynchronous

## Build

```bash
cd mavdiscovery
mkdir build && cd build
cmake ..
make
```

## Dependencies

- libudev
- libcurl
- C++17 compiler
- CMake 3.10+

## Usage

```bash
# Run with default configuration
./mavdiscovery

# Run with custom config file
./mavdiscovery config.json
```

## Configuration

Edit `config.json` to customize:

- **devicePathFilters**: Device path prefixes to monitor
- **baudrates**: List of baudrates to test (in order)
- **readTimeoutMs**: Serial read timeout
- **packetTimeoutMs**: Maximum time to wait for valid packet per baudrate
- **enableHTTP**: Enable HTTP notifications
- **httpEndpoint**: URL for device info POST requests
- **logLevel**: DEBUG, INFO, WARNING, or ERROR

## Architecture

The system follows the flowchart design:
1. udev monitor detects device add/remove events
2. Devices matching filters spawn verification threads
3. Each thread sequentially tests baudrates
4. Valid MAVLink packets trigger device verification
5. Callbacks and HTTP notifications are dispatched
6. Thread-safe state DB maintains device information
