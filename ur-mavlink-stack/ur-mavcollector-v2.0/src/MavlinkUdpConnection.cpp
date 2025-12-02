#include "MavlinkUdpConnection.h"
#include "MavlinkParser.h"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>

// External verbose mode flag
extern bool verbose_mode;

struct MavlinkUdpConnection::Impl {
    int socket_fd;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    socklen_t remote_addr_len;
    bool connected;
    bool receiving;
    std::unique_ptr<std::thread> receive_thread;
    MavlinkParser parser;
    HeartbeatCallback heartbeat_callback;
    uint8_t mavlink_version;
    
    Impl() : socket_fd(-1), connected(false), receiving(false), mavlink_version(0) {
        remote_addr_len = sizeof(remote_addr);
        memset(&local_addr, 0, sizeof(local_addr));
        memset(&remote_addr, 0, sizeof(remote_addr));
    }
    
    ~Impl() {
        if (connected) {
            if (receiving) {
                receiving = false;
                if (receive_thread && receive_thread->joinable()) {
                    receive_thread->join();
                }
                receive_thread.reset();
            }
            
            if (socket_fd >= 0) {
                close(socket_fd);
                socket_fd = -1;
            }
            connected = false;
        }
    }
};

MavlinkUdpConnection::MavlinkUdpConnection() : pImpl(std::make_unique<Impl>()) {}

MavlinkUdpConnection::~MavlinkUdpConnection() = default;

bool MavlinkUdpConnection::connect(const std::string& address, uint16_t port) {
    if (pImpl->connected) {
        return true;
    }
    
    pImpl->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (pImpl->socket_fd < 0) {
        std::cerr << "Socket creation error: " << strerror(errno) << std::endl;
        return false;
    }
    
    memset(&pImpl->local_addr, 0, sizeof(pImpl->local_addr));
    pImpl->local_addr.sin_family = AF_INET;
    pImpl->local_addr.sin_addr.s_addr = INADDR_ANY;
    pImpl->local_addr.sin_port = htons(port);
    
    if (bind(pImpl->socket_fd, (struct sockaddr*)&pImpl->local_addr, sizeof(pImpl->local_addr)) != 0) {
        std::cerr << "Bind error: " << strerror(errno) << std::endl;
        close(pImpl->socket_fd);
        pImpl->socket_fd = -1;
        return false;
    }
    
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    if (setsockopt(pImpl->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        std::cerr << "Setsockopt error: " << strerror(errno) << std::endl;
        close(pImpl->socket_fd);
        pImpl->socket_fd = -1;
        return false;
    }
    
    pImpl->connected = true;
    pImpl->mavlink_version = 0;
    
    if (verbose_mode) {
        std::cout << "Connected to UDP " << address << ":" << port << std::endl;
    }
    return true;
}

void MavlinkUdpConnection::disconnect() {
    if (!pImpl->connected) {
        return;
    }
    
    stopReceiving();
    
    if (pImpl->socket_fd >= 0) {
        close(pImpl->socket_fd);
        pImpl->socket_fd = -1;
    }
    
    pImpl->connected = false;
    if (verbose_mode) {
        std::cout << "Disconnected" << std::endl;
    }
}

bool MavlinkUdpConnection::isConnected() const {
    return pImpl->connected;
}

void MavlinkUdpConnection::startReceiving() {
    if (!pImpl->connected || pImpl->receiving) {
        return;
    }
    
    pImpl->receiving = true;
    
    pImpl->receive_thread = std::make_unique<std::thread>([this]() {
        uint8_t buffer[2048];
        
        while (pImpl->receiving && pImpl->connected) {
            const int ret = recvfrom(
                pImpl->socket_fd, 
                buffer, 
                sizeof(buffer), 
                0, 
                (struct sockaddr*)&pImpl->remote_addr, 
                &pImpl->remote_addr_len
            );
            
            if (ret < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    if (verbose_mode) {
                        std::cerr << "Recvfrom error: " << strerror(errno) << std::endl;
                    }
                    break;
                }
                continue;
            } else if (ret == 0) {
                continue;
            }
            
            pImpl->parser.parseBytes(buffer, ret);
            pImpl->mavlink_version = pImpl->parser.getDetectedMavlinkVersion();
        }
    });
}

