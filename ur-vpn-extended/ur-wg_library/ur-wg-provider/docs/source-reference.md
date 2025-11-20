
# Source Files Reference

## Core Library Sources (`src/`)

### config.c

**Purpose**: Parse WireGuard configuration files into device structures.

**Key Functions:**

```c
bool config_read_init(struct config_ctx *ctx, bool append);
```
- Initializes parser context
- Allocates new `wgdevice` structure
- Sets appropriate flags for replace/append mode
- Returns: `true` on success, `false` on allocation failure

```c
bool config_read_line(struct config_ctx *ctx, const char *line);
```
- Processes a single configuration line
- Handles section headers `[Interface]` and `[Peer]`
- Parses key=value pairs
- Updates parser state
- Returns: `true` on success, `false` on parse error

```c
struct wgdevice *config_read_finish(struct config_ctx *ctx);
```
- Finalizes parsing and validates configuration
- Ensures all peers have public keys
- Returns: Pointer to device structure or NULL on validation failure

**Internal Functions:**

- `get_value()` - Extracts value from key=value line
- `parse_port()` - Validates and converts port numbers
- `parse_fwmark()` - Parses firewall mark (supports hex/decimal)
- `parse_key()` - Decodes base64 keys
- `parse_keyfile()` - Reads keys from files
- `parse_ip()` - Parses IP addresses (IPv4/IPv6)
- `parse_endpoint()` - Parses endpoint addresses with DNS resolution
- `parse_allowedips()` - Parses comma-separated CIDR ranges
- `parse_persistent_keepalive()` - Validates keepalive interval
- `process_line()` - State machine for line processing
- `validate_netmask()` - Ensures CIDR ranges have zero host bits

**Error Handling:**
- All parse errors print descriptive messages to stderr
- Cleanup performed via `free_wgdevice()` on error
- Sets `ctx->device = NULL` to prevent double-free

**Special Features:**
- DNS resolution with retry logic (respects `WG_ENDPOINT_RESOLUTION_RETRIES`)
- Incremental allowed IP updates (prefix `+` or `-`)
- Silently ignores wg-quick specific fields (Address, DNS, MTU, etc.)

---

### curve25519.c

**Purpose**: Elliptic curve Diffie-Hellman key exchange operations.

**Key Functions:**

```c
void curve25519_generate_public(uint8_t pub[32], const uint8_t secret[32]);
```
- Derives public key from private key
- Uses standard Curve25519 basepoint
- Essential for generating keypairs

```c
void curve25519(uint8_t out[32], const uint8_t secret[32], const uint8_t basepoint[32]);
```
- General-purpose Curve25519 scalar multiplication
- Used for key agreement operations
- Constant-time implementation

**Implementation Details:**

**Architecture-Specific Code Paths:**
- Detects `__SIZEOF_INT128__` at compile time
- **64-bit**: Uses HACL* implementation (`curve25519-hacl64.h`)
  - Optimized for 64-bit registers
  - Uses 128-bit integer types
  - Approximately 2x faster on modern CPUs

- **32-bit**: Uses Fiat Crypto implementation (`curve25519-fiat32.h`)
  - Machine-verified correctness
  - Portable to all 32-bit platforms
  - No 128-bit integer requirement

**Security Properties:**
- Formally verified implementations from MIT/INRIA
- Constant-time operations (no secret-dependent branches)
- Resistant to timing side-channel attacks
- Clamps secret keys to valid Curve25519 scalars

**Memory Safety:**
- Uses `memzero_explicit()` to clear sensitive data
- No dynamic allocations
- Stack-only operations

---

### encoding.c

**Purpose**: Convert cryptographic keys between binary and text formats.

**Key Functions:**

```c
void key_to_base64(char base64[45], const uint8_t key[32]);
```
- Encodes 32-byte binary key to 44-character base64 string
- Adds padding and null terminator
- Used for human-readable key representation

```c
bool key_from_base64(uint8_t key[32], const char *base64);
```
- Decodes base64 string to 32-byte binary key
- Validates input length (must be 44 chars + null)
- Returns: `true` on success, `false` on invalid input

```c
void key_to_hex(char hex[65], const uint8_t key[32]);
```
- Encodes key to 64-character lowercase hex string
- Used for debugging and alternative key formats

```c
bool key_from_hex(uint8_t key[32], const char *hex);
```
- Decodes hex string to binary key
- Accepts both uppercase and lowercase
- Returns: `true` on success, `false` on invalid input

```c
bool key_is_zero(const uint8_t key[32]);
```
- Checks if key is all zeros
- Constant-time implementation
- Used to validate key presence

**Implementation Details:**

**Constant-Time Design:**
- All operations resistant to timing attacks
- No early exits based on secret data
- Careful bit manipulation to avoid branches

