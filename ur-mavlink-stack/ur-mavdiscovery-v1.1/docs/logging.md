
# Logging System

## Overview

The Logger provides thread-safe, configurable logging with multiple output destinations and severity levels.

## Log Levels

```cpp
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};
```

### Level Hierarchy

- DEBUG: Detailed diagnostic information
- INFO: General informational messages
- WARNING: Warning messages (potential issues)
- ERROR: Error messages (failures)

## Logger Architecture

### Singleton Pattern

```cpp
Logger& Logger::getInstance();
```

Global logging instance.

### Thread Safety

```cpp
std::mutex mutex_;
```

All logging operations are mutex-protected.

## Configuration

### Set Log Level

```cpp
Logger::getInstance().setLogLevel(LogLevel::INFO);
```

Only logs at or above this level are output.

### Set Log File

```cpp
Logger::getInstance().setLogFile("mavdiscovery.log");
```

Enables file logging:
- Append mode
- Auto-flush
- Thread-safe writes

## Logging Macros

### Convenience Macros

```cpp
#define LOG_DEBUG(msg) Logger::getInstance().log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg) Logger::getInstance().log(LogLevel::INFO, msg)
#define LOG_WARNING(msg) Logger::getInstance().log(LogLevel::WARNING, msg)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::ERROR, msg)
```

### Usage

```cpp
LOG_INFO("Starting device discovery");
LOG_ERROR("Failed to open port: " + devicePath);
LOG_DEBUG("Testing baudrate: " + std::to_string(baudrate));
```

## Log Format

### Message Structure

```
[2025-01-10 12:34:56.789] [INFO] Starting device discovery
```

Components:
- Timestamp (millisecond precision)
- Log level
- Message content

### Timestamp Generation

```cpp
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}
```

## Output Destinations

### Console Output

All logs written to stdout:

```cpp
std::cout << logMsg << std::endl;
```

Benefits:
- Real-time monitoring
- Easy debugging
- Standard output capture

### File Output

Optional file logging:

```cpp
if (logFile_.is_open()) {
    logFile_ << logMsg << std::endl;
    logFile_.flush();
}
```

Features:
- Persistent logging
- Append mode (preserves history)
- Auto-flush (immediate writes)

## Logging Best Practices

### When to Log

**DEBUG**: Implementation details
```cpp
LOG_DEBUG("Buffer size: " + std::to_string(size));
LOG_DEBUG("Thread ID: " + std::to_string(threadId));
```

**INFO**: Normal operations
```cpp
LOG_INFO("Device added: " + devicePath);
LOG_INFO("Verification complete");
```

**WARNING**: Potential issues
```cpp
LOG_WARNING("Thread did not stop gracefully");
LOG_WARNING("Config file not found, using defaults");
```

**ERROR**: Failures
```cpp
LOG_ERROR("Failed to initialize udev");
LOG_ERROR("HTTP POST failed: " + error);
```

### Message Content

Good messages:
- Include context
- Provide values
- Aid debugging

Examples:
```cpp
LOG_INFO("Device verified: " + devicePath + " @ " + 
         std::to_string(baudrate) + " baud");
LOG_ERROR("Port open failed: " + devicePath + ": " + errorMsg);
```

## Performance Considerations

### Mutex Overhead

Logging is synchronous:
- Mutex lock per call
- May impact high-frequency logging
- Consider LOG_DEBUG for verbose output

### String Construction

Build strings efficiently:

```cpp
// Good
LOG_INFO("Count: " + std::to_string(count));

// Avoid
std::string msg = "Count: " + std::to_string(count);
LOG_INFO(msg);  // Extra string copy
```

### Level Filtering

Early filtering:

```cpp
if (level < minLevel_) return;  // Skip formatting
```

No overhead for disabled levels.

## Integration

### Initialization

```cpp
// Set from configuration
Logger::getInstance().setLogLevel(LogLevel::INFO);
Logger::getInstance().setLogFile("mavdiscovery.log");
```

### Throughout Application

```cpp
// DeviceMonitor
LOG_INFO("Device monitor started");

// DeviceVerifier
LOG_DEBUG("Testing baudrate: " + std::to_string(baudrate));

// DeviceManager
LOG_WARNING("Remaining threads: " + std::to_string(count));
```

## Log Rotation

### Manual Rotation

Current implementation:
- Append mode only
- No automatic rotation
- Manual file management

### Future Enhancements

Potential improvements:
- Size-based rotation
- Time-based rotation
- Compression of old logs
- Configurable retention

## Debugging Tips

### Increase Verbosity

```cpp
Logger::getInstance().setLogLevel(LogLevel::DEBUG);
```

### Filter by Component

Use grep/search:
```bash
grep "DeviceVerifier" mavdiscovery.log
grep "ERROR" mavdiscovery.log
```

### Timestamp Analysis

Analyze timing:
- Verification duration
- Thread lifecycle
- Event sequences
