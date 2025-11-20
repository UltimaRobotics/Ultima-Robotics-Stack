
# Serial Communication

## Overview

The SerialPort class provides low-level serial communication with proper configuration for MAVLink traffic.

## Port Configuration

### Opening a Port

```cpp
SerialPort port("/dev/ttyUSB0");
if (!port.open(57600)) {
    // Handle error
}
```

Parameters:
- Device path
- Baudrate

### Termios Configuration

```cpp
// 8 data bits, no parity, 1 stop bit (8N1)
tty.c_cflag &= ~PARENB;   // No parity
tty.c_cflag &= ~CSTOPB;   // 1 stop bit
tty.c_cflag &= ~CSIZE;
tty.c_cflag |= CS8;       // 8 data bits

// No hardware flow control
tty.c_cflag &= ~CRTSCTS;

// Enable receiver, ignore modem control lines
tty.c_cflag |= CREAD | CLOCAL;
```

### Line Discipline

```cpp
// Raw mode (no line processing)
tty.c_lflag &= ~ICANON;   // Non-canonical
tty.c_lflag &= ~ECHO;     // No echo
tty.c_lflag &= ~ISIG;     // No signals

// No input processing
tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // No software flow control
tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

// No output processing
tty.c_oflag &= ~OPOST;
tty.c_oflag &= ~ONLCR;
```

### Read Behavior

```cpp
// Non-blocking reads
tty.c_cc[VTIME] = 0;  // No inter-character timer
tty.c_cc[VMIN] = 0;   // Return immediately
```

## Supported Baudrates

### Standard Rates

- 9600, 19200, 38400
- 57600, 115200, 230400
- 460800, 921600

### High-Speed Rates

- 500000, 576000
- 1000000, 1152000
- 1500000, 2000000

### Speed Mapping

```cpp
speed_t speed;
switch (baudrate) {
    case 57600: speed = B57600; break;
    case 115200: speed = B115200; break;
    case 921600: speed = B921600; break;
    // ... etc
}

cfsetispeed(&tty, speed);
cfsetospeed(&tty, speed);
```

## Read Operations

### Timeout-based Read

```cpp
int read(uint8_t* buffer, size_t size, int timeoutMs);
```

Implementation:

```cpp
fd_set readfds;
struct timeval timeout;

FD_ZERO(&readfds);
FD_SET(fd_, &readfds);

timeout.tv_sec = timeoutMs / 1000;
timeout.tv_usec = (timeoutMs % 1000) * 1000;

int ret = select(fd_ + 1, &readfds, nullptr, nullptr, &timeout);
if (ret > 0) {
    return ::read(fd_, buffer, size);
}
return 0;  // Timeout
```

Benefits:
- Non-blocking main thread
- Configurable timeout
- Efficient CPU usage

### Read Pattern

```cpp
std::vector<uint8_t> buffer(280);
int bytesRead = port.read(buffer.data(), buffer.size(), 100);
if (bytesRead > 0) {
    // Process data
}
```

## Write Operations

### Simple Write

```cpp
int write(const uint8_t* data, size_t size);
```

Implementation:

```cpp
return ::write(fd_, data, size);
```

Usage:
- Not used for MAVLink discovery
- Available for bidirectional communication

## Port Lifecycle

### Open

1. Open device file with O_RDWR | O_NOCTTY | O_NONBLOCK
2. Configure termios settings
3. Flush buffers (tcflush)
4. Ready for I/O

### Close

```cpp
void close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
```

Automatic cleanup:
- Destructor calls close()
- RAII pattern

### Status Check

```cpp
bool isOpen() const {
    return fd_ >= 0;
}
```

## Error Handling

### Open Failures

```cpp
fd_ = ::open(devicePath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
if (fd_ < 0) {
    LOG_ERROR("Failed to open " + devicePath_ + ": " + 
              std::string(strerror(errno)));
    return false;
}
```

Common errors:
- ENOENT: Device doesn't exist
- EACCES: Permission denied
- EBUSY: Device in use

### Configuration Failures

```cpp
if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    LOG_ERROR("tcsetattr failed: " + std::string(strerror(errno)));
    close();
    return false;
}
```

## Platform Specifics

### Linux-only

Uses Linux-specific:
- termios API
- select() for timeouts
- Standard serial device files

### Device Paths

Common patterns:
- `/dev/ttyUSB*`: USB-to-serial adapters
- `/dev/ttyACM*`: USB CDC ACM devices
- `/dev/ttyS*`: Native serial ports
- `/dev/ttyAMA*`: ARM serial ports (Raspberry Pi)

## Performance Considerations

### Buffer Size

280 bytes recommended:
- MAVLink v2 max packet size
- Efficient read operations
- Reduces read calls

### Read Timeout

100ms default:
- Balance between responsiveness and CPU usage
- Allows for slow serial data
- Configurable per use case

### Flush Buffers

On open:
```cpp
tcflush(fd_, TCIOFLUSH);
```

Ensures:
- No stale data
- Clean state
- Accurate verification