void MavlinkUdpConnection::stopReceiving() {
    if (!pImpl->receiving) {
        return;
    }
    
    pImpl->receiving = false;
    
    if (pImpl->receive_thread && pImpl->receive_thread->joinable()) {
        pImpl->receive_thread->join();
    }
    
    pImpl->receive_thread.reset();
}

void MavlinkUdpConnection::setHeartbeatCallback(HeartbeatCallback callback) {
    pImpl->heartbeat_callback = callback;
    pImpl->parser.setHeartbeatCallback(callback);
}

void MavlinkUdpConnection::setAutopilotVersionCallback(AutopilotVersionCallback callback) {
    pImpl->parser.setAutopilotVersionCallback(callback);
}

void MavlinkUdpConnection::sendHeartbeat() {
    if (!pImpl->connected) {
        return;
    }
    
    mavlink_message_t message;
    const uint8_t system_id = 255;  // GCS system ID
    const uint8_t component_id = MAV_COMP_ID_MISSIONPLANNER;  // Mission Planner component ID
    const uint8_t base_mode = 0;
    const uint32_t custom_mode = 0;
    
    mavlink_msg_heartbeat_pack_chan(
        system_id,
        component_id,
        MAVLINK_COMM_0,
        &message,
        MAV_TYPE_GCS,  // Ground Control Station
        MAV_AUTOPILOT_GENERIC,  // Generic autopilot
        base_mode,
        custom_mode,
        MAV_STATE_ACTIVE
    );
    
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    const int len = mavlink_msg_to_send_buffer(buffer, &message);
    
    int ret = sendto(
        pImpl->socket_fd, 
        buffer, 
        len, 
        0, 
        (const struct sockaddr*)&pImpl->remote_addr, 
        pImpl->remote_addr_len
    );
    
    if (ret != len) {
        if (verbose_mode) {
            std::cerr << "Sendto error: " << strerror(errno) << std::endl;
        }
    } else {
        if (verbose_mode) {
            std::cout << "Sent heartbeat" << std::endl;
        }
    }
}

uint8_t MavlinkUdpConnection::getMavlinkVersion() const {
    return pImpl->mavlink_version;
}

void MavlinkUdpConnection::requestAutopilotVersion() {
    if (!pImpl->connected) {
        return;
    }
    
    mavlink_message_t message;
    const uint8_t system_id = 255;  // GCS system ID
    const uint8_t component_id = MAV_COMP_ID_MISSIONPLANNER;  // Mission Planner component ID
    const uint8_t target_system = 1;  // Request from system 1
    const uint8_t target_component = 1;  // Request from component 1 (autopilot)
    
    // Use MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES as per MAVLink spec
    mavlink_msg_command_long_pack_chan(
        system_id,
        component_id,
        MAVLINK_COMM_0,
        &message,
        target_system,
        target_component,
        MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES,
        1,  // confirmation = 1
        1,  // param1 = 1 (request all capabilities)
        0,  // param2: reserved
        0,  // param3: reserved
        0,  // param4: reserved
        0,  // param5: reserved
        0,  // param6: reserved
        0   // param7: reserved
    );
    
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    const int len = mavlink_msg_to_send_buffer(buffer, &message);
    
    int ret = sendto(
        pImpl->socket_fd, 
        buffer, 
        len, 
        0, 
        (const struct sockaddr*)&pImpl->remote_addr, 
        pImpl->remote_addr_len
    );
    
    if (ret != len) {
        if (verbose_mode) {
            std::cerr << "Request autopilot version sendto error: " << strerror(errno) << std::endl;
        }
    } else {
        if (verbose_mode) {
            std::cout << "Sent AUTOPILOT_VERSION request command" << std::endl;
        }
    }
}
