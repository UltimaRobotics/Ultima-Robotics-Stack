# Operations Guide

## Build Operations

### Building the Library and CLI

**Prerequisites:**
- CMake 3.10 or later
- C11-compatible compiler (GCC, Clang)
- Make or Ninja build system
- Root/sudo access for installation

**Build Steps:**

```bash
# Navigate to project directory
cd ur-wg-provider

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build library and executable
make

# Optional: Install system-wide
sudo make install
```

**Build Outputs:**

- **Static Library**: `build/lib/libwg-provider.a`
  - Link this into your applications
  - Contains all WireGuard management functionality

- **CLI Executable**: `build/bin/wg-tunnel`
  - Standalone tunnel management tool
  - Requires root privileges to run

### Build Configuration Options

**Debug Build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```
- Enables debug symbols (-g)
- Disables optimizations (-O0)
- Useful for development and troubleshooting

**Release Build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
- Enables optimizations (-O2)
- Includes debug symbols for profiling
- Recommended for production use

**Custom Install Prefix:**
```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/wireguard ..
make install
```

### Cleaning Build Artifacts

```bash
# Clean build outputs
make clean

# Remove all build files
cd ..
rm -rf build/
```

---

## Runtime Operations

### Starting a WireGuard Tunnel

**Usage**

### Basic Usage

```bash
# Start tunnel with default interface name (wg0)
sudo ./wg-tunnel /etc/wireguard/wg0.conf

# Start tunnel with custom interface name
sudo ./wg-tunnel /etc/wireguard/client.conf wg-client

# Start tunnel without routing rules (no-bypass mode)
sudo ./wg-tunnel /etc/wireguard/wg0.conf -no-bypass
sudo ./wg-tunnel /etc/wireguard/wg0.conf wg-client -no-bypass
```

### No-Bypass Mode

The `-no-bypass` flag allows you to establish a WireGuard connection without adding any routing rules from the `AllowedIPs`. This is useful when:

- You want to manually manage routing tables
- You need the tunnel for specific applications only
- You're using the tunnel in a non-standard network configuration

In no-bypass mode:
- The WireGuard interface is created and configured
- IP addresses are assigned to the interface
- The interface is brought up
- **No routing rules are added** (0.0.0.0/0, ::/0, or any AllowedIPs routes)
- DNS configuration is still applied (if specified)

**Operational Flow:**

1. **Parse Configuration**
   - Read and validate config file
   - Parse interface and peer settings
   - Decode cryptographic keys

2. **Create Network Interface**
   - Execute: `ip link add dev <name> type wireguard`
   - Allocates kernel resources
   - Registers interface with network stack

3. **Apply Configuration**
   - Set private key
   - Set listen port
   - Configure firewall mark
   - Add all peers with allowed IPs

4. **Bring Interface Up**
   - Execute: `ip link set dev <name> up`
   - Interface becomes active
   - Routes are installed
   - Peers can connect

5. **Display Status**
   - Show configured interface
   - Display all peers
   - Show allowed IP ranges

### Stopping a WireGuard Tunnel

```bash
# Remove interface (stops tunnel)
sudo ip link del wg0

# Or use interface name
sudo ip link del wg-client
```

**Cleanup Operations:**
- Closes all peer connections
- Removes routing table entries
- Deallocates kernel resources
- Frees cryptographic keys from memory

### Monitoring Tunnel Status

**View Configuration:**
```bash
# Using wg command (if installed)
sudo wg show wg0

# Using ip command
ip link show wg0
ip addr show wg0
```

**Check Peer Handshakes:**
```bash
sudo wg show wg0 latest-handshakes
```
- Recent handshake (< 3 minutes): Active connection
- Old handshake: Connection may be idle or broken

**Monitor Transfer Statistics:**
```bash
sudo wg show wg0 transfer
```
- Shows RX/TX bytes per peer
- Useful for bandwidth monitoring

---

## Configuration Operations

### Configuration File Format

WireGuard uses INI-style configuration files:

```ini
[Interface]
PrivateKey = <base64-encoded-private-key>
ListenPort = 51820
FwMark = 0x1234

