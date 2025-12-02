#include "MAVLinkUdpConnection.h"
#include "Vehicle.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <iostream>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <cstdio>

MAVLinkUdpConnection::MAVLinkUdpConnection()
    : _socketFd(-1)
    , _port(0)
{
    // Initialize MAVLink status
    memset(_mavlinkStatus, 0, sizeof(_mavlinkStatus));
    
    // Initialize connection health monitoring with configurable defaults
    _lastMessageTime.store(std::chrono::steady_clock::now());
    _restartCount.store(0);
    _restartInProgress.store(false);
    
    // Note: Default values are now set by the caller (main.cpp)
    // This ensures no hardcoded connection parameters in the binary
}

MAVLinkUdpConnection::~MAVLinkUdpConnection()
{
    disconnect();
}

bool MAVLinkUdpConnection::connect(const std::string &targetAddress, uint16_t port)
{
    std::lock_guard<std::mutex> lock(_socketMutex);
    
    if (_connected) {
        disconnect();
    }
    
    _targetAddress = targetAddress;
    _port = port;
    
    // Create UDP socket
    _socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_socketFd < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set socket timeout (like the working C example)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms timeout
    if (setsockopt(_socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        std::cerr << "Failed to set socket timeout: " << strerror(errno) << std::endl;
        close(_socketFd);
        _socketFd = -1;
        return false;
    }
    
    // Bind to local port to listen for MAVLink data (like the C example)
    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    localAddr.sin_port = htons(_port);
    
    if (bind(_socketFd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        std::cerr << "Failed to bind to port " << _port << ": " << strerror(errno) << std::endl;
        close(_socketFd);
        _socketFd = -1;
        return false;
    }
    
    // DON'T connect the socket - we want to receive from any sender
    // The original code was connecting which filters packets!
    
    _connected = true;
    _running = true;
    
    // Start receive thread
    _receiveThread = std::thread(&MAVLinkUdpConnection::_receiveThreadFunc, this);
    
    // Start heartbeat thread
    _sendHeartbeatFlag = true;
    _heartbeatThread = std::thread([this]() {
        while (_sendHeartbeatFlag && _running) {
            _sendHeartbeatFunc();
            std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_INTERVAL_MS));
        }
    });
    
    // Start health check thread if enabled
    if (_healthCheckEnabled) {
        _healthCheckThread = std::thread(&MAVLinkUdpConnection::_healthCheckThreadFunc, this);
    }
    
    if (_connectionChangedCallback) {
        _connectionChangedCallback(true);
    }
    
    return true;
}

void MAVLinkUdpConnection::disconnect()
{
    {
        std::lock_guard<std::mutex> lock(_socketMutex);
        if (!_connected) {
            return;
        }
        
        _running = false;
        _sendHeartbeatFlag = false;
        _connected = false;
    }
    
    // Wait for threads to finish
    if (_receiveThread.joinable()) {
        _receiveThread.join();
    }
    
    if (_heartbeatThread.joinable()) {
        _heartbeatThread.join();
    }
    
    if (_healthCheckThread.joinable()) {
        _healthCheckThread.join();
    }
    
    // Close socket
    if (_socketFd >= 0) {
        close(_socketFd);
        _socketFd = -1;
    }
    
    if (_connectionChangedCallback) {
        _connectionChangedCallback(false);
    }
}

bool MAVLinkUdpConnection::sendMessage(const mavlink_message_t &message)
{
    return sendMessage(message, 0, 0); // Broadcast to all
}

bool MAVLinkUdpConnection::sendMessage(const mavlink_message_t &message, uint8_t systemId, uint8_t componentId)
{
    std::lock_guard<std::mutex> lock(_socketMutex);
    
    if (!_connected || _socketFd < 0 || !_haveSenderAddr) {
        return false;
    }
    
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    uint16_t len;
    
    if (_detectedMavlinkVersion == 1) {
        len = mavlink_msg_to_send_buffer(buffer, &message);
    } else {
        len = mavlink_msg_to_send_buffer(buffer, &message);
    }
    
    if (len > 0) {
        // Use sendto() to send back to the last sender (like the C example)
        ssize_t sent = sendto(_socketFd, buffer, len, 0, 
                             (struct sockaddr*)&_lastSenderAddr, _lastSenderAddrLen);
        if (sent > 0) {
            _bytesSent += sent;
            _packetsSent++;
            return true;
        } else {
            _packetsLost++;
            return false;
        }
    }
    
    return false;
}

