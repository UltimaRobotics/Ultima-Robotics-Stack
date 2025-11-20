
# Build System Documentation

## Overview

The ur-openvpn-library uses CMake as its primary build system, providing a modular and configurable approach to building OpenVPN with custom API extensions.

## Build System Architecture

### Main Components

1. **CMakeLists.txt** - Root build configuration
2. **cmake/** - Modular build configuration files
3. **Makefile** - Convenience wrapper for common build tasks

### CMake Module Structure

```
cmake/
├── compiler_settings.cmake    # Compiler flags and warnings
├── dependencies.cmake          # Library dependency detection
├── external_deps.cmake         # External dependency management
├── installation.cmake          # Installation rules
├── platform_detection.cmake    # OS-specific detection
├── source_files.cmake          # Source file organization
├── summary.cmake               # Build configuration summary
└── target.cmake                # Build target definitions
```

## Building the Library

### Prerequisites

- CMake 3.10 or higher
- C compiler (GCC, Clang, or MSVC)
- OpenSSL development libraries
- Optional: LZO, LZ4, libnl (for Linux DCO support)

### Basic Build

```bash
cd ur-openvpn-library/source-port
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Build Options

#### Portable Build

Build all dependencies from source (useful for deployment):

```bash
cmake -D_PORTABLE_BUILD=ON ..
make -j$(nproc)
```

This will:
- Download and compile OpenSSL 1.1.1w
- Build LZO 2.10 compression library
- Build LZ4 1.9.4 compression library
- Build libcap-ng (Linux only)
- Build libnl (Linux only, for DCO support)

#### Custom Installation Prefix

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/openvpn ..
make install
```

#### Debug Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

#### Release Build (Optimized)

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Build Targets

### Library Targets

1. **openvpn_static** - Static OpenVPN library
   ```bash
   make openvpn_static
   ```

2. **openvpn_shared** - Shared OpenVPN library
   ```bash
   make openvpn_shared
   ```

### Executable Targets

1. **openvpn-bin** - Main OpenVPN executable
   ```bash
   make openvpn_exe
   ```

2. **ovpn-client-api** - Client API example
   ```bash
   make ovpn-client-api
   ```

3. **ovpn-server-api** - Server API example (when enabled)
   ```bash
   make ovpn-server-api
   ```

## Compiler Settings

### Flags (from compiler_settings.cmake)

```cmake
# Warning flags
-Wall
-Wold-style-definition
-Wstrict-prototypes
-Wno-stringop-truncation

# Debug flags
-g -O0

# Release flags
-g -O2
```

### Include Directories

The build system automatically includes:
- `${CMAKE_SOURCE_DIR}/include`
- `${CMAKE_SOURCE_DIR}/src/compat`
- `${CMAKE_BINARY_DIR}`
- `${CMAKE_BINARY_DIR}/include`

## Dependency Management

### System Dependencies

The build system searches for:

1. **OpenSSL** (required)
   - Provides crypto and SSL/TLS functionality
   - Detected via `find_package(OpenSSL REQUIRED)`

2. **LZO** (optional)
   - Compression support
   - Detected via `pkg_check_modules(LZO lzo2)`

3. **LZ4** (optional)
   - Modern compression
   - Detected via `pkg_check_modules(LZ4 liblz4)`

4. **libnl** (Linux only, optional)
   - Netlink support for DCO
   - Detected via `pkg_check_modules(LIBNL libnl-3.0>=3.4.0)`

5. **libcap-ng** (Linux only, optional)
   - Capability management
   - Detected via `pkg_check_modules(LIBCAPNG libcap-ng)`

### Portable Dependencies

When `_PORTABLE_BUILD=ON`, dependencies are built from source:

```cmake
# OpenSSL
URL: https://www.openssl.org/source/openssl-1.1.1w.tar.gz
Configuration: --prefix=${INSTALL_DIR} --openssldir=${INSTALL_DIR}/ssl no-shared

# LZO
URL: http://www.oberhumer.com/opensource/lzo/download/lzo-2.10.tar.gz
Configuration: --prefix=${INSTALL_DIR} --enable-static --disable-shared

# LZ4
URL: https://github.com/lz4/lz4/archive/v1.9.4.tar.gz
Build: BUILD_STATIC=yes BUILD_SHARED=no
```

## Platform-Specific Configuration

### Windows

- Uses Windows Sockets (ws2_32)
- Includes cryptography API (crypt32)
- IP Helper API (iphlpapi)
- Multimedia API (winmm)
- Windows Filtering Platform (fwpuclnt)

### Linux

- Supports DCO (Data Channel Offload) when libnl is available
- Uses standard POSIX APIs
- pthread for threading
- dl for dynamic loading

### DCO Support

DCO is automatically enabled on Linux when:
- libnl-3.0 >= 3.4.0 is found
- libnl-genl-3.0 >= 3.4.0 is found

To disable DCO:
```cmake
add_definitions(-DENABLE_DCO=0)
```

## Source File Organization

### Core Sources (from source_files.cmake)

```cmake
lib-src/
├── Core networking
│   ├── socket.c
│   ├── packet_id.c
│   └── buffer.c
├── Cryptography
│   ├── crypto.c
│   ├── ssl.c
│   └── tls_crypt.c
├── Protocol
│   ├── forward.c
│   ├── proto.c
│   └── reliable.c
└── Platform-specific
    ├── tun.c
    ├── win32.c
    └── dco_linux.c
```

### API Sources

```cmake
apis/
├── cJSON.c                    # JSON parsing
├── openvpn_client_api.c      # Client API implementation
├── openvpn_server_api.c      # Server API implementation
├── example_client_usage.c    # Client example
└── example_server_usage.c    # Server example
```

## Build Configuration Summary

After configuration, the build system displays:

```
OpenVPN Configuration Summary:
  Build type: Release
  C Compiler: /usr/bin/gcc
  OpenSSL: YES
  libcap-ng: YES/NO
  libnl: YES/NO
  DCO support: YES/NO
  LZO compression: YES/NO
  LZ4 compression: YES/NO
```

## Common Build Issues

### Missing OpenSSL

```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev

# RHEL/CentOS
sudo yum install openssl-devel

# Or use portable build
cmake -D_PORTABLE_BUILD=ON ..
```

### Missing LZ4/LZO

```bash
# Ubuntu/Debian
sudo apt-get install liblz4-dev liblzo2-dev

# Or use portable build
cmake -D_PORTABLE_BUILD=ON ..
```

### DCO Not Available

DCO requires libnl on Linux:

```bash
# Ubuntu/Debian
sudo apt-get install libnl-3-dev libnl-genl-3-dev

# Or disable DCO
cmake -DENABLE_DCO=OFF ..
```

## Makefile Convenience Commands

The root Makefile provides shortcuts:

```bash
# Configure and build
make build

# Clean build directory
make clean

# Rebuild from scratch
make rebuild

# Install
make install

# Run tests (if available)
make test
```

## Advanced Configuration

### Custom Compiler

```bash
cmake -DCMAKE_C_COMPILER=/usr/bin/clang ..
```

### Custom Build Flags

```bash
cmake -DCMAKE_C_FLAGS="-march=native -O3" ..
```

### Verbose Build

```bash
make VERBOSE=1
```

### Parallel Build

```bash
make -j$(nproc)  # Use all CPU cores
make -j4         # Use 4 cores
```

## Integration with Other Projects

### As a Static Library

```cmake
# In your CMakeLists.txt
add_subdirectory(ur-openvpn-library/source-port)
target_link_libraries(your_project openvpn_static)
```

### As a Shared Library

```cmake
add_subdirectory(ur-openvpn-library/source-port)
target_link_libraries(your_project openvpn_shared)
```

### Using the API

```c
#include "openvpn_client_api.h"

int main() {
    ovpn_client_api_init();
    // Your code here
    ovpn_client_api_cleanup();
    return 0;
}
```
