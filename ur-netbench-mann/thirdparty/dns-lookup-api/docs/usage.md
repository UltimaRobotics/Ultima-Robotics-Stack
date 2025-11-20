
# DNS Lookup API - Usage Guide

This guide provides detailed instructions on how to use the DNS Lookup API both as a C++ library and as a command-line binary.

## Table of Contents

1. [Building the Project](#building-the-project)
2. [Using the Binary](#using-the-binary)
3. [Using the API in Your Code](#using-the-api-in-your-code)
4. [Configuration Reference](#configuration-reference)
5. [Output Format](#output-format)
6. [Real-time Export Feature](#real-time-export-feature)

---

## Building the Project

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+)
- CMake 3.10 or higher
- nlohmann/json header library

### Build Steps

```bash
# Navigate to the dns-lookup-api directory
cd net-bench-apis/dns-lookup-api

# Download the nlohmann json header
cd include
wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
cd ..

# Build using CMake
mkdir -p build
cd build
cmake ..
make -j4

# Or use the provided Makefile
cd ..
make
```

The binary will be located at `build/dns_lookup_cli`.

---

## Using the Binary

### Basic Usage

The `dns_lookup_cli` binary accepts configuration via JSON file or JSON string.

#### Using a Configuration File

```bash
./build/dns_lookup_cli --config examples/lookup_a.json
```

#### Using a JSON String

```bash
./build/dns_lookup_cli --json '{"hostname":"google.com","query_type":"A"}'
```

### Command-Line Options

| Option | Short | Description |
|--------|-------|-------------|
| `--config <file>` | `-c` | Load configuration from a JSON file |
| `--json <string>` | `-j` | Load configuration from a JSON string |
| `--help` | `-h` | Display help message |
| `--example` | `-e` | Show example JSON configuration |

### Example Commands

**Look up A records for google.com:**
```bash
./build/dns_lookup_cli --json '{"hostname":"google.com","query_type":"A"}'
```

**Look up MX records:**
```bash
./build/dns_lookup_cli --json '{"hostname":"google.com","query_type":"MX"}'
```

**Look up AAAA records with custom timeout:**
```bash
./build/dns_lookup_cli --json '{"hostname":"google.com","query_type":"AAAA","timeout_ms":10000}'
```

**Look up with real-time export to file:**
```bash
./build/dns_lookup_cli --config examples/lookup_with_export.json
```

### Output Format

The binary produces JSON output with the query results:

```json
{
  "hostname": "google.com",
  "query_type": "A",
  "success": true,
  "nameserver": "8.8.8.8",
  "query_time_ms": 12.456,
  "records": [
    {
      "type": "A",
      "value": "142.250.185.46",
      "ttl": 0
    }
  ]
}
```

If the query fails:

```json
{
  "hostname": "nonexistent.example.com",
  "query_type": "A",
  "success": false,
  "nameserver": "",
  "query_time_ms": 0.0,
  "error_message": "Name or service not known"
}
```

---

## Using the API in Your Code

### Basic Integration

#### 1. Include the Header

```cpp
#include "DNSLookupAPI.hpp"
```

#### 2. Create and Configure

```cpp
// Create DNS lookup instance
DNSLookupAPI dns;

// Configure the lookup
DNSConfig config;
config.hostname = "google.com";
config.query_type = "A";
config.timeout_ms = 5000;
config.use_tcp = false;

dns.setConfig(config);
```

#### 3. Execute the Lookup

```cpp
DNSResult result = dns.execute();

if (result.success) {
    std::cout << "Lookup successful!\n";
    std::cout << "Query time: " << result.query_time_ms << " ms\n";
    std::cout << "Nameserver: " << result.nameserver << "\n";
    
    for (const auto& record : result.records) {
        std::cout << "Type: " << record.type 
                  << ", Value: " << record.value 
                  << ", TTL: " << record.ttl << "\n";
    }
} else {
    std::cerr << "Lookup failed: " << result.error_message << "\n";
}
```

### Complete Example

```cpp
#include "DNSLookupAPI.hpp"
#include <iostream>

int main() {
    DNSLookupAPI dns;
    
    // Configure DNS lookup
    DNSConfig config;
    config.hostname = "google.com";
    config.query_type = "A";
    config.timeout_ms = 5000;
    config.use_tcp = false;
    config.export_file_path = "dns_results.json";
    
    dns.setConfig(config);
    
    // Execute
    DNSResult result = dns.execute();
    
    // Display results
    std::cout << "Hostname: " << result.hostname << "\n";
    std::cout << "Query Type: " << result.query_type << "\n";
    std::cout << "Success: " << (result.success ? "Yes" : "No") << "\n";
    std::cout << "Query Time: " << result.query_time_ms << " ms\n";
    std::cout << "Nameserver: " << result.nameserver << "\n";
    
    if (result.success) {
        std::cout << "\nRecords found: " << result.records.size() << "\n";
        for (const auto& record : result.records) {
            std::cout << "  - " << record.type << ": " << record.value;
            if (record.ttl > 0) {
                std::cout << " (TTL: " << record.ttl << ")";
            }
            std::cout << "\n";
        }
    } else {
        std::cerr << "Error: " << result.error_message << "\n";
    }
    
    return result.success ? 0 : 1;
}
```

### Advanced Example: Multiple Query Types

```cpp
#include "DNSLookupAPI.hpp"
#include <iostream>
#include <vector>

int main() {
    std::string hostname = "google.com";
    std::vector<std::string> query_types = {"A", "AAAA", "MX", "NS", "TXT"};
    
    DNSLookupAPI dns;
    DNSConfig config;
    config.hostname = hostname;
    config.timeout_ms = 5000;
    
    for (const auto& type : query_types) {
        config.query_type = type;
        dns.setConfig(config);
        
        std::cout << "\n=== " << type << " Records for " << hostname << " ===\n";
        
        DNSResult result = dns.execute();
        
        if (result.success && !result.records.empty()) {
            std::cout << "Query time: " << result.query_time_ms << " ms\n";
            for (const auto& record : result.records) {
                std::cout << "  " << record.value << "\n";
            }
        } else if (result.success && result.records.empty()) {
            std::cout << "  No records found\n";
        } else {
            std::cout << "  Error: " << result.error_message << "\n";
        }
    }
    
    return 0;
}
```

### Linking Against the Library

When building your project:

```cmake
# CMakeLists.txt
add_executable(my_app main.cpp)
target_link_libraries(my_app dns_lookup_api)
```

Or with g++ directly:

```bash
g++ -std=c++17 -I/path/to/dns-lookup-api/include \
    my_app.cpp /path/to/dns-lookup-api/build/libdns_lookup_api.a \
    -o my_app
```

---

## Configuration Reference

### DNSConfig Structure

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `hostname` | `string` | *required* | Domain name or IP address to look up |
| `query_type` | `string` | "A" | DNS record type (see below) |
| `nameserver` | `string` | "" | Custom nameserver (empty = system default) |
| `timeout_ms` | `int` | 5000 | Timeout in milliseconds |
| `use_tcp` | `bool` | false | Use TCP instead of UDP |
| `export_file_path` | `string` | "" | Path to export results in real-time |

### Supported Query Types

| Type | Description | Example Use Case |
|------|-------------|------------------|
| `A` | IPv4 address | Standard domain to IP lookup |
| `AAAA` | IPv6 address | IPv6 connectivity testing |
| `MX` | Mail exchange records | Email server discovery |
| `NS` | Name server records | DNS delegation info |
| `TXT` | Text records | SPF, DKIM, domain verification |
| `CNAME` | Canonical name records | Alias resolution |
| `SOA` | Start of authority records | Zone information |
| `PTR` | Pointer records | Reverse DNS lookup |
| `ANY` | All available records | Comprehensive lookup |

### JSON Configuration Example

```json
{
  "hostname": "google.com",
  "query_type": "A",
  "nameserver": "",
  "timeout_ms": 5000,
  "use_tcp": false,
  "export_file_path": "dns_export.json"
}
```

---

## Output Format

### DNSResult Structure

| Field | Type | Description |
|-------|------|-------------|
| `hostname` | `string` | Queried hostname |
| `query_type` | `string` | DNS record type queried |
| `success` | `bool` | Whether the query succeeded |
| `error_message` | `string` | Error message if failed |
| `records` | `vector<DNSRecord>` | Array of DNS records found |
| `nameserver` | `string` | Nameserver that was used |
| `query_time_ms` | `double` | Query execution time in milliseconds |

### DNSRecord Structure

| Field | Type | Description |
|-------|------|-------------|
| `type` | `string` | Record type (A, AAAA, MX, etc.) |
| `value` | `string` | Record value (IP address, hostname, etc.) |
| `ttl` | `int` | Time-to-live (0 if not available) |

---

## Real-time Export Feature

When `export_file_path` is specified, the API writes results to a JSON file in real-time.

### Export File Structure

```json
{
  "query": {
    "hostname": "google.com",
    "query_type": "A",
    "nameserver": "",
    "timeout_ms": 5000,
    "use_tcp": false
  },
  "records": [
    {
      "type": "A",
      "value": "142.250.185.46",
      "ttl": 0
    }
  ],
  "summary": {
    "success": true,
    "total_records": 1,
    "query_time_ms": 12.456,
    "nameserver": "8.8.8.8"
  }
}
```

### Export File with Error

If the query fails, the export file includes error information:

```json
{
  "query": {
    "hostname": "nonexistent.example.com",
    "query_type": "A",
    "nameserver": "",
    "timeout_ms": 5000,
    "use_tcp": false
  },
  "records": [],
  "summary": {
    "success": false,
    "total_records": 0,
    "query_time_ms": 0.0,
    "error": "Name or service not known"
  }
}
```

### Export Behavior

- **File Creation**: If the file doesn't exist, it's created automatically
- **File Cleanup**: If the file exists, it's truncated (cleared) before writing
- **Real-time Updates**: Records are written as they are discovered and flushed immediately
- **Summary Statistics**: Written at the end with query metrics and status
- **Error Handling**: If the query fails, a summary with error information is still written

### Monitoring Export Files

You can monitor the export file in real-time:

```bash
# Run DNS lookup with export
./build/dns_lookup_cli --config examples/lookup_with_export.json &

# View the file
cat dns_export_results.json
```

Or use `watch` for continuous monitoring:

```bash
watch -n 1 cat dns_export_results.json
```

---

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Hostname cannot be empty" | Missing hostname in config | Provide a valid hostname |
| "Name or service not known" | Invalid hostname or DNS resolution failed | Check hostname spelling and DNS configuration |
| "Connection timed out" | DNS server unreachable | Check network connectivity or increase timeout |
| "Failed to open export file" | Permission or path issue | Verify file path and write permissions |

### Checking for Errors

```cpp
DNSResult result = dns.execute();

if (!result.success) {
    std::cerr << "Error: " << result.error_message << "\n";
    std::cerr << "Last API error: " << dns.getLastError() << "\n";
}
```

### Error Codes and Messages

The API uses system error messages from `getaddrinfo()` which can include:

- "Name or service not known" - Hostname doesn't exist
- "Temporary failure in name resolution" - DNS server temporarily unavailable
- "System error" - Internal system error occurred
- "No address associated with hostname" - Valid hostname but no records of requested type

---

## Performance Considerations

- **Timeout**: Default 5000ms is reasonable for most cases; reduce for faster failure detection
- **TCP vs UDP**: UDP is faster but less reliable; use TCP for zones that require it
- **Multiple Queries**: When performing multiple lookups, reuse the `DNSLookupAPI` instance
- **Export File**: Writing to export file adds minimal overhead due to buffering

---

## Examples

See the `examples/` directory for ready-to-use configurations:

- `lookup_a.json` - Look up A records (IPv4)
- `lookup_aaaa.json` - Look up AAAA records (IPv6)
- `lookup_mx.json` - Look up MX records (mail servers)
- `lookup_with_export.json` - Look up with real-time export to file

Run any example:

```bash
./build/dns_lookup_cli --config examples/lookup_a.json
```

---

## Use Cases

### 1. Domain Resolution Testing

```cpp
DNSConfig config;
config.hostname = "example.com";
config.query_type = "A";

DNSLookupAPI dns;
dns.setConfig(config);
DNSResult result = dns.execute();

if (result.success && !result.records.empty()) {
    std::cout << "Domain resolves to: " << result.records[0].value << "\n";
}
```

### 2. Mail Server Discovery

```cpp
DNSConfig config;
config.hostname = "example.com";
config.query_type = "MX";

DNSLookupAPI dns;
dns.setConfig(config);
DNSResult result = dns.execute();

if (result.success) {
    std::cout << "Mail servers for " << config.hostname << ":\n";
    for (const auto& record : result.records) {
        std::cout << "  " << record.value << "\n";
    }
}
```

### 3. DNS Performance Testing

```cpp
DNSConfig config;
config.hostname = "google.com";
config.query_type = "A";
config.export_file_path = "dns_performance.json";

DNSLookupAPI dns;
dns.setConfig(config);

auto start = std::chrono::high_resolution_clock::now();
DNSResult result = dns.execute();
auto end = std::chrono::high_resolution_clock::now();

std::cout << "Total execution time: " 
          << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() 
          << " ms\n";
std::cout << "DNS query time: " << result.query_time_ms << " ms\n";
```

### 4. Reverse DNS Lookup

```cpp
DNSConfig config;
config.hostname = "8.8.8.8.in-addr.arpa";
config.query_type = "PTR";

DNSLookupAPI dns;
dns.setConfig(config);
DNSResult result = dns.execute();

if (result.success && !result.records.empty()) {
    std::cout << "8.8.8.8 resolves to: " << result.records[0].value << "\n";
}
```

---

## Limitations

1. **TTL Values**: The current implementation using `getaddrinfo()` doesn't provide TTL values (always 0)
2. **Query Types**: Some advanced query types (MX, NS, TXT, etc.) may require alternative implementation for full functionality
3. **DNSSEC**: DNSSEC validation is not currently supported
4. **Custom Nameservers**: While the configuration supports custom nameservers, the current implementation uses system defaults

---

## Troubleshooting

### Issue: No records returned for valid domain

**Solution**: Try different query types or check if the domain actually has records of that type:

```bash
./build/dns_lookup_cli --json '{"hostname":"google.com","query_type":"A"}'
./build/dns_lookup_cli --json '{"hostname":"google.com","query_type":"AAAA"}'
```

### Issue: Query times out

**Solution**: Increase timeout or check network connectivity:

```bash
./build/dns_lookup_cli --json '{"hostname":"google.com","query_type":"A","timeout_ms":10000}'
```

### Issue: Export file not created

**Solution**: Check write permissions and directory existence:

```bash
# Ensure directory exists
mkdir -p output
./build/dns_lookup_cli --json '{"hostname":"google.com","query_type":"A","export_file_path":"output/dns_export.json"}'
```

---

## Best Practices

1. **Always check `result.success`** before accessing records
2. **Handle empty record sets** - a successful query may return zero records
3. **Set appropriate timeouts** based on your network conditions
4. **Use export files** for debugging and auditing purposes
5. **Reuse API instances** when performing multiple queries for better performance

---

## Integration with Other Tools

### Combine with Ping API

```cpp
#include "DNSLookupAPI.hpp"
#include "PingAPI.hpp"

// First resolve the hostname
DNSLookupAPI dns;
DNSConfig dns_config;
dns_config.hostname = "google.com";
dns_config.query_type = "A";
dns.setConfig(dns_config);

DNSResult dns_result = dns.execute();

if (dns_result.success && !dns_result.records.empty()) {
    // Then ping the resolved IP
    PingAPI ping;
    PingConfig ping_config;
    ping_config.destination = dns_result.records[0].value;
    ping_config.count = 5;
    ping.setConfig(ping_config);
    
    PingResult ping_result = ping.execute();
    // Process ping results...
}
```

---

## API Reference Summary

### Main Class

```cpp
class DNSLookupAPI {
public:
    DNSLookupAPI();
    ~DNSLookupAPI();
    
    void setConfig(const DNSConfig& config);
    DNSResult execute();
    std::string getLastError() const;
};
```

### Configuration

```cpp
struct DNSConfig {
    std::string hostname;           // Required
    std::string query_type;         // Default: "A"
    std::string nameserver;         // Default: ""
    int timeout_ms;                 // Default: 5000
    bool use_tcp;                   // Default: false
    std::string export_file_path;   // Default: ""
};
```

### Results

```cpp
struct DNSResult {
    std::string hostname;
    std::string query_type;
    bool success;
    std::string error_message;
    std::vector<DNSRecord> records;
    std::string nameserver;
    double query_time_ms;
};

struct DNSRecord {
    std::string type;
    std::string value;
    int ttl;
};
```