void MAVLinkUdpConnection::resetStatistics()
{
    _bytesReceived = 0;
    _bytesSent = 0;
    _packetsReceived = 0;
    _packetsSent = 0;
    _packetsLost = 0;
}

void MAVLinkUdpConnection::_receiveThreadFunc()
{
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);
    
    while (_running) {
        // Receive data with timeout (like the C example)
        ssize_t received = recvfrom(_socketFd, buffer, sizeof(buffer), 0, 
                                  (struct sockaddr*)&senderAddr, &senderAddrLen);
        
        if (received > 0) {
            _bytesReceived += received;
            _packetsReceived++;
            
            char senderIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &senderAddr.sin_addr, senderIp, INET_ADDRSTRLEN);
            uint16_t senderPort = ntohs(senderAddr.sin_port);
            
            // Store sender address for sending responses
            _lastSenderAddr = senderAddr;
            _lastSenderAddrLen = senderAddrLen;
            _haveSenderAddr = true;
            
            _processReceivedData(buffer, received, senderIp, senderPort);
        } else if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout, continue loop (this is expected behavior)
                continue;
            } else {
                // Real error occurred
                std::cerr << "Receive error: " << strerror(errno) << std::endl;
                break;
            }
        }
        // received == 0 means timeout, continue loop
    }
}

void MAVLinkUdpConnection::_processReceivedData(const uint8_t* data, size_t length, const std::string& senderIp, uint16_t senderPort)
{
    // Process data silently for efficient data collection
    if (_parseMavlinkData(data, length)) {
        // Successfully parsed MAVLink message(s)
    }
}

bool MAVLinkUdpConnection::_parseMavlinkData(const uint8_t *data, size_t length)
{
    bool parsedMessage = false;
    mavlink_message_t message;
    
    // Parse all bytes in the packet - there can be multiple messages per packet
    for (size_t i = 0; i < length; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, data[i], &message, &_mavlinkStatus[MAVLINK_COMM_0]) == MAVLINK_FRAMING_OK) {
            parsedMessage = true;
            
            // Update last message time for health monitoring
            _updateLastMessageTime();
            
            // Update message loss detection
            _updateMessageLossStats(message);
            
            if (_autoVersionDetection) {
                _detectMavlinkVersion(message);
            }
            
            if (_messageReceivedCallback) {
                _messageReceivedCallback(message);
            }
            
            if (_vehicle) {
                _vehicle->handleMessage(message);
            }
        }
    }
    
    return parsedMessage;
}

void MAVLinkUdpConnection::_detectMavlinkVersion(const mavlink_message_t &message)
{
    // Simple version detection based on message magic field
    // MAVLink v1.0 uses STX (0xFE), v2.0 uses STX (0xFD)
    if (message.magic == 0xFE) {
        _detectedMavlinkVersion = 1;
    } else {
        _detectedMavlinkVersion = 2;
    }
    
    // Update protocol version if this is the first message detected
    static bool firstMessageDetected = false;
    if (!firstMessageDetected) {
        firstMessageDetected = true;
        std::cout << "Detected MAVLink v" << _detectedMavlinkVersion << std::endl;
    }
}

void MAVLinkUdpConnection::_updateMessageLossStats(const mavlink_message_t &message)
{
    // Track sequence numbers to detect message loss
    const auto key = std::make_pair(message.sysid, message.compid);
    
    uint8_t expectedSeq;
    if (!_firstMessageSeen.contains(key)) {
        _firstMessageSeen.insert(key);
        expectedSeq = message.seq;
    } else {
        expectedSeq = _lastSequence[key] + 1;
    }
    
    uint64_t lostMessages;
    if (message.seq >= expectedSeq) {
        lostMessages = message.seq - expectedSeq;
    } else {
        // Handle sequence number wrap-around
        lostMessages = static_cast<uint64_t>(message.seq) + 256ULL - expectedSeq;
    }
    
    _totalLossCounter += lostMessages;
    _lastSequence[key] = message.seq;
    
    // Update loss percentage
    uint64_t totalSent = _packetsReceived + _totalLossCounter;
    if (totalSent > 0) {
        _runningLossPercent = (static_cast<double>(_totalLossCounter) / totalSent) * 100.0f;
    }
}

