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

void MavlinkParser::setBatteryInfoCallback(MavlinkUdpConnection::BatteryInfoCallback callback) {
    m_batteryInfoCallback = callback;
}

void MavlinkParser::setBatteryStatusCallback(MavlinkUdpConnection::BatteryStatusCallback callback) {
    m_batteryStatusCallback = callback;
}

void MavlinkParser::setGPSDataCallback(MavlinkUdpConnection::GPSDataCallback callback) {
    m_gpsDataCallback = callback;
}

void MavlinkParser::setSystemStatusCallback(MavlinkUdpConnection::SystemStatusCallback callback) {
    m_systemStatusCallback = callback;
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
        case MAVLINK_MSG_ID_SYS_STATUS:
            handleSystemStatusMessage(message);
            break;
        case MAVLINK_MSG_ID_BATTERY_INFO:
            handleBatteryInfoMessage(message);
            break;
        case MAVLINK_MSG_ID_BATTERY_STATUS:
            handleBatteryStatusMessage(message);
            break;
        case MAVLINK_MSG_ID_GPS_RAW_INT:
            handleGPSRawIntMessage(message);
            break;
        case MAVLINK_MSG_ID_GPS_STATUS:
            handleGPSStatusMessage(message);
            break;
        case MAVLINK_MSG_ID_GPS_GLOBAL_ORIGIN:
            handleGPSGlobalOriginMessage(message);
            break;
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT_COV:
            handleGlobalPositionIntCovMessage(message);
            break;
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
            handleGlobalPositionIntMessage(message);
            break;
        case MAVLINK_MSG_ID_GPS2_RAW:
            handleGPS2RawMessage(message);
            break;
        case MAVLINK_MSG_ID_GPS_INPUT:
            handleGPSInputMessage(message);
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

void MavlinkParser::handleBatteryInfoMessage(const mavlink_message_t& message) {
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
    
    if (m_batteryInfoCallback) {
        mavlink_battery_info_t battery_info;
        mavlink_msg_battery_info_decode(&message, &battery_info);
        
        MavlinkBatteryInfo info;
        info.id = battery_info.id;
        info.battery_function = battery_info.battery_function;
        info.type = battery_info.type;
        info.state_of_health = battery_info.state_of_health;
        info.cells_in_series = battery_info.cells_in_series;
        info.cycle_count = battery_info.cycle_count;
        info.weight = battery_info.weight;
        info.discharge_minimum_voltage = battery_info.discharge_minimum_voltage;
        info.charging_minimum_voltage = battery_info.charging_minimum_voltage;
        info.resting_minimum_voltage = battery_info.resting_minimum_voltage;
        info.charging_maximum_voltage = battery_info.charging_maximum_voltage;
        info.charging_maximum_current = battery_info.charging_maximum_current;
        info.nominal_voltage = battery_info.nominal_voltage;
        info.discharge_maximum_current = battery_info.discharge_maximum_current;
        info.discharge_maximum_burst_current = battery_info.discharge_maximum_burst_current;
        info.design_capacity = battery_info.design_capacity;
        info.full_charge_capacity = battery_info.full_charge_capacity;
        memcpy(info.manufacture_date, battery_info.manufacture_date, 9);
        memcpy(info.serial_number, battery_info.serial_number, 32);
        memcpy(info.name, battery_info.name, 50);
        
        m_batteryInfoCallback(info);
    }
}

void MavlinkParser::handleBatteryStatusMessage(const mavlink_message_t& message) {
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
    
    if (m_batteryStatusCallback) {
        mavlink_battery_status_t battery_status;
        mavlink_msg_battery_status_decode(&message, &battery_status);
        
        MavlinkBatteryStatus status;
        status.id = battery_status.id;
        status.battery_function = battery_status.battery_function;
        status.type = battery_status.type;
        status.temperature = battery_status.temperature;
        memcpy(status.voltages, battery_status.voltages, sizeof(uint16_t) * 10);
        status.current_battery = battery_status.current_battery;
        status.current_consumed = battery_status.current_consumed;
        status.energy_consumed = battery_status.energy_consumed;
        status.battery_remaining = battery_status.battery_remaining;
        status.time_remaining = battery_status.time_remaining;
        status.charge_state = battery_status.charge_state;
        memcpy(status.voltages_ext, battery_status.voltages_ext, sizeof(uint16_t) * 4);
        status.mode = battery_status.mode;
        status.fault_bitmask = battery_status.fault_bitmask;
        
        m_batteryStatusCallback(status);
    }
}

void MavlinkParser::handleGPSRawIntMessage(const mavlink_message_t& message) {
    if (verbose_mode) {
        std::cout << "GPS_RAW_INT message received" << std::endl;
    }
    
    if (m_gpsDataCallback) {
        mavlink_gps_raw_int_t gps_raw;
        mavlink_msg_gps_raw_int_decode(&message, &gps_raw);
        
        // Update GPS state based on fix type
        m_gpsData.fix_type = static_cast<GPSFixType>(gps_raw.fix_type);
        
        // Update position if we have a fix
        if (gps_raw.fix_type >= 2) { // 2D fix or better
            m_gpsData.latitude = gps_raw.lat;
            m_gpsData.longitude = gps_raw.lon;
            m_gpsData.altitude = gps_raw.alt;
            m_gpsData.has_position = true;
            m_gpsData.position_time_usec = gps_raw.time_usec;
        }
        
        // Update satellite count
        m_gpsData.satellites_visible = gps_raw.satellites_visible;
        
        // Update satellites_used based on fix type if we don't have detailed satellite data
        if (gps_raw.fix_type >= 2 && m_gpsData.satellites_used == 0) {
            if (gps_raw.fix_type >= 3) {
                m_gpsData.satellites_used = std::min(static_cast<uint8_t>(4), gps_raw.satellites_visible);
            } else if (gps_raw.fix_type >= 2) {
                m_gpsData.satellites_used = std::min(static_cast<uint8_t>(3), gps_raw.satellites_visible);
            }
        }
        
        m_gpsData.last_update = std::chrono::steady_clock::now();
        updateGPSState();
        notifyGPSCallback();
    }
}

void MavlinkParser::handleGPSStatusMessage(const mavlink_message_t& message) {
    if (verbose_mode) {
        std::cout << "GPS_STATUS message received" << std::endl;
    }
    
    if (m_gpsDataCallback) {
        mavlink_gps_status_t gps_status;
        mavlink_msg_gps_status_decode(&message, &gps_status);
        
        m_gpsData.satellites_visible = gps_status.satellites_visible;
        m_gpsData.satellites_used = 0;
        
        // Process satellite information
        for (int i = 0; i < 20 && i < gps_status.satellites_visible; ++i) {
            m_gpsData.satellites[i].prn = gps_status.satellite_prn[i];
            m_gpsData.satellites[i].used = gps_status.satellite_used[i];
            m_gpsData.satellites[i].elevation = gps_status.satellite_elevation[i];
            m_gpsData.satellites[i].azimuth = gps_status.satellite_azimuth[i];
            m_gpsData.satellites[i].snr = gps_status.satellite_snr[i];
            
            if (gps_status.satellite_used[i]) {
                m_gpsData.satellites_used++;
            }
        }
        
        m_gpsData.last_update = std::chrono::steady_clock::now();
        notifyGPSCallback();
    }
}

void MavlinkParser::handleGPSGlobalOriginMessage(const mavlink_message_t& message) {
    if (verbose_mode) {
        std::cout << "GPS_GLOBAL_ORIGIN message received" << std::endl;
    }
    
    if (m_gpsDataCallback) {
        mavlink_gps_global_origin_t gps_origin;
        mavlink_msg_gps_global_origin_decode(&message, &gps_origin);
        
        m_gpsData.origin_latitude = gps_origin.latitude;
        m_gpsData.origin_longitude = gps_origin.longitude;
        m_gpsData.origin_altitude = gps_origin.altitude;
        m_gpsData.origin_time_usec = gps_origin.time_usec;
        m_gpsData.has_origin = true;
        
        m_gpsData.last_update = std::chrono::steady_clock::now();
    }
}

void MavlinkParser::handleGlobalPositionIntCovMessage(const mavlink_message_t& message) {
    if (verbose_mode) {
        std::cout << "GLOBAL_POSITION_INT_COV message received" << std::endl;
    }
    
    if (m_gpsDataCallback) {
        mavlink_global_position_int_cov_t global_pos;
        mavlink_msg_global_position_int_cov_decode(&message, &global_pos);
        
        m_gpsData.position_time_usec = global_pos.time_usec;
        m_gpsData.latitude = global_pos.lat;
        m_gpsData.longitude = global_pos.lon;
        m_gpsData.altitude = global_pos.alt;
        m_gpsData.relative_altitude = global_pos.relative_alt;
        m_gpsData.velocity_x = global_pos.vx / 100.0f; // Convert cm/s to m/s
        m_gpsData.velocity_y = global_pos.vy / 100.0f; // Convert cm/s to m/s
        m_gpsData.velocity_z = global_pos.vz / 100.0f; // Convert cm/s to m/s
        m_gpsData.estimator_type = global_pos.estimator_type;
        
        // Copy covariance matrix
        for (int i = 0; i < 36; ++i) {
            m_gpsData.covariance[i] = global_pos.covariance[i];
        }
        
        m_gpsData.has_position = true;
        m_gpsData.last_update = std::chrono::steady_clock::now();
        
        updateGPSState();
        notifyGPSCallback();
    }
}

void MavlinkParser::handleGlobalPositionIntMessage(const mavlink_message_t& message) {
    if (verbose_mode) {
        std::cout << "GLOBAL_POSITION_INT message received" << std::endl;
    }
    
    if (m_gpsDataCallback) {
        mavlink_global_position_int_t global_pos;
        mavlink_msg_global_position_int_decode(&message, &global_pos);
        
        // Update current position
        m_gpsData.latitude = global_pos.lat;
        m_gpsData.longitude = global_pos.lon;
        m_gpsData.altitude = global_pos.alt;
        m_gpsData.relative_altitude = global_pos.relative_alt;
        m_gpsData.velocity_x = global_pos.vx / 100.0f; // Convert to m/s
        m_gpsData.velocity_y = global_pos.vy / 100.0f; // Convert to m/s
        m_gpsData.velocity_z = global_pos.vz / 100.0f; // Convert to m/s
        m_gpsData.has_position = true;
        m_gpsData.position_time_usec = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // If we have position data, assume at least 2D fix
        if (m_gpsData.fix_type == GPSFixType::NO_FIX) {
            m_gpsData.fix_type = GPSFixType::GPS_2D_FIX;
        }
        
        m_gpsData.last_update = std::chrono::steady_clock::now();
        updateGPSState();
        notifyGPSCallback();
    }
}

void MavlinkParser::handleGPS2RawMessage(const mavlink_message_t& message) {
    if (verbose_mode) {
        std::cout << "GPS2_RAW message received" << std::endl;
    }
    
    if (m_gpsDataCallback) {
        mavlink_gps2_raw_t gps2_raw;
        mavlink_msg_gps2_raw_decode(&message, &gps2_raw);
        
        // Use secondary GPS data if primary has no fix
        if (m_gpsData.fix_type == GPSFixType::NO_FIX && gps2_raw.fix_type >= 2) {
            m_gpsData.fix_type = static_cast<GPSFixType>(gps2_raw.fix_type);
            m_gpsData.latitude = gps2_raw.lat;
            m_gpsData.longitude = gps2_raw.lon;
            m_gpsData.altitude = gps2_raw.alt;
            m_gpsData.has_position = true;
            m_gpsData.position_time_usec = gps2_raw.time_usec;
            m_gpsData.satellites_visible = gps2_raw.satellites_visible;
            
            updateGPSState();
            notifyGPSCallback();
        }
    }
}

void MavlinkParser::handleGPSInputMessage(const mavlink_message_t& message) {
    if (verbose_mode) {
        std::cout << "GPS_INPUT message received" << std::endl;
    }
    
    if (m_gpsDataCallback) {
        mavlink_gps_input_t gps_input;
        mavlink_msg_gps_input_decode(&message, &gps_input);
        
        // Update GPS state based on GPS input
        if (gps_input.ignore_flags == 0) { // No flags ignored
            m_gpsData.latitude = gps_input.lat;
            m_gpsData.longitude = gps_input.lon;
            m_gpsData.altitude = gps_input.alt;
            m_gpsData.has_position = true;
            m_gpsData.position_time_usec = gps_input.time_usec;
            m_gpsData.satellites_visible = gps_input.satellites_visible;
            
            // Determine fix type based on GPS input
            if (gps_input.fix_type >= 2) {
                m_gpsData.fix_type = static_cast<GPSFixType>(gps_input.fix_type);
            }
            
            updateGPSState();
            notifyGPSCallback();
        }
    }
}

void MavlinkParser::updateGPSState() {
    GPSState old_state = m_gpsData.state;
    
    // Determine GPS state based on available data
    if (m_gpsData.has_position && m_gpsData.position_time_usec > 0) {
        // We have valid position data, so we have some kind of fix
        m_gpsData.state = GPSState::GPS_FIX;
        
        // Determine fix type based on available data
        if (m_gpsData.satellites_used >= 4) {
            m_gpsData.fix_type = GPSFixType::GPS_3D_FIX;
        } else if (m_gpsData.satellites_used >= 3) {
            m_gpsData.fix_type = GPSFixType::GPS_2D_FIX;
        } else if (m_gpsData.fix_type >= GPSFixType::GPS_2D_FIX) {
            // Use the fix type from the GPS message if available
        } else {
            // Default to 2D fix if we have position but no satellite count
            m_gpsData.fix_type = GPSFixType::GPS_2D_FIX;
        }
    } else {
        // No valid position data
        m_gpsData.state = GPSState::NO_FIX;
        m_gpsData.fix_type = GPSFixType::NO_FIX;
    }
}

void MavlinkParser::notifyGPSCallback() {
    if (m_gpsDataCallback) {
        m_gpsDataCallback(m_gpsData);
    }
}

void MavlinkParser::handleSystemStatusMessage(const mavlink_message_t& message) {
    mavlink_sys_status_t sys_status;
    mavlink_msg_sys_status_decode(&message, &sys_status);
    
    // Update system status data
    m_systemStatus.onboard_control_sensors_present = sys_status.onboard_control_sensors_present;
    m_systemStatus.onboard_control_sensors_enabled = sys_status.onboard_control_sensors_enabled;
    m_systemStatus.onboard_control_sensors_health = sys_status.onboard_control_sensors_health;
    m_systemStatus.load = sys_status.load;
    m_systemStatus.voltage_battery = sys_status.voltage_battery;
    m_systemStatus.current_battery = sys_status.current_battery;
    m_systemStatus.battery_remaining = sys_status.battery_remaining;
    m_systemStatus.drop_rate_comm = sys_status.drop_rate_comm;
    m_systemStatus.errors_comm = sys_status.errors_comm;
    m_systemStatus.errors_count1 = sys_status.errors_count1;
    m_systemStatus.errors_count2 = sys_status.errors_count2;
    m_systemStatus.errors_count3 = sys_status.errors_count3;
    m_systemStatus.errors_count4 = sys_status.errors_count4;
    m_systemStatus.last_update = std::chrono::steady_clock::now();
    
    // Notify callback
    notifySystemStatusCallback();
}

void MavlinkParser::notifySystemStatusCallback() {
    if (m_systemStatusCallback) {
        m_systemStatusCallback(m_systemStatus);
    }
}
