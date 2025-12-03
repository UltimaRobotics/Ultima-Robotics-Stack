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

MavlinkUdpConnection::MavlinkUdpConnection(uint8_t system_id, uint8_t component_id) 
    : pImpl(std::make_unique<Impl>()), m_system_id(system_id), m_component_id(component_id) {}

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

void MavlinkUdpConnection::setBatteryInfoCallback(BatteryInfoCallback callback) {
    pImpl->parser.setBatteryInfoCallback(callback);
}

void MavlinkUdpConnection::setBatteryStatusCallback(BatteryStatusCallback callback) {
    pImpl->parser.setBatteryStatusCallback(callback);
}

void MavlinkUdpConnection::setGPSDataCallback(GPSDataCallback callback) {
    pImpl->parser.setGPSDataCallback(callback);
}

void MavlinkUdpConnection::setSystemStatusCallback(SystemStatusCallback callback) {
    pImpl->parser.setSystemStatusCallback(callback);
}

void MavlinkUdpConnection::sendHeartbeat() {
    if (!pImpl->connected) {
        return;
    }
    
    mavlink_message_t message;
    const uint8_t base_mode = 0;
    const uint32_t custom_mode = 0;
    
    mavlink_msg_heartbeat_pack_chan(
        m_system_id,
        m_component_id,
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
    const uint8_t target_system = 1;  // Request from system 1
    const uint8_t target_component = 1;  // Request from component 1 (autopilot)
    
    // Use MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES as per MAVLink spec
    mavlink_msg_command_long_pack_chan(
        m_system_id,
        m_component_id,
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

void MavlinkUdpConnection::requestBatteryInfo() {
    if (!pImpl->connected) {
        return;
    }
    
    mavlink_message_t message;
    const uint8_t target_system = 1;  // Request from system 1
    const uint8_t target_component = 1;  // Request from component 1 (autopilot)
    
    // Use MAV_CMD_REQUEST_MESSAGE to request battery info
    mavlink_msg_command_long_pack_chan(
        m_system_id,
        m_component_id,
        MAVLINK_COMM_0,
        &message,
        target_system,
        target_component,
        MAV_CMD_REQUEST_MESSAGE,
        1,  // confirmation = 1
        MAVLINK_MSG_ID_BATTERY_INFO,  // param1 = message ID to request
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
            std::cerr << "Request battery info sendto error: " << strerror(errno) << std::endl;
        }
    } else {
        if (verbose_mode) {
            std::cout << "Sent BATTERY_INFO request command" << std::endl;
        }
    }
}

void MavlinkUdpConnection::requestBatteryStatus() {
    if (!pImpl->connected) {
        return;
    }
    
    mavlink_message_t message;
    const uint8_t target_system = 1;  // Request from system 1
    const uint8_t target_component = 1;  // Request from component 1 (autopilot)
    
    // Use MAV_CMD_REQUEST_MESSAGE to request battery status
    mavlink_msg_command_long_pack_chan(
        m_system_id,
        m_component_id,
        MAVLINK_COMM_0,
        &message,
        target_system,
        target_component,
        MAV_CMD_REQUEST_MESSAGE,
        1,  // confirmation = 1
        MAVLINK_MSG_ID_BATTERY_STATUS,  // param1 = message ID to request
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
            std::cerr << "Request battery status sendto error: " << strerror(errno) << std::endl;
        }
    } else {
        if (verbose_mode) {
            std::cout << "Sent BATTERY_STATUS request command" << std::endl;
        }
    }
}

void MavlinkUdpConnection::requestGPSData(uint8_t target_system, uint8_t target_component, uint16_t message_rate_hz) {
    if (!pImpl->connected) {
        if (verbose_mode) {
            std::cerr << "Cannot request GPS data: not connected" << std::endl;
        }
        return;
    }
    
    if (verbose_mode) {
        std::cout << "Requesting GPS data streams at " << message_rate_hz << " Hz" << std::endl;
    }
    
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t msg;
    
    // Method 1: Try data stream requests first
    if (verbose_mode) {
        std::cout << "Requesting data streams..." << std::endl;
    }
    
    // Request GPS_RAW_INT stream (for basic GPS data)
    if (verbose_mode) {
        std::cout << "Requesting MAV_DATA_STREAM_RAW_SENSORS stream" << std::endl;
    }
    mavlink_msg_request_data_stream_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_DATA_STREAM_RAW_SENSORS, message_rate_hz, 1);
    
    const int len1 = mavlink_msg_to_send_buffer(buffer, &msg);
    int ret1 = sendto(pImpl->socket_fd, buffer, len1, 0, 
                     (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    if (verbose_mode) {
        std::cout << "RAW_SENSORS request sent: " << (ret1 == len1 ? "SUCCESS" : "FAILED") << std::endl;
    }
    
    // Request GPS_STATUS stream (for satellite info)
    if (verbose_mode) {
        std::cout << "Requesting MAV_DATA_STREAM_EXTENDED_STATUS stream" << std::endl;
    }
    mavlink_msg_request_data_stream_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_DATA_STREAM_EXTENDED_STATUS, message_rate_hz, 1);
    
    const int len2 = mavlink_msg_to_send_buffer(buffer, &msg);
    int ret2 = sendto(pImpl->socket_fd, buffer, len2, 0, 
                     (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    if (verbose_mode) {
        std::cout << "EXTENDED_STATUS request sent: " << (ret2 == len2 ? "SUCCESS" : "FAILED") << std::endl;
    }
    
    // Request GLOBAL_POSITION_INT stream (for position data)
    if (verbose_mode) {
        std::cout << "Requesting MAV_DATA_STREAM_POSITION stream" << std::endl;
    }
    mavlink_msg_request_data_stream_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_DATA_STREAM_POSITION, message_rate_hz, 1);
    
    const int len3 = mavlink_msg_to_send_buffer(buffer, &msg);
    int ret3 = sendto(pImpl->socket_fd, buffer, len3, 0, 
                     (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    if (verbose_mode) {
        std::cout << "POSITION request sent: " << (ret3 == len3 ? "SUCCESS" : "FAILED") << std::endl;
    }
    
    // Method 2: Try individual message requests (alternative approach)
    if (verbose_mode) {
        std::cout << "Requesting individual GPS messages..." << std::endl;
    }
    
    // Request GPS_RAW_INT message
    mavlink_msg_command_long_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_CMD_SET_MESSAGE_INTERVAL, 0, 
        MAVLINK_MSG_ID_GPS_RAW_INT, static_cast<int32_t>(1000000.0f / message_rate_hz), 0, 0, 0, 0, 0);
    
    const int len4 = mavlink_msg_to_send_buffer(buffer, &msg);
    int ret4 = sendto(pImpl->socket_fd, buffer, len4, 0, 
                     (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    if (verbose_mode) {
        std::cout << "GPS_RAW_INT request sent: " << (ret4 == len4 ? "SUCCESS" : "FAILED") << std::endl;
    }
    
    // Request GPS_STATUS message
    mavlink_msg_command_long_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_CMD_SET_MESSAGE_INTERVAL, 0, 
        MAVLINK_MSG_ID_GPS_STATUS, static_cast<int32_t>(1000000.0f / message_rate_hz), 0, 0, 0, 0, 0);
    
    const int len5 = mavlink_msg_to_send_buffer(buffer, &msg);
    int ret5 = sendto(pImpl->socket_fd, buffer, len5, 0, 
                     (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    if (verbose_mode) {
        std::cout << "GPS_STATUS request sent: " << (ret5 == len5 ? "SUCCESS" : "FAILED") << std::endl;
    }
    
    // Request GLOBAL_POSITION_INT message
    mavlink_msg_command_long_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_CMD_SET_MESSAGE_INTERVAL, 0, 
        MAVLINK_MSG_ID_GLOBAL_POSITION_INT, static_cast<int32_t>(1000000.0f / message_rate_hz), 0, 0, 0, 0, 0);
    
    const int len6 = mavlink_msg_to_send_buffer(buffer, &msg);
    int ret6 = sendto(pImpl->socket_fd, buffer, len6, 0, 
                     (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    if (verbose_mode) {
        std::cout << "GLOBAL_POSITION_INT request sent: " << (ret6 == len6 ? "SUCCESS" : "FAILED") << std::endl;
    }
}

void MavlinkUdpConnection::stopGPSData(uint8_t target_system, uint8_t target_component) {
    if (!pImpl->connected) {
        return;
    }
    
    if (verbose_mode) {
        std::cout << "Stopping GPS data streams" << std::endl;
    }
    
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t msg;
    
    // Stop all GPS-related streams
    mavlink_msg_request_data_stream_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_DATA_STREAM_RAW_SENSORS, 0, 0);
    
    const int len1 = mavlink_msg_to_send_buffer(buffer, &msg);
    sendto(pImpl->socket_fd, buffer, len1, 0, 
          (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    mavlink_msg_request_data_stream_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_DATA_STREAM_EXTENDED_STATUS, 0, 0);
    
    const int len2 = mavlink_msg_to_send_buffer(buffer, &msg);
    sendto(pImpl->socket_fd, buffer, len2, 0, 
          (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    mavlink_msg_request_data_stream_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_DATA_STREAM_POSITION, 0, 0);
    
    const int len3 = mavlink_msg_to_send_buffer(buffer, &msg);
    sendto(pImpl->socket_fd, buffer, len3, 0, 
          (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
}

void MavlinkUdpConnection::requestSystemStatus(uint8_t target_system, uint8_t target_component, uint16_t message_rate_hz) {
    if (!pImpl->connected) {
        if (verbose_mode) {
            std::cerr << "Cannot request system status: not connected" << std::endl;
        }
        return;
    }
    
    if (verbose_mode) {
        std::cout << "Requesting system status data at " << message_rate_hz << " Hz" << std::endl;
    }
    
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t msg;
    
    // Request SYS_STATUS message using message interval command
    mavlink_msg_command_long_pack_chan(
        m_system_id, m_component_id, MAVLINK_COMM_0, &msg,
        target_system, target_component,
        MAV_CMD_SET_MESSAGE_INTERVAL, 0, 
        MAVLINK_MSG_ID_SYS_STATUS, static_cast<int32_t>(1000000.0f / message_rate_hz), 0, 0, 0, 0, 0);
    
    const int len = mavlink_msg_to_send_buffer(buffer, &msg);
    int ret = sendto(pImpl->socket_fd, buffer, len, 0, 
                    (const struct sockaddr*)&pImpl->remote_addr, pImpl->remote_addr_len);
    
    if (verbose_mode) {
        std::cout << "SYS_STATUS request sent: " << (ret == len ? "SUCCESS" : "FAILED") << std::endl;
    }
}