[Peer]
PublicKey = <base64-encoded-public-key>
PresharedKey = <base64-encoded-psk>  # Optional
Endpoint = 192.0.2.1:51820
AllowedIPs = 10.0.0.0/24, 0.0.0.0/0
PersistentKeepalive = 25
```

### Generating Keys

**Generate Private Key:**
```bash
wg genkey > privatekey

# Or using urandom directly
dd if=/dev/urandom bs=32 count=1 2>/dev/null | base64 > privatekey
```

**Generate Public Key from Private Key:**
```bash
wg pubkey < privatekey > publickey

# Manual calculation using curve25519
# (requires implementation in your code)
```

**Generate Preshared Key:**
```bash
wg genpsk > preshared

# Or using urandom
dd if=/dev/urandom bs=32 count=1 2>/dev/null | base64 > preshared
```

### Configuration Validation

**Validate Before Applying:**

```bash
# Check syntax (wg-tunnel will validate)
sudo ./build/bin/wg-tunnel config/test.conf 2>&1 | grep -i error

# Dry-run (requires code modification to add --dry-run flag)
```

**Common Configuration Errors:**

1. **Invalid Key Format**
   - Keys must be 44 base64 characters
   - Must end with padding if needed
   - Example: `sI4jS0DYf1T14K4jUpA5dlcGoupzHZMuIk6YPeIvnEE=`

2. **Invalid Endpoint**
   - Must include port: `192.0.2.1:51820`
   - IPv6 must use brackets: `[2001:db8::1]:51820`
   - DNS names are resolved at config time

3. **Invalid AllowedIPs**
   - Must be valid CIDR notation
   - Examples: `10.0.0.0/24`, `0.0.0.0/0`, `::/0`
   - Comma-separated for multiple ranges

---

## Embedding the Library

### Linking Against libwg-provider.a

**CMake Integration:**

```cmake
# In your CMakeLists.txt
add_executable(myapp main.c)

# Link static library
target_link_libraries(myapp ${CMAKE_SOURCE_DIR}/ur-wg-provider/build/lib/libwg-provider.a)

# Add include directories
target_include_directories(myapp PRIVATE ${CMAKE_SOURCE_DIR}/ur-wg-provider/include)
```

**Manual Compilation:**

```bash
# Compile your application
gcc -c myapp.c -I ur-wg-provider/include -o myapp.o

# Link with static library
gcc myapp.o ur-wg-provider/build/lib/libwg-provider.a -o myapp
```

### Using the API in Your Code

**Example: Read and Apply Configuration**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "ipc.h"
#include "containers.h"

int configure_tunnel(const char *config_path, const char *iface_name) {
    FILE *fp;
    char line[4096];
    struct config_ctx ctx = {0};
    struct wgdevice *dev = NULL;
    int ret = -1;

    // Initialize parser
    if (!config_read_init(&ctx, false)) {
        fprintf(stderr, "Failed to initialize config parser\n");
        return -1;
    }

    // Open config file
    fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open config file: %s\n", config_path);
        free_wgdevice(ctx.device);
        return -1;
    }

    // Parse line by line
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        if (!config_read_line(&ctx, line)) {
            fprintf(stderr, "Parse error: %s\n", line);
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);

    // Finalize configuration
    dev = config_read_finish(&ctx);
    if (!dev) {
        fprintf(stderr, "Invalid configuration\n");
        return -1;
    }

    // Set interface name
    strncpy(dev->name, iface_name, IFNAMSIZ - 1);

    // Apply to kernel
    ret = ipc_set_device(dev);
    if (ret < 0) {
        fprintf(stderr, "Failed to configure device: %s\n", strerror(-ret));
        free_wgdevice(dev);
        return ret;
    }

    printf("Successfully configured %s\n", iface_name);
    free_wgdevice(dev);
    return 0;
}
```

