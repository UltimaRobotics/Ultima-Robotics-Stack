#include "Vehicle.h"
#include "VehicleFactGroup.h"
#include "VehicleGPSFactGroup.h"
#include "VehicleGPS2FactGroup.h"
#include "VehicleBatteryFactGroup.h"
#include "VehicleSystemStatusFactGroup.h"
#include "VehicleRCFactGroup.h"
#include "VehicleVibrationFactGroup.h"
#include "VehicleTemperatureFactGroup.h"
#include "VehicleEstimatorStatusFactGroup.h"
#include "VehicleWindFactGroup.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include "ParameterManager.h"

// MAVLink headers for version information
#include "../thirdparty/c_library_v2/standard/mavlink_msg_autopilot_version.h"
#include "../thirdparty/c_library_v2/common/mavlink.h"

// Board identification
#include "BoardIdentifier.h"

// MAVLink constants
#define MAVLINK_MSG_ID_AUTOPILOT_VERSION 148
#define MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES 520

Vehicle::Vehicle(MAVLinkUdpConnection* connection)
    : FactGroup(100) // Update every 100ms
    , _connection(connection)
{
    _initializeFactGroups();
    
    if (_connection) {
        _connection->setVehicle(this);
    }
    
    _parameterManager = std::make_shared<ParameterManager>(this);
}

Vehicle::~Vehicle()
{
    if (_connection) {
        _connection->setVehicle(nullptr);
    }
}

bool Vehicle::sendMessage(const mavlink_message_t &message)
{
    if (_connection) {
        return _connection->sendMessage(message, _systemId, _componentId);
    }
    return false;
}

bool Vehicle::sendCommand(uint16_t command, uint8_t confirmation, 
                         float param1, float param2, float param3, 
                         float param4, float param5, float param6, float param7)
{
    mavlink_message_t message;
    mavlink_msg_command_long_pack(255, MAV_COMP_ID_MISSIONPLANNER, &message,
                                 _systemId, _componentId, command, confirmation,
                                 param1, param2, param3, param4, param5, param6, param7);
    
    return sendMessage(message);
}

