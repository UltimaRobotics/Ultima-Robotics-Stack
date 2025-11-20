
#include "SerialPort.hpp"
#include "Logger.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>

SerialPort::SerialPort(const std::string& devicePath) 
    : devicePath_(devicePath), fd_(-1) {}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(int baudrate) {
    if (fd_ >= 0) {
        close();
    }
    
    fd_ = ::open(devicePath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        LOG_ERROR("Failed to open " + devicePath_ + ": " + std::string(strerror(errno)));
        return false;
    }
    
    if (!configurePort(baudrate)) {
        close();
        return false;
    }
    
    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::isOpen() const {
    return fd_ >= 0;
}

int SerialPort::read(uint8_t* buffer, size_t size, int timeoutMs) {
    if (fd_ < 0) return -1;
    
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);
    FD_SET(fd_, &readfds);
    
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    
    int ret = select(fd_ + 1, &readfds, nullptr, nullptr, &timeout);
    if (ret < 0) {
        return -1;
    } else if (ret == 0) {
        return 0;
    }
    
    return ::read(fd_, buffer, size);
}

int SerialPort::write(const uint8_t* data, size_t size) {
    if (fd_ < 0) return -1;
    return ::write(fd_, data, size);
}

bool SerialPort::configurePort(int baudrate) {
    struct termios tty;
    
    if (tcgetattr(fd_, &tty) != 0) {
        LOG_ERROR("tcgetattr failed: " + std::string(strerror(errno)));
        return false;
    }
    
    speed_t speed;
    switch (baudrate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 500000: speed = B500000; break;
        case 576000: speed = B576000; break;
        case 921600: speed = B921600; break;
        case 1000000: speed = B1000000; break;
        case 1152000: speed = B1152000; break;
        case 1500000: speed = B1500000; break;
        case 2000000: speed = B2000000; break;
        default:
            LOG_ERROR("Unsupported baudrate: " + std::to_string(baudrate));
            return false;
    }
    
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;
    
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;
    
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        LOG_ERROR("tcsetattr failed: " + std::string(strerror(errno)));
        return false;
    }
    
    tcflush(fd_, TCIOFLUSH);
    
    return true;
}
