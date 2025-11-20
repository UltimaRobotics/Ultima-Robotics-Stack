
# ur-wg-provider Architecture

## Overview

ur-wg-provider is a reusable WireGuard tunnel management library and CLI tool designed for embedding WireGuard functionality into applications. It's built as a static library with a clean separation between core library functionality and the command-line interface.

## Project Structure

```
ur-wg-provider/
├── src/              # Core library implementation
│   ├── config.c      # Configuration file parsing
│   ├── curve25519.c  # Elliptic curve cryptography
│   ├── encoding.c    # Base64/hex key encoding
│   ├── ipc.c        # Kernel/userspace communication
│   └── terminal.c    # Terminal utilities
├── include/          # Public and private headers
│   ├── linux/       # Linux kernel interface definitions
│   ├── config.h     # Configuration parsing API
│   ├── containers.h # Core data structures
│   ├── encoding.h   # Key encoding API
│   ├── ipc.h        # IPC API
│   └── ...          # Platform-specific headers
├── main.c           # CLI application entry point
├── build/           # Build output directory
│   ├── lib/        # Static library (libwg-provider.a)
│   └── bin/        # Executable (wg-tunnel)
└── CMakeLists.txt   # Build configuration
```

## Architecture Components

### 1. Core Data Structures (`containers.h`)

The library uses a linked-list based structure hierarchy:

```
struct wgdevice (WireGuard Interface)
    ├── Basic properties (name, keys, port, fwmark)
    └── struct wgpeer* (Peer list)
            ├── Peer properties (public key, endpoint, keepalive)
            └── struct wgallowedip* (Allowed IP list)
                    └── IP/CIDR properties
```

**Key Data Structures:**

- **`struct wgdevice`**: Represents a WireGuard network interface
  - Device name and kernel interface index
  - Public/private keys (32 bytes each)
  - Listen port and firewall mark
  - Linked list of peers

- **`struct wgpeer`**: Represents a remote peer
  - Public key and optional preshared key
  - Endpoint address (IPv4/IPv6)
  - Allowed IP ranges
  - Statistics (rx/tx bytes, last handshake time)

- **`struct wgallowedip`**: Represents an allowed IP range
  - IP address (IPv4 or IPv6)
  - CIDR netmask
  - Flags for operations

### 2. Configuration Layer (`config.c`, `config.h`)

**Purpose**: Parse WireGuard INI-style configuration files into data structures.

**Architecture Pattern**: State machine parser

**Key Components:**

- **`struct config_ctx`**: Parser context maintaining state
  - Current device being configured
  - Current section type (Interface/Peer)
  - Tracking of current peer and allowed IP

**Parsing Flow:**
```
File → Line-by-line processing → Section detection → Field parsing → Structure population
```

**Supported Configuration Fields:**

*Interface Section:*
- `PrivateKey` - Device private key (base64)
- `ListenPort` - UDP port for incoming connections
- `FwMark` - Firewall mark for policy routing
- `Address`, `DNS`, `MTU`, etc. - Silently ignored (wg-quick specific)

*Peer Section:*
- `PublicKey` - Peer identification (base64)
- `PresharedKey` - Optional symmetric key (base64)
- `Endpoint` - Peer address (IP:port)
- `AllowedIPs` - Comma-separated CIDR ranges
- `PersistentKeepalive` - NAT keepalive interval

**Error Handling:**
- Line-by-line validation with descriptive error messages
- Double-free protection during parse failures
- Setting `ctx->device = NULL` on errors to prevent cleanup issues

### 3. Cryptographic Layer (`curve25519.c`, `encoding.c`)

**Purpose**: Provide cryptographic operations and key encoding/decoding.

**Curve25519 Operations:**

Uses formal verification implementations:
- **32-bit systems**: `curve25519-fiat32.h` (Fiat Crypto)
- **64-bit systems**: `curve25519-hacl64.h` (HACL*)

**Key Operations:**
```c
void curve25519_generate_public(uint8_t pub[32], const uint8_t secret[32]);
void curve25519(uint8_t out[32], const uint8_t secret[32], const uint8_t basepoint[32]);
```

**Encoding Operations:**

Base64 encoding for human-readable keys:
```c
void key_to_base64(char base64[45], const uint8_t key[32]);
bool key_from_base64(uint8_t key[32], const char *base64);
```

Hex encoding for debugging:
```c
void key_to_hex(char hex[65], const uint8_t key[32]);
bool key_from_hex(uint8_t key[32], const char *hex);
```

**Security Features:**
- Constant-time operations to resist side-channel attacks
- Proper key validation
- Zero-check for all-zero keys

