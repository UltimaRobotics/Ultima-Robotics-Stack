#ifndef MAVLINK_PARSER_H
#define MAVLINK_PARSER_H

#include <cstdint>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

#include <mavlink.h>

#ifdef __cplusplus
}
#endif

#include "MavlinkUdpConnection.h"

class MavlinkParser {
public:
    MavlinkParser();
    ~MavlinkParser();

    void parseBytes(const uint8_t* data, size_t length);
    void setHeartbeatCallback(MavlinkUdpConnection::HeartbeatCallback callback);
    void setAutopilotVersionCallback(MavlinkUdpConnection::AutopilotVersionCallback callback);
    
    uint8_t getDetectedMavlinkVersion() const;

private:
    mavlink_status_t m_status;
    uint8_t m_detectedVersion;
    MavlinkUdpConnection::HeartbeatCallback m_heartbeatCallback;
    MavlinkUdpConnection::AutopilotVersionCallback m_autopilotVersionCallback;
    
    void handleMessage(const mavlink_message_t& message);
    void handleHeartbeatMessage(const mavlink_message_t& message);
    void handleAutopilotVersionMessage(const mavlink_message_t& message);
};

#endif // MAVLINK_PARSER_H
