
# DNS Lookup API

A C++ library and command-line tool for performing DNS lookups without using terminal execution.

## Features

- Performs DNS lookups programmatically using C++ sockets
- Supports multiple DNS record types (A, AAAA, MX, NS, TXT, CNAME, SOA, PTR, ANY)
- JSON-based configuration
- JSON output format
- Custom nameserver support
- Configurable timeout
- TCP/UDP support

## Building

```bash
mkdir -p build
cd build
cmake ..
make
```

## Usage

### Command Line

Using a JSON configuration file:
```bash
./dns_cli --config examples/lookup_a.json
```

Using a JSON string:
```bash
./dns_cli --json '{"hostname":"google.com","query_type":"A"}'
```

Show help:
```bash
./dns_cli --help
```

Show example configuration:
```bash
./dns_cli --example
```

### Configuration Format

```json
{
  "hostname": "google.com",
  "query_type": "A",
  "nameserver": "",
  "timeout_ms": 5000,
  "use_tcp": false
}
```

### Supported Query Types

- **A**: IPv4 address
- **AAAA**: IPv6 address
- **MX**: Mail exchange records
- **NS**: Name server records
- **TXT**: Text records
- **CNAME**: Canonical name records
- **SOA**: Start of authority records
- **PTR**: Pointer records (reverse DNS)
- **ANY**: All available records

### Output Format

```json
{
  "hostname": "google.com",
  "query_type": "A",
  "success": true,
  "nameserver": "8.8.8.8",
  "query_time_ms": 15.234,
  "records": [
    {
      "type": "A",
      "value": "142.250.185.46",
      "ttl": 0
    }
  ]
}
```

## Library Usage

```cpp
#include "DNSLookupAPI.hpp"

DNSConfig config;
config.hostname = "google.com";
config.query_type = "A";

DNSLookupAPI dns;
dns.setConfig(config);

DNSResult result = dns.execute();

if (result.success) {
    for (const auto& record : result.records) {
        std::cout << record.type << ": " << record.value << std::endl;
    }
}
```

## Dependencies

- C++17 compiler
- nlohmann/json (single header library included)
- POSIX socket libraries (libresolv)
