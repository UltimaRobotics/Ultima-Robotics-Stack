#ifndef MAVLINK_UDP_CONNECTION_H
#define MAVLINK_UDP_CONNECTION_H

#include <string>
#include <functional>
#include <memory>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#include <mavlink.h>

#ifdef __cplusplus
}
#endif

struct MavlinkHeartbeatInfo {
    uint8_t system_id;
    uint8_t component_id;
    uint8_t type;
    uint8_t autopilot;
    uint8_t base_mode;
    uint32_t custom_mode;
    uint8_t system_status;
};

struct MavlinkAutopilotVersionInfo {
    uint64_t capabilities;
    uint64_t uid;
    uint32_t flight_sw_version;
    uint32_t middleware_sw_version;
    uint32_t os_sw_version;
    uint32_t board_version;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t flight_custom_version[8];
    uint8_t middleware_custom_version[8];
    uint8_t os_custom_version[8];
    uint8_t uid2[18];
    
    std::string flightSwVersionString() const;
    std::string flightCustomVersionString() const;
    std::string middlewareSwVersionString() const;
    std::string middlewareCustomVersionString() const;
    std::string osSwVersionString() const;
    std::string osCustomVersionString() const;
    std::string boardVersionString() const;
    std::string uid2String() const;
    std::string capabilitiesString() const;
};

class MavlinkUdpConnection {
public:
    using HeartbeatCallback = std::function<void(const MavlinkHeartbeatInfo&)>;
    using AutopilotVersionCallback = std::function<void(const MavlinkAutopilotVersionInfo&)>;

    MavlinkUdpConnection();
    ~MavlinkUdpConnection();

    bool connect(const std::string& address, uint16_t port);
    void disconnect();
    bool isConnected() const;

    void startReceiving();
    void stopReceiving();

    void setHeartbeatCallback(HeartbeatCallback callback);
    void setAutopilotVersionCallback(AutopilotVersionCallback callback);
    
    void sendHeartbeat();
    void requestAutopilotVersion();
    
    uint8_t getMavlinkVersion() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // MAVLINK_UDP_CONNECTION_H
