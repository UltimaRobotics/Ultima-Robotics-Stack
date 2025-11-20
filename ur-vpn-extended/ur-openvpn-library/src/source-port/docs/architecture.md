
# Code Architecture Documentation

## Overview

The ur-openvpn-library extends OpenVPN 2.x with modern API interfaces for client and server management, providing programmatic control over VPN operations.

## High-Level Architecture

```
┌─────────────────────────────────────────────┐
│         Application Layer                    │
│  (Client/Server API Integration)            │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│         API Layer                            │
│  ┌─────────────────┐  ┌──────────────────┐ │
│  │  Client API     │  │  Server API      │ │
│  │  (openvpn_      │  │  (openvpn_       │ │
│  │   client_api.c) │  │   server_api.c)  │ │
│  └─────────────────┘  └──────────────────┘ │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│         OpenVPN Core Library                 │
│  ┌──────────┐ ┌──────────┐ ┌─────────────┐ │
│  │ Network  │ │  Crypto  │ │   Protocol  │ │
│  │ Layer    │ │  Layer   │ │   Layer     │ │
│  └──────────┘ └──────────┘ └─────────────┘ │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│         Platform Layer                       │
│  (TUN/TAP, Socket, OS-specific)             │
└─────────────────────────────────────────────┘
```

## Core Components

### 1. Initialization System (init.c/init.h)

**Purpose**: Manages OpenVPN context initialization and lifecycle

**Key Functions**:
- `init_static()` - Initialize program-wide statics
- `context_init_1()` - Phase 1 initialization
- `context_init_2()` - Phase 2 initialization (network setup)
- `do_init_crypto()` - Initialize cryptography
- `do_init_tun()` - Initialize TUN/TAP device

**Context Structure**:
```c
struct context {
    struct context_0 *c0;     // Persistent across SIGUSR1
    struct context_1 c1;      // Reset on SIGUSR1
    struct context_2 c2;      // Reset on SIGUSR1
    struct gc_arena gc;       // Garbage collection
    struct signal_info *sig;  // Signal handling
    struct options options;   // Configuration
    bool first_time;         // First iteration flag
};
```

### 2. Network Layer

#### Socket Management (socket.c/socket.h)
- TCP/UDP socket operations
- Link-level protocol handling
- Connection management
- Address resolution

#### TUN/TAP Interface (tun.c/tun.h)
- Virtual network device management
- Platform-specific implementations
- IP configuration
- Routing setup

#### Protocol Handler (proto.c/proto.h)
- OpenVPN protocol implementation
- Packet framing
- Reliability layer
- Connection state machine

### 3. Cryptography Layer

#### Core Crypto (crypto.c/crypto.h)
- Cipher operations (AES, BF, etc.)
- HMAC authentication
- Packet encryption/decryption
- Key management

#### SSL/TLS (ssl.c/ssl.h)
- TLS handshake
- Certificate handling
- Session management
- Key exchange

#### TLS Crypt (tls_crypt.c/tls_crypt.h)
- TLS control channel encryption
- TLS-Crypt v2 support
- Metadata handling

### 4. Client API Layer

**Location**: `apis/openvpn_client_api.c`

**Architecture**:
```c
// Session Management
┌──────────────────────────────┐
│  Session Array (64 max)      │
│  ┌────────────────────────┐  │
│  │ Session 1              │  │
│  │  - Config              │  │
│  │  - State               │  │
│  │  - Stats               │  │
│  │  - Worker Thread       │  │
│  │  - Event Queue         │  │
│  └────────────────────────┘  │
└──────────────────────────────┘
```

**Key Components**:

1. **Session Context** (`ovpn_client_session_t`)
   - Configuration storage
   - Connection state
   - Statistics tracking
   - Event handling
   - Threading support

2. **Worker Thread** (`client_worker_thread()`)
   - State machine execution
   - Connection monitoring
   - Automatic reconnection
   - Statistics updates

3. **Event System**
   - Event queue (256 events max)
   - Callback mechanism
   - Thread-safe operations

**State Machine**:
```
INITIAL → CONNECTING → AUTH → GET_CONFIG → 
ASSIGN_IP → ADD_ROUTES → CONNECTED
                ↓
              ERROR/DISCONNECTED → RECONNECTING (if enabled)
```

### 5. Server API Layer

**Location**: `apis/openvpn_server_api.c`

**Architecture**:
```c
┌──────────────────────────────────────┐
│  Server Context                      │
│  ┌────────────────────────────────┐  │
│  │ Configuration                  │  │
│  │ Client Array (1000 max)        │  │
│  │ OpenVPN Context                │  │
│  │ Management Interface           │  │
│  │ Statistics                     │  │
│  │ Event Callback                 │  │
│  └────────────────────────────────┘  │
└──────────────────────────────────────┘
```

**Key Features**:
1. Client certificate management
2. Dynamic client configuration generation
3. Session monitoring
4. Event-driven architecture
5. Multi-threaded server operation

## Data Flow

### Client Connection Flow

