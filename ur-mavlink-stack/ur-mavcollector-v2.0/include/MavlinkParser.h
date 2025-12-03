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
    void setBatteryInfoCallback(MavlinkUdpConnection::BatteryInfoCallback callback);
    void setBatteryStatusCallback(MavlinkUdpConnection::BatteryStatusCallback callback);
    void setGPSDataCallback(MavlinkUdpConnection::GPSDataCallback callback);
    void setSystemStatusCallback(MavlinkUdpConnection::SystemStatusCallback callback);
    
    uint8_t getDetectedMavlinkVersion() const;

private:
    mavlink_status_t m_status;
    uint8_t m_detectedVersion;
    MavlinkUdpConnection::HeartbeatCallback m_heartbeatCallback;
    MavlinkUdpConnection::AutopilotVersionCallback m_autopilotVersionCallback;
    MavlinkUdpConnection::BatteryInfoCallback m_batteryInfoCallback;
    MavlinkUdpConnection::BatteryStatusCallback m_batteryStatusCallback;
    MavlinkUdpConnection::GPSDataCallback m_gpsDataCallback;
    MavlinkGPSData m_gpsData; // Store current GPS data
    MavlinkUdpConnection::SystemStatusCallback m_systemStatusCallback;
    MavlinkSystemStatus m_systemStatus; // Store current system status
    
    void handleMessage(const mavlink_message_t& message);
    void handleHeartbeatMessage(const mavlink_message_t& message);
    void handleAutopilotVersionMessage(const mavlink_message_t& message);
    void handleBatteryInfoMessage(const mavlink_message_t& message);
    void handleBatteryStatusMessage(const mavlink_message_t& message);
    void handleGPSRawIntMessage(const mavlink_message_t& message);
    void handleGPSStatusMessage(const mavlink_message_t& message);
    void handleGPSGlobalOriginMessage(const mavlink_message_t& message);
    void handleGlobalPositionIntCovMessage(const mavlink_message_t& message);
    void handleGlobalPositionIntMessage(const mavlink_message_t& message);
    void handleGPS2RawMessage(const mavlink_message_t& message);
    void handleGPSInputMessage(const mavlink_message_t& message);
    void handleSystemStatusMessage(const mavlink_message_t& message);
    void updateGPSState();
    void notifyGPSCallback();
    void notifySystemStatusCallback();
};

#endif // MAVLINK_PARSER_H