**Base64 Alphabet:**
- Standard RFC 4648 alphabet: `A-Za-z0-9+/`
- Uses padding character `=`
- WireGuard-specific 44-character format

**Hex Encoding:**
- Uses lowercase a-f by default
- Case-insensitive decoding
- Standard two-character-per-byte format

---

### ipc.c

**Purpose**: Abstract platform-specific kernel communication.

**Key Functions:**

```c
int ipc_set_device(struct wgdevice *dev);
```
- Applies configuration to WireGuard device
- Routes to platform-specific implementation
- Returns: 0 on success, negative errno on failure

```c
int ipc_get_device(struct wgdevice **dev, const char *iface);
```
- Retrieves current device configuration
- Allocates new `wgdevice` structure
- Caller must free with `free_wgdevice()`
- Returns: 0 on success, negative errno on failure

```c
char *ipc_list_devices(void);
```
- Lists all WireGuard interfaces
- Returns: Null-delimited string list (e.g., "wg0\0wg1\0\0")
- Caller must free returned pointer

**Internal Architecture:**

**Platform Selection:**
```c
#ifdef IPC_SUPPORTS_KERNEL_INTERFACE
    // Kernel interface available (Linux, BSD)
    if (userspace_has_wireguard_interface(iface))
        return userspace_get_device(dev, iface);
    return kernel_get_device(dev, iface);
#else
    // Userspace-only (fallback)
    return userspace_get_device(dev, iface);
#endif
```

**String List Management:**
```c
struct string_list {
    char *buffer;    // Dynamically growing buffer
    size_t len;      // Current length
    size_t cap;      // Allocated capacity
};
```
- Automatic buffer growth (doubles on overflow)
- Null-delimited strings
- Final double-null terminator

**Platform Implementations:**

1. **Linux** (`ipc-linux.h`)
   - Uses Generic Netlink
   - Commands: `WG_CMD_GET_DEVICE`, `WG_CMD_SET_DEVICE`
   - Socket buffer management
   - Message fragmentation for large peer lists

2. **FreeBSD** (`ipc-freebsd.h`)
   - Uses nvlist (name-value list) serialization
   - ioctl-based communication
   - Device path: `/dev/wg*`

3. **OpenBSD** (`ipc-openbsd.h`)
   - Direct ioctl interface
   - Kernel structures match userspace
   - Uses `SIOCGWG`/`SIOCSWG` ioctls

4. **Userspace** (`ipc-uapi-unix.h`)
   - Unix socket communication
   - Socket path: `/var/run/wireguard/<interface>.sock`
   - Text protocol (get=1/set=1 commands)

---

### terminal.c

**Purpose**: Terminal output utilities with color support.

**Key Functions:**

```c
void terminal_printf(const char *fmt, ...);
```
- Printf-like function with ANSI color filtering
- Strips ANSI codes when output is not a TTY
- Respects `WG_COLOR_MODE` environment variable

**Internal Functions:**

```c
static bool color_mode(void);
```
- Determines if color output is appropriate
- Checks environment variable `WG_COLOR_MODE`:
  - `"always"` - Force color output
  - `"never"` - Strip all color codes
  - Unset - Auto-detect based on `isatty(stdout)`

```c
static void filter_ansi(const char *fmt, va_list args);
```
- Processes format string and arguments
- Strips ANSI escape sequences when colors disabled
- Uses `vasprintf()` for dynamic buffer allocation

**ANSI Code Handling:**
- Detects sequences: `\x1b[` followed by control characters
- Removes entire escape sequence including parameters
- Preserves text content

**Usage Example:**
```c
terminal_printf(TERMINAL_FG_GREEN "Success" TERMINAL_RESET "\n");
// Output: "Success" in green if terminal supports it
```

---

## Header Files (`include/`)

### containers.h

**Purpose**: Core data structure definitions for WireGuard configuration.

**Key Structures:**

```c
struct wgdevice {
    char name[IFNAMSIZ];           // Interface name (e.g., "wg0")
    uint32_t ifindex;               // Kernel interface index
    uint32_t flags;                 // WGDEVICE_* flags
    uint8_t public_key[32];        // Device public key
    uint8_t private_key[32];       // Device private key
    uint32_t fwmark;                // Firewall mark
    uint16_t listen_port;           // UDP listening port
    struct wgpeer *first_peer;     // Peer linked list head
    struct wgpeer *last_peer;      // Peer linked list tail
};
```

**Flags:**
- `WGDEVICE_REPLACE_PEERS` - Replace all existing peers
- `WGDEVICE_HAS_PRIVATE_KEY` - Private key is set
- `WGDEVICE_HAS_PUBLIC_KEY` - Public key is set
- `WGDEVICE_HAS_LISTEN_PORT` - Listen port is set
- `WGDEVICE_HAS_FWMARK` - Firewall mark is set