void MAVLinkUdpConnection::_sendHeartbeatFunc()
{
    if (!_connected || !_sendHeartbeatFlag) {
        return;
    }
    
    mavlink_message_t message;
    mavlink_msg_heartbeat_pack(_systemId, _componentId, &message, 
                               MAV_TYPE_GCS, MAV_AUTOPILOT_GENERIC, 
                               MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 0, 
                               MAV_STATE_ACTIVE);
    
    sendMessage(message);
}

// Connection health monitoring methods
bool MAVLinkUdpConnection::isConnectionHealthy() const
{
    if (!_connected || !_healthCheckEnabled) {
        return _connected;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto lastMessage = _lastMessageTime.load();
    auto timeSinceLastMessage = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMessage).count();
    
    return timeSinceLastMessage < static_cast<int64_t>(_connectionTimeoutMs.load());
}

uint32_t MAVLinkUdpConnection::getTimeSinceLastMessage() const
{
    auto now = std::chrono::steady_clock::now();
    auto lastMessage = _lastMessageTime.load();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMessage).count());
}

void MAVLinkUdpConnection::_updateLastMessageTime()
{
    _lastMessageTime.store(std::chrono::steady_clock::now());
}

void MAVLinkUdpConnection::_healthCheckThreadFunc()
{
    std::cout << "[HEALTH] Connection health monitoring started" << std::endl;
    std::cout << "[HEALTH] Timeout: " << _connectionTimeoutMs.load() << "ms, Auto-restart: " 
              << (_autoRestartEnabled.load() ? "enabled" : "disabled") << std::endl;
    
    while (_running && _healthCheckEnabled) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Check every second
        
        if (!_connected || _restartInProgress.load()) {
            continue;
        }
        
        auto timeSinceLastMessage = getTimeSinceLastMessage();
        
        if (timeSinceLastMessage > _connectionTimeoutMs.load()) {
            std::cout << "[HEALTH] Connection unhealthy - no data for " << timeSinceLastMessage 
                      << "ms (timeout: " << _connectionTimeoutMs.load() << "ms)" << std::endl;
            
            if (_autoRestartEnabled.load()) {
                std::cout << "[HEALTH] Initiating connection restart..." << std::endl;
                _restartConnection();
            }
        }
    }
    
    std::cout << "[HEALTH] Connection health monitoring stopped" << std::endl;
}

void MAVLinkUdpConnection::_restartConnection()
{
    if (_restartInProgress.exchange(true)) {
        return; // Restart already in progress
    }
    
    std::string targetAddress = _targetAddress;
    uint16_t port = _port;
    
    // Disconnect current connection
    std::cout << "[RESTART] Disconnecting current connection..." << std::endl;
    {
        std::lock_guard<std::mutex> lock(_socketMutex);
        _running = false;
        _sendHeartbeatFlag = false;
        _connected = false;
    }
    
    // Wait for threads to finish
    if (_receiveThread.joinable()) {
        _receiveThread.join();
    }
    if (_heartbeatThread.joinable()) {
        _heartbeatThread.join();
    }
    if (_healthCheckThread.joinable()) {
        _healthCheckThread.join();
    }
    
    // Close socket
    if (_socketFd >= 0) {
        close(_socketFd);
        _socketFd = -1;
    }
    
    // Notify about connection loss
    if (_connectionChangedCallback) {
        _connectionChangedCallback(false);
    }
    
    // Wait before reconnecting
    std::cout << "[RESTART] Waiting " << _autoRestartDelayMs.load() 
              << "ms before reconnecting..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(_autoRestartDelayMs.load()));
    
    // Reconnect
    std::cout << "[RESTART] Reconnecting to " << targetAddress << ":" << port << "..." << std::endl;
    
    if (connect(targetAddress, port)) {
        _restartCount++;
        std::cout << "[RESTART] Connection restarted successfully (restart #" 
                  << _restartCount.load() << ")" << std::endl;
    } else {
        std::cout << "[RESTART] Failed to restart connection" << std::endl;
    }
    
    _restartInProgress.store(false);
}