**Example: Retrieve Device Status**

```c
#include "ipc.h"
#include "containers.h"
#include "encoding.h"

void print_device_status(const char *iface_name) {
    struct wgdevice *dev = NULL;
    struct wgpeer *peer;
    char key_b64[WG_KEY_LEN_BASE64];
    int ret;

    ret = ipc_get_device(&dev, iface_name);
    if (ret < 0) {
        fprintf(stderr, "Failed to get device: %s\n", strerror(-ret));
        return;
    }

    printf("Interface: %s\n", dev->name);

    if (dev->flags & WGDEVICE_HAS_PUBLIC_KEY) {
        key_to_base64(key_b64, dev->public_key);
        printf("Public Key: %s\n", key_b64);
    }

    if (dev->flags & WGDEVICE_HAS_LISTEN_PORT) {
        printf("Listen Port: %u\n", dev->listen_port);
    }

    printf("\nPeers:\n");
    for_each_wgpeer(dev, peer) {
        key_to_base64(key_b64, peer->public_key);
        printf("  Peer: %s\n", key_b64);
        printf("    RX: %llu bytes\n", (unsigned long long)peer->rx_bytes);
        printf("    TX: %llu bytes\n", (unsigned long long)peer->tx_bytes);
    }

    free_wgdevice(dev);
}
```

---

## Troubleshooting Operations

### Common Issues

**Issue: "No such device"**

```
Error: Failed to configure WireGuard interface 'wg0': No such device
```

**Solution:**
- Interface doesn't exist yet
- wg-tunnel automatically creates it, but manual operations need:
  ```bash
  sudo ip link add dev wg0 type wireguard
  ```

**Issue: Permission Denied**

```
Error: Failed to configure WireGuard interface 'wg0': Permission denied
```

**Solution:**
- WireGuard operations require root privileges
- Run with sudo: `sudo ./wg-tunnel config.conf`
- Or grant capabilities: `sudo setcap cap_net_admin+ep ./wg-tunnel`

**Issue: Parse Errors**

```
Line unrecognized: `SomeField=value'
```

**Solution:**
- Remove unsupported fields from config
- Supported fields listed in configuration documentation
- wg-quick fields (Address, DNS, etc.) are now silently ignored

**Issue: Invalid Key Format**

```
Key is not the correct length or format: `abcd1234'
```

**Solution:**
- Keys must be exactly 32 bytes encoded as 44-character base64
- Generate proper keys: `wg genkey`
- Check for trailing whitespace or newlines

### Debugging Operations

**Enable Verbose Output:**

Add debug prints to `main.c`:
```c
#define DEBUG 1

#ifdef DEBUG
#define debug_printf(...) fprintf(stderr, "[DEBUG] " __VA_ARGS__)
#else
#define debug_printf(...)
#endif
```

**Check Kernel Logs:**
```bash
# View WireGuard kernel messages
sudo dmesg | grep wireguard

# Monitor real-time
sudo dmesg -w | grep wireguard
```

**Verify Interface Creation:**
```bash
# List all network interfaces
ip link show

# Check WireGuard interfaces specifically
ip link show type wireguard
```

**Test Connectivity:**
```bash
# Ping through tunnel
ping -I wg0 10.0.0.1

# Check routing
ip route show table all | grep wg0

# Verify peer connectivity
wg show wg0 latest-handshakes
```

### Performance Operations

**Optimize MTU:**
```bash
# Set MTU (account for WireGuard overhead: 60 bytes)
sudo ip link set mtu 1420 dev wg0
```

**Monitor Performance:**
```bash
# Real-time transfer statistics
watch -n 1 'wg show wg0 transfer'

# Network throughput
iperf3 -c <peer-ip>
```

**Resource Usage:**
```bash
# CPU usage
top -p $(pgrep wireguard)

# Memory usage
cat /proc/$(pgrep wireguard)/status | grep VmRSS