```c
struct wgpeer {
    uint32_t flags;                           // WGPEER_* flags
    uint8_t public_key[32];                  // Peer public key
    uint8_t preshared_key[32];               // Optional PSK
    union {
        struct sockaddr addr;
        struct sockaddr_in addr4;
        struct sockaddr_in6 addr6;
    } endpoint;                               // Peer endpoint
    struct timespec64 last_handshake_time;   // Last successful handshake
    uint64_t rx_bytes, tx_bytes;             // Transfer statistics
    uint16_t persistent_keepalive_interval;  // Keepalive seconds
    struct wgallowedip *first_allowedip;    // Allowed IP list head
    struct wgallowedip *last_allowedip;     // Allowed IP list tail
    struct wgpeer *next_peer;                // Next peer in list
};
```

**Peer Flags:**
- `WGPEER_REMOVE_ME` - Remove this peer
- `WGPEER_REPLACE_ALLOWEDIPS` - Replace peer's allowed IPs
- `WGPEER_HAS_PUBLIC_KEY` - Public key is set
- `WGPEER_HAS_PRESHARED_KEY` - Preshared key is set
- `WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL` - Keepalive is set

```c
struct wgallowedip {
    uint16_t family;               // AF_INET or AF_INET6
    union {
        struct in_addr ip4;       // IPv4 address
        struct in6_addr ip6;      // IPv6 address
    };
    uint8_t cidr;                  // Network prefix length
    uint32_t flags;                // WGALLOWEDIP_* flags
    struct wgallowedip *next_allowedip;  // Next allowed IP
};
```

**Helper Macros:**

```c
#define for_each_wgpeer(device, peer) \
    for ((peer) = (device)->first_peer; (peer); (peer) = (peer)->next_peer)

#define for_each_wgallowedip(peer, allowedip) \
    for ((allowedip) = (peer)->first_allowedip; (allowedip); \
         (allowedip) = (allowedip)->next_allowedip)
```

**Cleanup Function:**

```c
static inline void free_wgdevice(struct wgdevice *dev);
```
- Recursively frees all peers and allowed IPs
- Safe to call with NULL pointer
- Clears all allocated memory

---

### config.h

**Purpose**: Configuration parsing API.

**Key Structures:**

```c
struct config_ctx {
    struct wgdevice *device;          // Device being configured
    struct wgpeer *last_peer;         // Current peer
    struct wgallowedip *last_allowedip;  // Current allowed IP
    bool is_peer_section;             // Currently in [Peer] section
    bool is_device_section;           // Currently in [Interface] section
};
```

**Public API:**

```c
bool config_read_init(struct config_ctx *ctx, bool append);
bool config_read_line(struct config_ctx *ctx, const char *line);
struct wgdevice *config_read_finish(struct config_ctx *ctx);
struct wgdevice *config_read_cmd(const char *argv[], int argc);
```

---

### ipc.h

**Purpose**: IPC layer public API.

**Public Functions:**

```c
int ipc_set_device(struct wgdevice *dev);
int ipc_get_device(struct wgdevice **dev, const char *interface);
char *ipc_list_devices(void);
```

**Platform Detection:**
- Includes appropriate platform header based on OS
- Defines `IPC_SUPPORTS_KERNEL_INTERFACE` when kernel support available

---

### encoding.h

**Purpose**: Key encoding/decoding API.

**Constants:**

```c
#define WG_KEY_LEN_BASE64 45  // 32 bytes -> 44 chars + null
#define WG_KEY_LEN_HEX 65     // 32 bytes -> 64 chars + null
```

**Public Functions:**

```c
void key_to_base64(char base64[WG_KEY_LEN_BASE64], const uint8_t key[WG_KEY_LEN]);
bool key_from_base64(uint8_t key[WG_KEY_LEN], const char *base64);
void key_to_hex(char hex[WG_KEY_LEN_HEX], const uint8_t key[WG_KEY_LEN]);
bool key_from_hex(uint8_t key[WG_KEY_LEN], const char *hex);
bool key_is_zero(const uint8_t key[WG_KEY_LEN]);
```

---

### curve25519.h

**Purpose**: Elliptic curve cryptography API.

**Constants:**

```c
enum curve25519_lengths {
    CURVE25519_KEY_SIZE = 32
};
```

**Public Functions:**

```c
void curve25519(uint8_t mypublic[32], const uint8_t secret[32], 
                const uint8_t basepoint[32]);
void curve25519_generate_public(uint8_t pub[32], const uint8_t secret[32]);
```

**Key Clamping:**

```c
static inline void curve25519_clamp_secret(uint8_t secret[32]) {
    secret[0] &= 248;
    secret[31] = (secret[31] & 127) | 64;
}
```
- Ensures secret is valid Curve25519 scalar
- Clears low 3 bits (multiple of 8)
- Clears high bit and sets second-highest bit