```
1. Configuration → Parse JSON
                 ↓
2. Create Session → Allocate resources
                 ↓
3. Connect → Start worker thread
                 ↓
4. Authentication → TLS handshake
                 ↓
5. IP Assignment → Receive network config
                 ↓
6. Route Setup → Configure routing
                 ↓
7. Connected → Data transfer
```

### Packet Flow

```
Application Data
      ↓
[Compression] (optional)
      ↓
[Encryption] → HMAC → Packet ID
      ↓
[Framing] → Protocol Header
      ↓
Network (UDP/TCP)
```

## Threading Model

### Client API
- **Main Thread**: API calls, session management
- **Worker Threads**: One per session
  - Connection state machine
  - Network I/O
  - Event generation

**Synchronization**:
- Mutex per session (`state_mutex`)
- Event queue mutex (`event_mutex`)
- Global sessions mutex (`g_sessions_mutex`)

### Server API
- **Main Thread**: API calls, client management
- **Server Thread**: OpenVPN main loop
- **Monitoring Thread**: Statistics updates

## Memory Management

### Garbage Collection (gc_arena)

The library uses a custom garbage collection system:

```c
struct gc_arena gc = gc_new();
char *data = gc_malloc(size, false, &gc);
// Use data...
gc_free(&gc);  // Frees all allocations
```

**Benefits**:
- Automatic cleanup
- Exception safety
- Reduced memory leaks

### Buffer Management

```c
struct buffer {
    int capacity;
    int offset;
    int len;
    uint8_t *data;
};
```

- Pre-allocated buffers
- Headroom/tailroom support
- Alignment handling

## Configuration System

### Options Structure

```c
struct options {
    // Connection
    struct connection_entry ce;
    struct connection_list *connection_list;
    
    // Security
    const char *ciphername;
    const char *authname;
    struct tls_options tls_opts;
    
    // Network
    const char *dev;
    const char *dev_type;
    const char *ifconfig_local;
    
    // Behavior
    int verbosity;
    bool persist_tun;
    bool pull;
};
```

### Configuration Flow

```
1. Parse config file (.ovpn)
         ↓
2. Set options structure
         ↓
3. Validate options
         ↓
4. Apply to context
         ↓
5. Initialize subsystems
```

## Error Handling

### Error Categories

1. **Fatal Errors** (`M_FATAL`)
   - Program exits
   - Logged to syslog/file

2. **Non-Fatal Errors** (`M_NONFATAL`)
   - Retry possible
   - Connection recoverable

3. **Warnings** (`M_WARN`)
   - Informational
   - No action required

### Error Propagation

```c
// Example error handling
if (status < 0) {
    msg(M_FATAL | M_ERRNO, "Socket creation failed");
}
```

## Event System

### Client Events

```c
typedef enum {
    CLIENT_EVENT_STATE_CHANGE,
    CLIENT_EVENT_LOG_MESSAGE,
    CLIENT_EVENT_STATS_UPDATE,
    CLIENT_EVENT_ERROR,
    CLIENT_EVENT_AUTH_REQUIRED,
    CLIENT_EVENT_RECONNECT,
    CLIENT_EVENT_LATENCY_UPDATE,
    CLIENT_EVENT_QUALITY_UPDATE,
    CLIENT_EVENT_BYTES_COUNT,
    CLIENT_EVENT_ROUTE_UPDATE
} ovpn_client_event_type_t;
```

### Event Queue

- Fixed-size circular buffer (256 events)
- Thread-safe operations
- Callback-based delivery

## Platform Abstractions

### Windows
- `win32.c/win32.h` - Windows-specific operations
- `wfp_block.c` - Windows Filtering Platform
- TAP-Windows adapter support

### Linux
- `dco_linux.c` - Data Channel Offload
- `networking_sitnl.c` - Netlink operations
- TUN/TAP native support

### Cross-Platform
- `platform.c/platform.h` - Platform abstraction
- Conditional compilation (`#ifdef _WIN32`)

## Extension Points

### 1. Custom Protocol Support
- Extend `proto.c` for new protocols
- Add protocol-specific handlers

### 2. New Cipher Support
- Add cipher in `crypto_openssl.c`
- Update cipher table

### 3. Platform Support
- Implement platform-specific TUN/TAP
- Add to `platform_detection.cmake`

### 4. Management Interface
- Extend `manage.c` for new commands
- Add API wrapper functions

## Code Organization Best Practices

1. **Module Separation**: Each .c file handles specific functionality
2. **Header Guards**: All headers use include guards
3. **Static Functions**: Internal functions marked static
4. **Error Checking**: All system calls checked
5. **Documentation**: Function-level comments
6. **Const Correctness**: Const pointers where appropriate

## Performance Considerations

1. **Buffer Reuse**: Buffers allocated once, reused
2. **Zero-Copy**: Minimize data copying
3. **Threading**: Parallel processing where beneficial
4. **Caching**: DNS, route, certificate caching
5. **Polling**: Efficient event polling (epoll/kqueue)
