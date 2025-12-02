#include "MavlinkParser.h"
#include <iostream>
#include <sstream>
#include <iomanip>

// External verbose mode flag
extern bool verbose_mode;

MavlinkParser::MavlinkParser() {
    mavlink_status_t* status_ptr = &m_status;
    memset(status_ptr, 0, sizeof(mavlink_status_t));
    m_detectedVersion = 0;
}

MavlinkParser::~MavlinkParser() = default;

void MavlinkParser::parseBytes(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        mavlink_message_t message;
        mavlink_status_t status;
        
        if (mavlink_parse_char(MAVLINK_COMM_0, data[i], &message, &status) == MAVLINK_FRAMING_OK) {
            if (m_detectedVersion == 0) {
                m_detectedVersion = (status.flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1) ? 1 : 2;
                if (verbose_mode) {
                    std::cout << "Detected MAVLink version: " << (int)m_detectedVersion << std::endl;
                }
            }
            handleMessage(message);
        }
    }
}

void MavlinkParser::setHeartbeatCallback(MavlinkUdpConnection::HeartbeatCallback callback) {
    m_heartbeatCallback = callback;
}

void MavlinkParser::setAutopilotVersionCallback(MavlinkUdpConnection::AutopilotVersionCallback callback) {
    m_autopilotVersionCallback = callback;
}

uint8_t MavlinkParser::getDetectedMavlinkVersion() const {
    return m_detectedVersion;
}

void MavlinkParser::handleMessage(const mavlink_message_t& message) {
    switch (message.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT:
            handleHeartbeatMessage(message);
            break;
        case MAVLINK_MSG_ID_AUTOPILOT_VERSION:
            handleAutopilotVersionMessage(message);
            break;
        default:
            break;
    }
}

void MavlinkParser::handleHeartbeatMessage(const mavlink_message_t& message) {
    // Ignore our own heartbeat (system_id 255) and only process vehicle heartbeats
    if (message.sysid == 255) {
        return;
    }
    
    // Filter duplicate messages using sequence number and system/component ID
    static uint8_t last_seq = 255;
    static uint8_t last_sysid = 0;
    static uint8_t last_compid = 0;
    
    if (message.seq == last_seq && 
        message.sysid == last_sysid && 
        message.compid == last_compid) {
        // This is a duplicate message, ignore it
        return;
    }
    
    last_seq = message.seq;
    last_sysid = message.sysid;
    last_compid = message.compid;
    
    if (m_heartbeatCallback) {
        mavlink_heartbeat_t heartbeat;
        mavlink_msg_heartbeat_decode(&message, &heartbeat);
        
        MavlinkHeartbeatInfo info;
        info.system_id = message.sysid;
        info.component_id = message.compid;
        info.type = heartbeat.type;
        info.autopilot = heartbeat.autopilot;
        info.base_mode = heartbeat.base_mode;
        info.custom_mode = heartbeat.custom_mode;
        info.system_status = heartbeat.system_status;
        
        m_heartbeatCallback(info);
    }
}

void MavlinkParser::handleAutopilotVersionMessage(const mavlink_message_t& message) {
    // Filter duplicate messages using sequence number and system/component ID
    static uint8_t last_seq = 255;
    static uint8_t last_sysid = 0;
    static uint8_t last_compid = 0;
    
    if (message.seq == last_seq && 
        message.sysid == last_sysid && 
        message.compid == last_compid) {
        // This is a duplicate message, ignore it
        return;
    }
    
    last_seq = message.seq;
    last_sysid = message.sysid;
    last_compid = message.compid;
    
    if (m_autopilotVersionCallback) {
        mavlink_autopilot_version_t version;
        mavlink_msg_autopilot_version_decode(&message, &version);
        
        MavlinkAutopilotVersionInfo info;
        info.capabilities = version.capabilities;
        info.uid = version.uid;
        info.flight_sw_version = version.flight_sw_version;
        info.middleware_sw_version = version.middleware_sw_version;
        info.os_sw_version = version.os_sw_version;
        info.board_version = version.board_version;
        info.vendor_id = version.vendor_id;
        info.product_id = version.product_id;
        memcpy(info.flight_custom_version, version.flight_custom_version, 8);
        memcpy(info.middleware_custom_version, version.middleware_custom_version, 8);
        memcpy(info.os_custom_version, version.os_custom_version, 8);
        memcpy(info.uid2, version.uid2, 18);
        
        m_autopilotVersionCallback(info);
    }
}
