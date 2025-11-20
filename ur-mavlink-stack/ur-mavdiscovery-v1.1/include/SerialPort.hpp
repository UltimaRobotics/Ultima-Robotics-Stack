
#pragma once
#include <string>
#include <vector>
#include <cstdint>

class SerialPort {
public:
    SerialPort(const std::string& devicePath);
    ~SerialPort();
    
    bool open(int baudrate);
    void close();
    bool isOpen() const;
    
    int read(uint8_t* buffer, size_t size, int timeoutMs);
    int write(const uint8_t* data, size_t size);
    
private:
    std::string devicePath_;
    int fd_;
    
    bool configurePort(int baudrate);
};
