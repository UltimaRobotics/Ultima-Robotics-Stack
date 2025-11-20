
# Architecture Overview

## System Design

The ur-mavdiscovery system follows a modular, event-driven architecture with clear separation of concerns.

## Component Hierarchy

```
DeviceManager
    ├── ConfigLoader
    ├── DeviceMonitor
    │   └── ThreadManager (from ur-threadder-api)
    └── DeviceVerifier (per device)
        ├── SerialPort
        ├── MAVLinkParser
        └── ThreadManager
```

## Core Components

### DeviceManager
The central orchestrator that:
- Initializes the configuration system
- Manages the device monitor
- Creates and manages verifier instances
- Handles device add/remove events
- Coordinates thread management

### DeviceMonitor
Monitors udev events for device changes:
- Uses Linux udev library for hardware events
- Filters devices based on configuration
- Runs in its own thread
- Triggers callbacks on device add/remove
- Thread-safe event handling

### DeviceVerifier
Performs MAVLink verification on detected devices:
- One instance per detected device
- Runs in dedicated thread
- Tests baudrates sequentially
- Parses MAVLink packets
- Updates device state database
- Triggers callbacks on verification

### ConfigLoader
Manages application configuration:
- Loads JSON configuration files
- Provides default values
- Validates configuration parameters
- Type-safe configuration access

### DeviceStateDB
Centralized device state storage:
- Singleton pattern
- Thread-safe operations
- Atomic state updates
- Device lifecycle tracking

## Data Flow

```
udev event → DeviceMonitor → DeviceManager → DeviceVerifier → State DB
                                                     ↓
                                           CallbackDispatcher
                                                     ↓
                                              HTTPClient (optional)
```

## Thread Architecture

The system uses the ur-threadder-api for thread management:
- Main thread: Application lifecycle
- Monitor thread: udev event processing
- Verifier threads: One per device being verified

Each thread is registered with a unique identifier for tracking and cleanup.

## Design Principles

1. **Asynchronous Operations**: No blocking in the main event loop
2. **Thread Safety**: Atomic operations and mutex protection
3. **Resource Management**: RAII and proper cleanup
4. **Modularity**: Clear component boundaries
5. **Configurability**: JSON-based runtime configuration
