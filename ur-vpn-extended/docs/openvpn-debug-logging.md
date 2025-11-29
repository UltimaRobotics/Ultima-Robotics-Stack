# OpenVPN Debug Logging Control

## Overview

This document describes the build-time control for OpenVPN debug logging that outputs verbose network operation messages such as:
- `UDPv4 write returned X`
- `event_wait returned X` 
- `read from TUN/TAP returned X`

## Build Configuration

### CMake Option

The OpenVPN debug logging is controlled by the `OPENVPN_DEBUG_LOGGING` CMake option:

```bash
# Enable OpenVPN debug logging (default: OFF)
cmake -DOPENVPN_DEBUG_LOGGING=ON ..

# Disable OpenVPN debug logging  
cmake -DOPENVPN_DEBUG_LOGGING=OFF ..
```

### Behavior

- **When DISABLED (default)**: Only error conditions (negative return values) are logged
- **When ENABLED**: All network operations are logged, including successful reads/writes

### Implementation Details

The control is implemented using conditional compilation:

1. **CMakeLists.txt**: Defines `OPENVPN_DEBUG_LOGGING_ENABLED` preprocessor macro when enabled
2. **error.h**: Modifies `check_status()` to conditionally include debug level checks
3. **error.c**: Wraps the actual `msg()` call in `#ifdef OPENVPN_DEBUG_LOGGING_ENABLED`

### Code Changes

#### error.h
```c
static inline void
check_status(int status, const char *description, struct link_socket *sock, struct tuntap *tt)
{
#ifdef OPENVPN_DEBUG_LOGGING_ENABLED
    if (status < 0 || check_debug_level(x_cs_verbose_level))
    {
        x_check_status(status, description, sock, tt);
    }
#else
    if (status < 0)
    {
        x_check_status(status, description, sock, tt);
    }
#endif
}
```

#### error.c
```c
void
x_check_status(int status, const char *description, struct link_socket *sock, struct tuntap *tt)
{
    // ... initialization code ...
    
#ifdef OPENVPN_DEBUG_LOGGING_ENABLED
    msg(x_cs_verbose_level, "%s %s returned %d",
        sock ? proto2ascii(sock->info.proto, sock->info.af, true) : "", description, status);
#endif
    
    // ... rest of function ...
}
```

## Usage Examples

### Build with Debug Logging Disabled (Default)
```bash
cd /home/fyou/Desktop/Ultima-Robotics-Stack/ur-vpn-extended
cmake -B build -DOPENVPN_DEBUG_LOGGING=OFF
make -C build
```
Output: `-- OpenVPN debug logging: DISABLED`

### Build with Debug Logging Enabled
```bash
cd /home/fyou/Desktop/Ultima-Robotics-Stack/ur-vpn-extended
cmake -B build -DOPENVPN_DEBUG_LOGGING=ON
make -C build
```
Output: `-- OpenVPN debug logging: ENABLED`

## Runtime Impact

- **Performance**: Minimal impact when disabled (conditional compilation removes debug code)
- **Log Volume**: Significantly reduces log spam in production environments
- **Debugging**: Enables detailed network operation tracing for development/troubleshooting

## Related Files

- `CMakeLists.txt` - Build configuration
- `ur-openvpn-library/src/source-port/lib-src/error.h` - Header with conditional compilation
- `ur-openvpn-library/src/source-port/lib-src/error.c` - Implementation with conditional logging
