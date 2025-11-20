
#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <netinet/in.h>
#include "../mavlink_c_library_v2/common/mavlink.h"
#include "../mavlink_c_library_v1/common/mavlink.h"

class UdpHandler {
public:
    struct UdpPacket {
        uint8_t data[2048];
        size_t length;
        struct sockaddr_in source_addr;
    };

    using PacketCallback = std::function<void(const UdpPacket&)>;

    struct Config {
        std::string bind_address;
        int bind_port;
        std::string target_address;
        int target_port;
        int socket_timeout_ms;
        bool verbose;
    };

    explicit UdpHandler(const Config& config);
    ~UdpHandler();

    // Lifecycle management
    bool start();
    void stop();
    bool isRunning() const { return running_; }
    
    // MAVLink version detection
    int getMavlinkVersion() const { return mavlink_version_; }
    bool isMavlinkVersionDetected() const { return mavlink_version_detected_; }

    // Packet operations
    bool sendPacket(const uint8_t* data, size_t length);
    bool sendMavlinkMessage(const mavlink_message_t& msg);
    
    // Callback registration
    void setPacketCallback(PacketCallback callback);

    // Address management
    void updateTargetAddress(const struct sockaddr_in& addr);
    struct sockaddr_in getTargetAddress() const;

private:
    void receiveLoop();
    bool setupSocket();
    void closeSocket();
    void detectMavlinkVersion(const uint8_t* data, size_t length);

    Config config_;
    int udp_socket_;
    struct sockaddr_in target_addr_;
    
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> receive_thread_;
    
    PacketCallback packet_callback_;
    mutable std::mutex callback_mutex_;
    mutable std::mutex target_addr_mutex_;
    
    // MAVLink version detection
    std::atomic<int> mavlink_version_;
    std::atomic<bool> mavlink_version_detected_;
};
