
#include "../include/UdpHandler.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

UdpHandler::UdpHandler(const Config& config)
    : config_(config), udp_socket_(-1), running_(false), 
      mavlink_version_(0), mavlink_version_detected_(false) {
    memset(&target_addr_, 0, sizeof(target_addr_));
    target_addr_.sin_family = AF_INET;
    target_addr_.sin_port = htons(config_.target_port);
    inet_pton(AF_INET, config_.target_address.c_str(), &target_addr_.sin_addr);
}

UdpHandler::~UdpHandler() {
    stop();
}

bool UdpHandler::start() {
    if (running_) {
        std::cerr << "[UdpHandler] Already running" << std::endl;
        return false;
    }

    if (!setupSocket()) {
        std::cerr << "[UdpHandler] Failed to setup socket" << std::endl;
        return false;
    }

    running_ = true;
    receive_thread_ = std::make_unique<std::thread>(&UdpHandler::receiveLoop, this);

    if (config_.verbose) {
        std::cout << "[UdpHandler] Started successfully" << std::endl;
    }

    return true;
}

void UdpHandler::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (receive_thread_ && receive_thread_->joinable()) {
        receive_thread_->join();
    }

    closeSocket();

    if (config_.verbose) {
        std::cout << "[UdpHandler] Stopped" << std::endl;
    }
}

bool UdpHandler::setupSocket() {
    udp_socket_ = socket(PF_INET, SOCK_DGRAM, 0);
    if (udp_socket_ < 0) {
        std::cerr << "[UdpHandler] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    // Bind to receive on specified address/port
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(config_.bind_port);
    
    if (config_.bind_address == "0.0.0.0" || config_.bind_address.empty()) {
        local_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, config_.bind_address.c_str(), &local_addr.sin_addr);
    }

    if (bind(udp_socket_, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        std::cerr << "[UdpHandler] Failed to bind socket: " << strerror(errno) << std::endl;
        close(udp_socket_);
        udp_socket_ = -1;
        return false;
    }

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = config_.socket_timeout_ms / 1000;
    tv.tv_usec = (config_.socket_timeout_ms % 1000) * 1000;
    if (setsockopt(udp_socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        std::cerr << "[UdpHandler] Failed to set socket timeout: " << strerror(errno) << std::endl;
        close(udp_socket_);
        udp_socket_ = -1;
        return false;
    }

    if (config_.verbose) {
        std::cout << "[UdpHandler] Socket bound to " << config_.bind_address 
                  << ":" << config_.bind_port << std::endl;
    }

    return true;
}

void UdpHandler::closeSocket() {
    if (udp_socket_ >= 0) {
        close(udp_socket_);
        udp_socket_ = -1;
    }
}

void UdpHandler::detectMavlinkVersion(const uint8_t* data, size_t length) {
    if (length < 1 || mavlink_version_detected_) {
        return;
    }

    // Check for MAVLink magic bytes
    // MAVLink v1: 0xFE (254)
    // MAVLink v2: 0xFD (253)
    if (data[0] == MAVLINK_STX_MAVLINK1 || data[0] == 0xFE) {
        mavlink_version_ = 1;
        mavlink_version_detected_ = true;
        std::cout << "[UdpHandler] ✓ MAVLink v1 detected (STX: 0xFE)" << std::endl;
        std::cout << "[UdpHandler]   Using REQUEST_DATA_STREAM for high frequency data" << std::endl;
    } else if (data[0] == MAVLINK_STX || data[0] == 0xFD) {
        mavlink_version_ = 2;
        mavlink_version_detected_ = true;
        std::cout << "[UdpHandler] ✓ MAVLink v2 detected (STX: 0xFD)" << std::endl;
        std::cout << "[UdpHandler]   Using SET_MESSAGE_INTERVAL for high frequency data" << std::endl;
    } else {
        if (config_.verbose) {
            std::cout << "[UdpHandler] Unknown packet format, STX: 0x" 
                      << std::hex << (int)data[0] << std::dec << std::endl;
        }
    }
}

void UdpHandler::receiveLoop() {
    if (config_.verbose) {
        std::cout << "[UdpHandler] Receive loop started" << std::endl;
    }

    while (running_) {
        UdpPacket packet;
        socklen_t src_len = sizeof(packet.source_addr);

        ssize_t received = recvfrom(udp_socket_, packet.data, sizeof(packet.data), 0,
                                   (struct sockaddr*)&packet.source_addr, &src_len);

        if (received > 0) {
            packet.length = received;

            if (config_.verbose) {
                char src_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &packet.source_addr.sin_addr, src_ip, INET_ADDRSTRLEN);
                std::cout << "[UdpHandler] Received " << received << " bytes from " 
                          << src_ip << ":" << ntohs(packet.source_addr.sin_port) << std::endl;
            }

            // Detect MAVLink version on first packet
            if (!mavlink_version_detected_) {
                detectMavlinkVersion(packet.data, packet.length);
            }

            // Invoke callback if registered
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (packet_callback_) {
                    packet_callback_(packet);
                }
            }
        } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "[UdpHandler] recvfrom error: " << strerror(errno) << std::endl;
        }
    }

    if (config_.verbose) {
        std::cout << "[UdpHandler] Receive loop stopped" << std::endl;
    }
}

bool UdpHandler::sendPacket(const uint8_t* data, size_t length) {
    std::lock_guard<std::mutex> lock(target_addr_mutex_);
    
    ssize_t sent = sendto(udp_socket_, data, length, 0, 
                         (struct sockaddr*)&target_addr_, sizeof(target_addr_));

    if (sent != static_cast<ssize_t>(length)) {
        std::cerr << "[UdpHandler] sendto error: " << strerror(errno) << std::endl;
        return false;
    }

    if (config_.verbose) {
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &target_addr_.sin_addr, dst_ip, INET_ADDRSTRLEN);
        std::cout << "[UdpHandler] Sent " << length << " bytes to " 
                  << dst_ip << ":" << ntohs(target_addr_.sin_port) << std::endl;
    }

    return true;
}

bool UdpHandler::sendMavlinkMessage(const mavlink_message_t& msg) {
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    return sendPacket(buffer, len);
}

void UdpHandler::setPacketCallback(PacketCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    packet_callback_ = callback;
}

void UdpHandler::updateTargetAddress(const struct sockaddr_in& addr) {
    std::lock_guard<std::mutex> lock(target_addr_mutex_);
    target_addr_ = addr;

    if (config_.verbose) {
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, dst_ip, INET_ADDRSTRLEN);
        std::cout << "[UdpHandler] Target address updated to " 
                  << dst_ip << ":" << ntohs(addr.sin_port) << std::endl;
    }
}

struct sockaddr_in UdpHandler::getTargetAddress() const {
    std::lock_guard<std::mutex> lock(target_addr_mutex_);
    return target_addr_;
}