### 4. IPC Layer (`ipc.c`)

**Purpose**: Abstract kernel/userspace communication across platforms.

**Architecture Pattern**: Platform abstraction with compile-time selection

**Platform Support:**

- **Linux**: Generic Netlink interface (`ipc-linux.h`)
- **FreeBSD**: nvlist-based ioctl (`ipc-freebsd.h`)
- **OpenBSD**: Direct ioctl (`ipc-openbsd.h`)
- **Userspace**: Unix socket communication (`ipc-uapi-unix.h`)

**Core API:**

```c
// Configure WireGuard device
int ipc_set_device(struct wgdevice *dev);

// Retrieve device configuration
int ipc_get_device(struct wgdevice **dev, const char *interface);

// List all WireGuard interfaces
char *ipc_list_devices(void);
```

**Communication Flow (Linux):**

```
Application → ipc_set_device() → Netlink socket → Kernel WireGuard module
                                                         ↓
                                                    Device configured
```

**Error Handling:**
- Returns negative errno values on failure
- Sets global errno for standard error reporting
- Proper cleanup of allocated resources

### 5. CLI Application (`main.c`)

**Purpose**: Command-line interface for tunnel management.

**Architecture**: Simple procedural design

**Workflow:**

1. **Parse Arguments**: Config file path and optional interface name
2. **Read Configuration**: Parse config file into `struct wgdevice`
3. **Create Interface**: Execute `ip link add` command
4. **Configure Device**: Apply configuration via IPC
5. **Bring Up Interface**: Execute `ip link set up`
6. **Display Status**: Show configuration summary
7. **Cleanup**: Free allocated memory

**Example Usage:**
```bash
# Start tunnel with default interface name (wg0)
sudo ./wg-tunnel /etc/wireguard/wg0.conf

# Start tunnel with custom interface name
sudo ./wg-tunnel /etc/wireguard/client.conf wg-client
```

## Build System

**CMake Configuration:**

- **Static Library Target**: `libwg-provider.a`
  - All source files in `src/`
  - Includes all headers from `include/`
  - Compiler flags: `-Wall -Wold-style-definition -Wstrict-prototypes`
  - Defines: `_GNU_SOURCE`, `RUNSTATEDIR="/var/run"`

- **Executable Target**: `wg-tunnel`
  - Links against static library
  - Single source: `main.c`

**Build Commands:**
```bash
cd ur-wg-provider
mkdir -p build && cd build
cmake ..
make
```

## Memory Management

**Allocation Strategy:**

- **Linked Lists**: Dynamic allocation for variable-length peer/IP lists
- **Device Structure**: Caller owns, must free with `free_wgdevice()`
- **String Buffers**: Fixed-size arrays or caller-managed buffers

**Cleanup Pattern:**
```c
struct wgdevice *dev = read_config_file("wg0.conf");
if (!dev) {
    // Error handling
    return -1;
}

// Use device...

// Cleanup
free_wgdevice(dev);  // Recursively frees all peers and allowed IPs
```

**Safety Features:**
- NULL pointer checks before freeing
- Setting pointers to NULL after free
- Proper unwinding on parse errors

## Threading Considerations

**Current State**: Not thread-safe

**Implications:**
- One operation at a time per device
- External synchronization required for concurrent access
- No global state (except temporary buffers in IPC layer)

## Error Handling Philosophy

**Return Value Convention:**
- `0` on success
- Negative errno values on failure
- Sets `errno` for standard error reporting

**Validation Levels:**
1. **Syntax Validation**: Config file format
2. **Semantic Validation**: Key lengths, IP formats
3. **Runtime Validation**: Interface existence, permissions

## Extension Points

**Adding New Platforms:**

1. Create `ipc-<platform>.h` header
2. Implement platform-specific `kernel_get_device()` and `kernel_set_device()`
3. Update `ipc.c` with platform detection

**Adding Configuration Fields:**

1. Update parser in `config.c` to recognize new field
2. Add field to appropriate structure in `containers.h`
3. Set corresponding flag bit

## Dependencies

**Required:**
- C11 compiler
- CMake 3.10+
- POSIX system (Linux, BSD, macOS)
- Root privileges (for network interface operations)

**Optional:**
- WireGuard kernel module (Linux)
- WireGuard userspace implementation (other platforms)

## Security Considerations

1. **Privilege Requirements**: Requires `CAP_NET_ADMIN` or root
2. **Key Handling**: Keys cleared from memory with `memzero_explicit()`
3. **Input Validation**: All config input validated before use
4. **Constant-Time Crypto**: Curve25519 implementation resists timing attacks