bool Vehicle::armed() const
{
    return (_baseMode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
}

bool Vehicle::flying() const
{
    // Simple heuristic: if armed and altitude > 1m, consider flying
    auto altitudeFact = altitudeRelative();
    if (altitudeFact && armed()) {
        double altitude = std::get<double>(altitudeFact->cookedValue());
        return altitude > 1.0;
    }
    return false;
}

std::string Vehicle::flightMode() const
{
    // This would need to be implemented based on autopilot-specific custom modes
    // For now, return a generic string based on base mode
    if (_baseMode & MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) {
        return "Custom";
    } else if (_baseMode & MAV_MODE_FLAG_STABILIZE_ENABLED) {
        return "Stabilize";
    } else if (_baseMode & MAV_MODE_FLAG_GUIDED_ENABLED) {
        return "Guided";
    } else if (_baseMode & MAV_MODE_FLAG_AUTO_ENABLED) {
        return "Auto";
    } else if (_baseMode & MAV_MODE_FLAG_MANUAL_INPUT_ENABLED) {
        return "Manual";
    }
    return "Unknown";
}

std::string Vehicle::vehicleTypeString() const
{
    switch (_vehicleType) {
        case MAV_TYPE_QUADROTOR:
            return "Quadcopter";
        case MAV_TYPE_COAXIAL:
            return "Coaxial";
        case MAV_TYPE_HELICOPTER:
            return "Helicopter";
        case MAV_TYPE_FIXED_WING:
            return "Fixed Wing";
        case MAV_TYPE_GROUND_ROVER:
            return "Ground Rover";
        case MAV_TYPE_SURFACE_BOAT:
            return "Boat";
        case MAV_TYPE_SUBMARINE:
            return "Submarine";
        case MAV_TYPE_HEXAROTOR:
            return "Hexacopter";
        case MAV_TYPE_OCTOROTOR:
            return "Octocopter";
        case MAV_TYPE_TRICOPTER:
            return "Tricopter";
        default:
            return "Unknown";
    }
}

std::string Vehicle::autopilotTypeString() const
{
    switch (_autopilotType) {
        case MAV_AUTOPILOT_PX4:
            return "PX4";
        case MAV_AUTOPILOT_ARDUPILOTMEGA:
            return "ArduPilot";
        case MAV_AUTOPILOT_GENERIC:
            return "Generic";
        default:
            return "Unknown";
    }
}

std::string Vehicle::systemStatusString() const
{
    switch (_systemStatus) {
        case MAV_STATE_UNINIT:
            return "Uninitialized";
        case MAV_STATE_BOOT:
            return "Booting";
        case MAV_STATE_CALIBRATING:
            return "Calibrating";
        case MAV_STATE_STANDBY:
            return "Standby";
        case MAV_STATE_ACTIVE:
            return "Active";
        case MAV_STATE_CRITICAL:
            return "Critical";
        case MAV_STATE_EMERGENCY:
            return "Emergency";
        case MAV_STATE_POWEROFF:
            return "Powering Off";
        case MAV_STATE_FLIGHT_TERMINATION:
            return "Flight Termination";
        default:
            return "Unknown";
    }
}

void Vehicle::handleMessage(const mavlink_message_t &message)
{
    static uint32_t totalMessages = 0;
    static std::map<uint32_t, uint32_t> messageCounts;
    
    totalMessages++;
    messageCounts[message.msgid]++;
    
    switch (message.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT:
            _handleHeartbeat(message);
            break;
            
        case MAVLINK_MSG_ID_AUTOPILOT_VERSION:
            _handleAutopilotVersion(message);
            break;
            
        case MAVLINK_MSG_ID_STATUSTEXT:
            _handleStatustext(message);
            break;
            
        case MAVLINK_MSG_ID_COMMAND_ACK:
            _handleCommandAck(message);
            break;
            
        default:
            // Let fact groups handle the message
            _updateAllValues();
            
            // Forward to fact groups
            for (const auto& pair : _nameToFactGroupMap) {
                const auto& factGroup = pair.second;
                if (factGroup) {
                    factGroup->handleMessage(this, message);
                }
            }
            
            // Forward to parameter manager
            if (_parameterManager) {
                _parameterManager->mavlinkMessageReceived(message);
            }
            break;
    }
}

void Vehicle::_initializeFactGroups()
{
    // Create main vehicle fact group
    auto vehicleFactGroup = std::make_shared<VehicleFactGroup>();
    _addFactGroup(vehicleFactGroup, "vehicle");
    
    // Create GPS fact group
    auto gpsFactGroup = std::make_shared<VehicleGPSFactGroup>();
    _addFactGroup(gpsFactGroup, "gps");
    
    // Create GPS2 fact group
    auto gps2FactGroup = std::make_shared<VehicleGPS2FactGroup>();
    _addFactGroup(gps2FactGroup, "gps2");
    
    // Create battery fact group
    auto batteryFactGroup = std::make_shared<VehicleBatteryFactGroup>();
    _addFactGroup(batteryFactGroup, "battery");
    
    // Create system status fact group
    auto systemStatusFactGroup = std::make_shared<VehicleSystemStatusFactGroup>();
    _addFactGroup(systemStatusFactGroup, "systemStatus");
    
    // Create RC fact group
    auto rcFactGroup = std::make_shared<VehicleRCFactGroup>();
    _addFactGroup(rcFactGroup, "rc");
    
    // Create vibration fact group
    auto vibrationFactGroup = std::make_shared<VehicleVibrationFactGroup>();
    _addFactGroup(vibrationFactGroup, "vibration");
    
    // Create temperature fact group
    auto temperatureFactGroup = std::make_shared<VehicleTemperatureFactGroup>();
    _addFactGroup(temperatureFactGroup, "temperature");
    
    // Create estimator status fact group
    auto estimatorStatusFactGroup = std::make_shared<VehicleEstimatorStatusFactGroup>();
    _addFactGroup(estimatorStatusFactGroup, "estimatorStatus");
    
    // Create wind fact group
    auto windFactGroup = std::make_shared<VehicleWindFactGroup>();
    _addFactGroup(windFactGroup, "wind");
    
    // Add main vehicle facts to this group for easy access
    auto mainFacts = vehicleFactGroup->factNames();
    for (const auto& factName : mainFacts) {
        auto fact = vehicleFactGroup->getFact(factName);
        if (fact) {
            _addFact(fact, factName);
        }
    }
}

void Vehicle::_handleHeartbeat(const mavlink_message_t &message)
{
    mavlink_heartbeat_t heartbeat;
    mavlink_msg_heartbeat_decode(&message, &heartbeat);
    
    std::cout << "Received heartbeat from sysid=" << static_cast<int>(message.sysid) 
              << " compid=" << static_cast<int>(message.compid) 
              << " type=" << static_cast<int>(heartbeat.type)
              << " autopilot=" << static_cast<int>(heartbeat.autopilot) << std::endl;
    
    uint8_t oldSystemId = _systemId;
    uint8_t oldComponentId = _componentId;
    uint8_t oldVehicleType = _vehicleType;
    uint8_t oldAutopilotType = _autopilotType;
    uint8_t oldBaseMode = _baseMode;
    uint32_t oldCustomMode = _customMode;
    uint8_t oldSystemStatus = _systemStatus;
    
    _systemId = message.sysid;
    _componentId = message.compid;
    _vehicleType = heartbeat.type;
    _autopilotType = heartbeat.autopilot;
    _baseMode = heartbeat.base_mode;
    _customMode = heartbeat.custom_mode;
    _systemStatus = heartbeat.system_status;
    _mavlinkVersion = heartbeat.mavlink_version;
    
    // Request parameters on first heartbeat
    static bool firstHeartbeat = true;
    if (firstHeartbeat && _parameterManager) {
        std::cout << "First heartbeat received, requesting parameters..." << std::endl;
        _parameterManager->refreshAllParameters();
        
        // Request autopilot version information
        std::cout << "Requesting autopilot version information..." << std::endl;
        _requestAutopilotVersion();
        
        // Request telemetry data streams
        std::cout << "Requesting telemetry data streams..." << std::endl;
        _requestTelemetryStreams();
        
        firstHeartbeat = false;
    }
    
    // Check if anything changed
    if (oldSystemId != _systemId || oldComponentId != _componentId ||
        oldVehicleType != _vehicleType || oldAutopilotType != _autopilotType ||
        oldBaseMode != _baseMode || oldCustomMode != _customMode ||
        oldSystemStatus != _systemStatus) {
        _notifyVehicleChanged();
    }
    
    _lastHeartbeatTime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

void Vehicle::_handleStatustext(const mavlink_message_t &message)
{
    mavlink_statustext_t statustext;
    mavlink_msg_statustext_decode(&message, &statustext);
    
    std::string text(reinterpret_cast<char*>(statustext.text));
    std::string severity;
    
    switch (statustext.severity) {
        case MAV_SEVERITY_EMERGENCY:
            severity = "EMERGENCY";
            break;
        case MAV_SEVERITY_ALERT:
            severity = "ALERT";
            break;
        case MAV_SEVERITY_CRITICAL:
            severity = "CRITICAL";
            break;
        case MAV_SEVERITY_ERROR:
            severity = "ERROR";
            break;
        case MAV_SEVERITY_WARNING:
            severity = "WARNING";
            break;
        case MAV_SEVERITY_NOTICE:
            severity = "NOTICE";
            break;
        case MAV_SEVERITY_INFO:
            severity = "INFO";
            break;
        case MAV_SEVERITY_DEBUG:
            severity = "DEBUG";
            break;
        default:
            severity = "UNKNOWN";
            break;
    }
    
    if (_vehicleTextMessageCallback) {
        _vehicleTextMessageCallback(this, severity, text);
    }
}

void Vehicle::_handleCommandAck(const mavlink_message_t &message)
{
    mavlink_command_ack_t commandAck;
    mavlink_msg_command_ack_decode(&message, &commandAck);
    
    // Handle command acknowledgment
    // This could be expanded to track pending commands
}

void Vehicle::_handleAutopilotVersion(const mavlink_message_t &message)
{
    mavlink_autopilot_version_t version;
    mavlink_msg_autopilot_version_decode(&message, &version);
    
    std::cout << "\n=== Received AUTOPILOT_VERSION Message ===" << std::endl;
    std::cout << "Board Identification: " << BoardIdentifier::instance().identifyBoard(version.vendor_id, version.product_id) << std::endl;
    std::cout << "Board Class: " << BoardIdentifier::instance().getBoardClass(version.vendor_id, version.product_id) << std::endl;
    std::cout << "Board Name: " << BoardIdentifier::instance().getBoardName(version.vendor_id, version.product_id) << std::endl;
    std::cout << "Vendor ID: " << version.vendor_id << std::endl;
    std::cout << "Product ID: " << version.product_id << std::endl;
    std::cout << "UID (Serial): " << version.uid << std::endl;
    std::cout << "Board Version: " << version.board_version << std::endl;
    std::cout << "Flight SW Version: " << flightSwVersionString() << " (raw: " << version.flight_sw_version << ")" << std::endl;
    std::cout << "Middleware SW Version: " << middlewareSwVersionString() << " (raw: " << version.middleware_sw_version << ")" << std::endl;
    std::cout << "OS SW Version: " << osSwVersionString() << " (raw: " << version.os_sw_version << ")" << std::endl;
    std::cout << "Flight Custom Version (Git Hash): " << flightCustomVersionString() << std::endl;
    std::cout << "Capabilities: " << capabilitiesString() << std::endl;
    std::cout << "===============================================\n" << std::endl;
    
    // Store version information
    _capabilities = version.capabilities;
    _uid = version.uid;
    _flightSwVersion = version.flight_sw_version;
    _middlewareSwVersion = version.middleware_sw_version;
    _osSwVersion = version.os_sw_version;
    _boardVersion = version.board_version;
    _vendorId = version.vendor_id;
    _productId = version.product_id;
    
    // Copy custom version bytes
    for (int i = 0; i < 8; i++) {
        _flightCustomVersion[i] = version.flight_custom_version[i];
    }
    
    _notifyVehicleChanged();
}

void Vehicle::_requestAutopilotVersion()
{
    // Send MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES command to request AUTOPILOT_VERSION message
    bool success = sendCommand(MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES, 1, 1.0f);
    
    if (success) {
        std::cout << "Sent AUTOPILOT_VERSION request command" << std::endl;
    } else {
        std::cout << "Failed to send AUTOPILOT_VERSION request command" << std::endl;
    }
}

void Vehicle::_requestTelemetryStreams()
{
    // Request key telemetry data streams at 10Hz (100ms interval)
    // MAV_CMD_SET_MESSAGE_INTERVAL: param1 = message ID, param2 = interval (microseconds)
    
    struct StreamRequest {
        uint32_t msgId;
        const char* name;
    };
    
    StreamRequest streams[] = {
        {MAVLINK_MSG_ID_SYS_STATUS, "SYS_STATUS"},
        {MAVLINK_MSG_ID_ATTITUDE, "ATTITUDE"},
        {MAVLINK_MSG_ID_GLOBAL_POSITION_INT, "GLOBAL_POSITION_INT"},
        {MAVLINK_MSG_ID_GPS_RAW_INT, "GPS_RAW_INT"},
        {MAVLINK_MSG_ID_GPS_STATUS, "GPS_STATUS"},
        {MAVLINK_MSG_ID_GPS2_RAW, "GPS2_RAW"},
        {MAVLINK_MSG_ID_BATTERY_STATUS, "BATTERY_STATUS"},
        {MAVLINK_MSG_ID_VFR_HUD, "VFR_HUD"},
        {MAVLINK_MSG_ID_RAW_IMU, "RAW_IMU"},
        {MAVLINK_MSG_ID_SCALED_IMU, "SCALED_IMU"},
        {MAVLINK_MSG_ID_SCALED_PRESSURE, "SCALED_PRESSURE"},
        {MAVLINK_MSG_ID_SERVO_OUTPUT_RAW, "SERVO_OUTPUT_RAW"}
    };
    
    const int streamCount = sizeof(streams) / sizeof(streams[0]);
    const uint32_t interval_us = 100000; // 10Hz = 100,000 microseconds
    
    for (int i = 0; i < streamCount; i++) {
        bool success = sendCommand(MAV_CMD_SET_MESSAGE_INTERVAL, 0, 
                                  static_cast<float>(streams[i].msgId), 
                                  static_cast<float>(interval_us));
        
        if (success) {
            std::cout << "Requested " << streams[i].name << " stream at 10Hz" << std::endl;
        } else {
            std::cout << "Failed to request " << streams[i].name << " stream" << std::endl;
        }
        
        // Small delay to avoid overwhelming the vehicle
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string Vehicle::flightCustomVersionString() const
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; i++) {
        ss << std::setw(2) << static_cast<unsigned int>(_flightCustomVersion[i]);
    }
    return ss.str();
}

std::string Vehicle::flightSwVersionString() const
{
    // Format flight_sw_version as: major.minor.patch.type
    // According to MAVLink spec: (major) (minor) (patch) (FIRMWARE_VERSION_TYPE)
    uint8_t major = (_flightSwVersion >> 24) & 0xFF;
    uint8_t minor = (_flightSwVersion >> 16) & 0xFF;
    uint8_t patch = (_flightSwVersion >> 8) & 0xFF;
    uint8_t type = _flightSwVersion & 0xFF;
    
    std::stringstream ss;
    ss << static_cast<int>(major) << "." 
       << static_cast<int>(minor) << "." 
       << static_cast<int>(patch);
    
    // Add firmware type description
    switch (type) {
        case 0: ss << " (dev)"; break;
        case 1: ss << " (alpha)"; break;
        case 2: ss << " (beta)"; break;
        case 3: ss << " (rc)"; break;
        case 4: ss << " (release)"; break;
        default: ss << " (type:" << static_cast<int>(type) << ")"; break;
    }
    
    return ss.str();
}

std::string Vehicle::middlewareSwVersionString() const
{
    uint8_t major = (_middlewareSwVersion >> 24) & 0xFF;
    uint8_t minor = (_middlewareSwVersion >> 16) & 0xFF;
    uint8_t patch = (_middlewareSwVersion >> 8) & 0xFF;
    
    std::stringstream ss;
    ss << static_cast<int>(major) << "." 
       << static_cast<int>(minor) << "." 
       << static_cast<int>(patch);
    
    return ss.str();
}

std::string Vehicle::osSwVersionString() const
{
    uint8_t major = (_osSwVersion >> 24) & 0xFF;
    uint8_t minor = (_osSwVersion >> 16) & 0xFF;
    uint8_t patch = (_osSwVersion >> 8) & 0xFF;
    
    std::stringstream ss;
    ss << static_cast<int>(major) << "." 
       << static_cast<int>(minor) << "." 
       << static_cast<int>(patch);
    
    return ss.str();
}

std::string Vehicle::capabilitiesString() const
{
    std::stringstream ss;
    ss << "0x" << std::hex << _capabilities << " (";
    
    bool first = true;
    if (_capabilities & (1ULL << 0)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_MISSION_FLOAT"; first = false; }
    if (_capabilities & (1ULL << 1)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT"; first = false; }
    if (_capabilities & (1ULL << 2)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_MISSION_INT"; first = false; }
    if (_capabilities & (1ULL << 3)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_COMMAND_INT"; first = false; }
    if (_capabilities & (1ULL << 4)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_PARAM_UNION"; first = false; }
    if (_capabilities & (1ULL << 5)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_FTP"; first = false; }
    if (_capabilities & (1ULL << 6)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_SET_ATTITUDE_TARGET"; first = false; }
    if (_capabilities & (1ULL << 7)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_SET_POSITION_TARGET_LOCAL_NED"; first = false; }
    if (_capabilities & (1ULL << 8)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_SET_POSITION_TARGET_GLOBAL_INT"; first = false; }
    if (_capabilities & (1ULL << 9)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_TERRAIN"; first = false; }
    if (_capabilities & (1ULL << 10)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_SET_ACTUATOR_TARGET"; first = false; }
    if (_capabilities & (1ULL << 11)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_FLIGHT_TERMINATION"; first = false; }
    if (_capabilities & (1ULL << 12)) { ss << (first ? "" : ", ") << "MAV_PROTOCOL_CAPABILITY_COMPASS_CALIBRATION"; first = false; }
    
    if (first) ss << "unknown";
    ss << ")";
    
    return ss.str();
}

std::string Vehicle::boardName() const
{
    return BoardIdentifier::instance().getBoardName(_vendorId, _productId);
}

std::string Vehicle::boardClass() const
{
    return BoardIdentifier::instance().getBoardClass(_vendorId, _productId);
}

std::string Vehicle::boardIdentification() const
{
    return BoardIdentifier::instance().identifyBoard(_vendorId, _productId);
}

void Vehicle::_notifyVehicleChanged()
{
    if (_vehicleChangedCallback) {
        _vehicleChangedCallback(this);
    }
}
