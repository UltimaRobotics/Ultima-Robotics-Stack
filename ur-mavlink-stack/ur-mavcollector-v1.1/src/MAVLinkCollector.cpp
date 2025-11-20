#include "../include/MAVLinkCollector.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iomanip>
#include <sstream>
#include <set>
#include <condition_variable>
#ifdef HTTP_ENABLED
#include "HttpClient.hpp"
#endif

MAVLinkCollector::MAVLinkCollector(const std::string& config_file)
    : running_(false) {

    // Load configuration
    std::ifstream config_stream(config_file);
    if (!config_stream.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_file);
    }

    nlohmann::json config;
    config_stream >> config;

    config_.udp_address = config.value("udp_address", "127.0.0.1");
    config_.udp_port = config.value("udp_port", 14550);
    config_.request_sysid = config.value("request_sysid", 255);
    config_.request_compid = config.value("request_compid", 0);
    config_.log_file = config.value("log_file", "mavlink_collector.json");
    config_.diagnose_data_logfile = config.value("diagnose_data_logfile", "diagnose_data.json");
    config_.request_interval_ms = config.value("request_interval_ms", 1000);
    config_.log_interval_ms = config.value("log_interval_ms", 1000);
    config_.diagnose_log_interval_ms = config.value("diagnose_log_interval_ms", 5000);
    config_.socket_timeout_ms = config.value("socket_timeout_ms", 100);
    config_.collection_loop_sleep_ms = config.value("collection_loop_sleep_ms", 10);
    config_.verbose = config.value("verbose", false);
    config_.mav_stats_counter = config.value("mav_stats_counter", false);
    config_.enableHTTP = config.value("enableHTTP", false);
    params_log_file_ = config.value("mav_stats_log_file", "mav_stats.json"); // Added for parameter logging

    // Load filtered system IDs
    if (config.contains("filtered_system_ids") && config["filtered_system_ids"].is_array()) {
        for (const auto& id : config["filtered_system_ids"]) {
            if (id.is_number_unsigned()) {
                uint8_t sys_id = id.get<uint8_t>();
                config_.filtered_system_ids.insert(sys_id);
                std::cout << "[Collector] Filtering system ID: " << (int)sys_id << std::endl;
            }
        }
    }

    if (config.contains("httpConfig")) {
        auto httpCfg = config["httpConfig"];
        config_.httpConfig.serverAddress = httpCfg.value("serverAddress", "0.0.0.0");
        config_.httpConfig.serverPort = httpCfg.value("serverPort", 5000);
        config_.httpConfig.timeoutMs = httpCfg.value("timeoutMs", 5000);
        config_.httpConfig.fetch_status_interval_ms = httpCfg.value("fetch_status_interval_ms", 1000);
    }

    // Initialize vehicle data
    vehicle_data_.system_id = 0;
    vehicle_data_.component_id = 0;
    vehicle_data_.armed = false;
    vehicle_data_.battery_voltage = -1.0f;
    vehicle_data_.messages_received = 0;

    // Initialize stats tracking
    total_messages_received_ = 0;

    // Create UDP handler
    UdpHandler::Config udp_config;
    udp_config.bind_address = "0.0.0.0";
    udp_config.bind_port = config_.udp_port;
    udp_config.target_address = config_.udp_address;
    udp_config.target_port = config_.udp_port;
    udp_config.socket_timeout_ms = config_.socket_timeout_ms;
    udp_config.verbose = config_.verbose;

    udp_handler_ = std::make_unique<UdpHandler>(udp_config);

    std::cout << "[Collector] Configuration loaded from " << config_file << std::endl;
    std::cout << "[Collector] UDP: " << config_.udp_address << ":" << config_.udp_port << std::endl;
    std::cout << "[Collector] Request from SysID=" << (int)config_.request_sysid 
              << " CompID=" << (int)config_.request_compid << std::endl;
}

MAVLinkCollector::~MAVLinkCollector() {
    stop();
}

bool MAVLinkCollector::start() {
    if (running_) {
        std::cerr << "[Collector] Already running" << std::endl;
        return false;
    }

    // Set up packet callback
    udp_handler_->setPacketCallback(
        [this](const UdpHandler::UdpPacket& packet) {
            this->onCollectorPacket(packet);
        }
    );

    // Start UDP handler
    if (!udp_handler_->start()) {
        std::cerr << "[Collector] Failed to start UDP handler" << std::endl;
        return false;
    }

    // Load or initialize JSON log
    loadExistingLog();
    loadExistingDiagnoseLog();

    running_ = true;
    vehicle_data_.start_time = std::chrono::system_clock::now();
    last_log_time_ = std::chrono::system_clock::now();
    last_diagnose_log_time_ = std::chrono::system_clock::now();
    last_rate_calculation_ = std::chrono::steady_clock::now();

    // Start collection thread
    collection_thread_ = std::make_unique<std::thread>(&MAVLinkCollector::collectionLoop, this);

    std::cout << "[Collector] Started successfully" << std::endl;
    return true;
}

void MAVLinkCollector::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (collection_thread_ && collection_thread_->joinable()) {
        collection_thread_->join();
    }

    udp_handler_->stop();

    // Save final state
    saveLogToFile();
    saveDiagnoseLogToFile();
    if (config_.mav_stats_counter) {
        saveParametersToFile(); // Save collected parameters when stopping
    }

    std::cout << "[Collector] Stopped" << std::endl;
}

void MAVLinkCollector::onCollectorPacket(const UdpHandler::UdpPacket& packet) {
    // Parse MAVLink messages from packet
    mavlink_message_t msg;
    mavlink_status_t status;

    // Use appropriate channel based on detected version
    uint8_t channel = (udp_handler_->getMavlinkVersion() == 1) ? MAVLINK_COMM_V1 : MAVLINK_COMM_V2;

    for (size_t i = 0; i < packet.length; i++) {
        if (mavlink_parse_char(channel, packet.data[i], &msg, &status) == 1) {
            // Filter out our own messages
            if (msg.sysid == config_.request_sysid && msg.compid == config_.request_compid) {
                if (config_.verbose) {
                    std::cout << "[Collector] Ignoring own message echo" << std::endl;
                }
                continue;
            }

            // Filter out configured system IDs
            if (config_.filtered_system_ids.find(msg.sysid) != config_.filtered_system_ids.end()) {
                if (config_.verbose) {
                    std::cout << "[Collector] Dropping message from filtered system ID: " << (int)msg.sysid << std::endl;
                }
                continue;
            }

            if (config_.verbose) {
                std::cout << "[VERBOSE] Parsed MAVLink message - ID: " << (int)msg.msgid 
                          << ", sysid: " << (int)msg.sysid << ", compid: " << (int)msg.compid << std::endl;
            }

            vehicle_data_.messages_received++;
            vehicle_data_.last_activity = std::chrono::system_clock::now();

            // Update stats counter if enabled
            if (config_.mav_stats_counter) {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                total_messages_received_++;
                message_stats_[msg.msgid]++;
            }

            // Store source address from first valid external message
            if (vehicle_data_.system_id == 0) {
                udp_handler_->updateTargetAddress(packet.source_addr);
                if (config_.verbose) {
                    char src_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &packet.source_addr.sin_addr, src_ip, INET_ADDRSTRLEN);
                    std::cout << "[VERBOSE] Updated target address to " << src_ip 
                              << ":" <<ntohs(packet.source_addr.sin_port) << std::endl;
                }
            }

            // Update message rate tracking
            updateMessageRate(msg.msgid);

            // Invoke registered callbacks
            invokeCallbacks(msg);

            // Process specific messages
            // Note: Message IDs are the same in v1 and v2 for common messages
            switch (msg.msgid) {
                case MAVLINK_MSG_ID_HEARTBEAT:
                    handleHeartbeat(msg);

                    // Once we have vehicle info and version is detected, request appropriate streams
                    if (vehicle_data_.system_id > 0 && udp_handler_->isMavlinkVersionDetected()) {
                        static bool streams_requested = false;
                        if (!streams_requested) {
                            if (udp_handler_->getMavlinkVersion() == 1) {
                                requestHighFrequencyStreamsV1();
                            } else {
                                requestHighFrequencyStreamsV2();
                            }
                            // Request all parameters after MAVLink version is detected
                            requestAllParameters();
                            streams_requested = true;
                        }
                    }
                    break;
                case MAVLINK_MSG_ID_AUTOPILOT_VERSION:
                    handleAutopilotVersion(msg);
                    break;
                case MAVLINK_MSG_ID_SYS_STATUS:
                    handleSysStatus(msg);
                    break;
                case MAVLINK_MSG_ID_BATTERY_STATUS:
                    handleBatteryStatus(msg);
                    break;
                case 370: // MAVLINK_MSG_ID_SMART_BATTERY_INFO (v1)
                case 372: // MAVLINK_MSG_ID_BATTERY_INFO (v2)
                    handleBatteryInfo(msg);
                    break;
                case MAVLINK_MSG_ID_PARAM_VALUE:
                    handleParamValue(msg);
                    break;
                case 396: // MAVLINK_MSG_ID_COMPONENT_INFORMATION_BASIC
                    handleComponentInformationBasic(msg);
                    break;
                case MAVLINK_MSG_ID_POWER_STATUS: // MAVLINK_MSG_ID_POWER_STATUS (ID: 111)
                    handlePowerStatus(msg);
                    break;
                default:
                    break;
            }
        }
    }
}

void MAVLinkCollector::loadExistingLog() {
    std::ifstream log_file(config_.log_file);
    if (log_file.is_open()) {
        try {
            log_file >> log_json_;
            std::cout << "[Collector] Loaded existing log file: " << config_.log_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Collector] Failed to parse existing log, creating new: " << e.what() << std::endl;
            log_json_ = nlohmann::json::object();
        }
    } else {
        std::cout << "[Collector] Creating new log file: " << config_.log_file << std::endl;
        log_json_ = nlohmann::json::object();
    }
}

void MAVLinkCollector::saveLogToFile() {
    std::ofstream log_file(config_.log_file);
    if (log_file.is_open()) {
        log_file << log_json_.dump(2) << std::endl;
        log_file.close();
    } else {
        std::cerr << "[Collector] Failed to save log file: " << config_.log_file << std::endl;
    }
}

void MAVLinkCollector::collectionLoop() {
    auto last_request_time = std::chrono::steady_clock::now();

    std::cout << "[Collector] Data collection loop started" << std::endl;

    while (running_) {
        auto now = std::chrono::steady_clock::now();

        // Send data requests periodically
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_request_time).count() >= config_.request_interval_ms) {
            requestData();
            last_request_time = now;
        }

        // Calculate message rates periodically (every second)
        auto rate_calc_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_rate_calculation_).count();
        if (rate_calc_elapsed >= 1000) {
            calculateMessageRates();
            last_rate_calculation_ = now;
        }

        // Log data periodically at configured interval
        auto time_since_log = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - last_log_time_).count();
        if (time_since_log >= config_.log_interval_ms) {
            logData();
            last_log_time_ = std::chrono::system_clock::now();
        }

        // Log diagnostic data periodically
        auto time_since_diagnose_log = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - last_diagnose_log_time_).count();
        if (time_since_diagnose_log >= config_.diagnose_log_interval_ms) {
            requestDiagnosticData();
            logDiagnosticData();
            last_diagnose_log_time_ = std::chrono::system_clock::now();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config_.collection_loop_sleep_ms));
    }

    std::cout << "[Collector] Data collection loop stopped" << std::endl;
}

void MAVLinkCollector::requestData() {
    // Send heartbeat to establish presence as GCS
    requestHeartbeat();

    // Use version-specific high frequency stream requests
    if (udp_handler_->isMavlinkVersionDetected()) {
        if (udp_handler_->getMavlinkVersion() == 1) {
            if (config_.verbose) {
                std::cout << "[Collector] Using MAVLink v1 stream requests" << std::endl;
            }
            requestHighFrequencyStreamsV1();
        } else {
            if (config_.verbose) {
                std::cout << "[Collector] Using MAVLink v2 message interval requests" << std::endl;
            }
            requestHighFrequencyStreamsV2();
        }
    } else {
        // Fallback: try both methods until version is detected
        requestDataStream();
        requestSpecificMessages();
    }
}

bool MAVLinkCollector::sendMavlinkMessage(const mavlink_message_t& msg) {
    return udp_handler_->sendMavlinkMessage(msg);
}

void MAVLinkCollector::requestHeartbeat() {
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        MAV_TYPE_GCS,
        MAV_AUTOPILOT_INVALID,
        0,
        0,
        MAV_STATE_ACTIVE
    );

    if (config_.verbose) {
        std::cout << "[VERBOSE] Sending HEARTBEAT request" << std::endl;
    }

    sendMavlinkMessage(msg);
}

void MAVLinkCollector::requestDataStream() {
    if (vehicle_data_.system_id == 0) {
        mavlink_message_t msg;
        mavlink_msg_request_data_stream_pack(
            config_.request_sysid,
            config_.request_compid,
            &msg,
            0,
            0,
            MAV_DATA_STREAM_ALL,
            1,
            1
        );

        if (config_.verbose) {
            std::cout << "[VERBOSE] Sending REQUEST_DATA_STREAM (legacy)" << std::endl;
        }

        sendMavlinkMessage(msg);
    }
}

void MAVLinkCollector::requestSpecificMessages() {
    if (!running_ || vehicle_data_.system_id == 0) {
        return;
    }

    // Request battery status (ID: 147)
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        vehicle_data_.system_id,
        vehicle_data_.component_id,
        MAV_CMD_REQUEST_MESSAGE,
        0,
        MAVLINK_MSG_ID_BATTERY_STATUS,
        0, 0, 0, 0, 0, 0
    );
    sendMavlinkMessage(msg);

    // Request power status (ID: 111)
    mavlink_msg_command_long_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        vehicle_data_.system_id,
        vehicle_data_.component_id,
        MAV_CMD_REQUEST_MESSAGE,
        0,
        MAVLINK_MSG_ID_POWER_STATUS,
        0, 0, 0, 0, 0, 0
    );
    sendMavlinkMessage(msg);
}

void MAVLinkCollector::requestBatteryComponents() {
    if (!running_ || vehicle_data_.system_id == 0) {
        return;
    }

    // Request battery information for both battery components
    requestBatteryComponentInfo(MAV_COMP_ID_BATTERY);
    requestBatteryComponentInfo(MAV_COMP_ID_BATTERY2);
}

void MAVLinkCollector::requestBatteryComponentInfo(uint8_t component_id) {
    mavlink_message_t msg;

    if (udp_handler_->getMavlinkVersion() == 2) {
        // Request BATTERY_STATUS for specific component at 1Hz
        mavlink_msg_command_long_pack(
            config_.request_sysid,
            config_.request_compid,
            &msg,
            vehicle_data_.system_id,
            component_id,  // Target specific battery component
            MAV_CMD_SET_MESSAGE_INTERVAL,
            0,
            MAVLINK_MSG_ID_BATTERY_STATUS,
            100000,  // 1 second interval (1Hz)
            0, 0, 0, 0, 0
        );
        sendMavlinkMessage(msg);

        // Request BATTERY_INFO for specific component (sent once)
        mavlink_msg_command_long_pack(
            config_.request_sysid,
            config_.request_compid,
            &msg,
            vehicle_data_.system_id,
            component_id,  // Target specific battery component
            MAV_CMD_REQUEST_MESSAGE,
            0,
            MAVLINK_MSG_ID_BATTERY_INFO,
            0, 0, 0, 0, 0, 0
        );
        sendMavlinkMessage(msg);

        if (config_.verbose) {
            std::cout << "[Collector] Requested battery data for component " 
                      << (int)component_id << std::endl;
        }
    }
}

void MAVLinkCollector::handleHeartbeat(const mavlink_message_t& msg) {
    mavlink_heartbeat_t heartbeat;
    mavlink_msg_heartbeat_decode(&msg, &heartbeat);

    // Update vehicle info
    vehicle_data_.system_id = msg.sysid;
    vehicle_data_.component_id = msg.compid;
    vehicle_data_.last_heartbeat = std::chrono::system_clock::now();

    // Map vehicle type
    std::string type_str = "Unknown";
    switch (heartbeat.type) {
        case MAV_TYPE_GENERIC: diagnostic_data_.airframe_type = "Generic micro air vehicle"; break;
        case MAV_TYPE_FIXED_WING: diagnostic_data_.airframe_type = "Fixed wing aircraft"; break;
        case MAV_TYPE_QUADROTOR: diagnostic_data_.airframe_type = "Quadrotor"; break;
        case MAV_TYPE_COAXIAL: diagnostic_data_.airframe_type = "Coaxial helicopter"; break;
        case MAV_TYPE_HELICOPTER: diagnostic_data_.airframe_type = "Normal helicopter with tail rotor"; break;
        case MAV_TYPE_ANTENNA_TRACKER: diagnostic_data_.airframe_type = "Ground installation"; break;
        case MAV_TYPE_GCS: diagnostic_data_.airframe_type = "Operator control unit / ground control station"; break;
        case MAV_TYPE_AIRSHIP: diagnostic_data_.airframe_type = "Airship, controlled"; break;
        case MAV_TYPE_FREE_BALLOON: diagnostic_data_.airframe_type = "Free balloon, uncontrolled"; break;
        case MAV_TYPE_ROCKET: diagnostic_data_.airframe_type = "Rocket"; break;
        case MAV_TYPE_GROUND_ROVER: diagnostic_data_.airframe_type = "Ground rover"; break;
        case MAV_TYPE_SURFACE_BOAT: diagnostic_data_.airframe_type = "Surface vessel, boat, ship"; break;
        case MAV_TYPE_SUBMARINE: diagnostic_data_.airframe_type = "Submarine"; break;
        case MAV_TYPE_HEXAROTOR: diagnostic_data_.airframe_type = "Hexarotor"; break;
        case MAV_TYPE_OCTOROTOR: diagnostic_data_.airframe_type = "Octorotor"; break;
        case MAV_TYPE_TRICOPTER: diagnostic_data_.airframe_type = "Tricopter"; break;
        case MAV_TYPE_FLAPPING_WING: diagnostic_data_.airframe_type = "Flapping wing"; break;
        case MAV_TYPE_KITE: diagnostic_data_.airframe_type = "Kite"; break;
        case MAV_TYPE_ONBOARD_CONTROLLER: diagnostic_data_.airframe_type = "Onboard companion controller"; break;
        case MAV_TYPE_VTOL_TILTROTOR: diagnostic_data_.airframe_type = "Tiltrotor VTOL"; break;
        case MAV_TYPE_VTOL_RESERVED5: diagnostic_data_.airframe_type = "VTOL reserved 5"; break;
        case MAV_TYPE_GIMBAL: diagnostic_data_.airframe_type = "Gimbal"; break;
        case MAV_TYPE_ADSB: diagnostic_data_.airframe_type = "ADSB system"; break;
        case MAV_TYPE_PARAFOIL: diagnostic_data_.airframe_type = "Steerable, nonrigid airfoil"; break;
        case MAV_TYPE_DODECAROTOR: diagnostic_data_.airframe_type = "Dodecarotor"; break;
        case 30: diagnostic_data_.airframe_type = "Camera"; break;
        case 31: diagnostic_data_.airframe_type = "Charging station"; break;
        case 32: diagnostic_data_.airframe_type = "FLARM collision avoidance system"; break;
        case 33: diagnostic_data_.airframe_type = "Servo"; break;
        case 34: diagnostic_data_.airframe_type = "Open Drone ID"; break;
        case 35: diagnostic_data_.airframe_type = "Decarotor"; break;
        case 36: diagnostic_data_.airframe_type = "Battery"; break;
        case 37: diagnostic_data_.airframe_type = "Parachute"; break;
        case 38: diagnostic_data_.airframe_type = "Log"; break;
        case 39: diagnostic_data_.airframe_type = "OSD"; break;
        case 40: diagnostic_data_.airframe_type = "IMU"; break;
        case 41: diagnostic_data_.airframe_type = "GPS"; break;
        case 42: diagnostic_data_.airframe_type = "Winch"; break;
        case 43: diagnostic_data_.airframe_type = "Generic multirotor"; break;
        case 44: diagnostic_data_.airframe_type = "Illuminator"; break;
        case 45: diagnostic_data_.airframe_type = "Spacecraft Orbiter"; break;
        case 46: diagnostic_data_.airframe_type = "Ground Quadruped"; break;
        case 47: diagnostic_data_.airframe_type = "VTOL Gyrodyne"; break;
        case 48: diagnostic_data_.airframe_type = "Gripper"; break;
        case 49: diagnostic_data_.airframe_type = "Radio"; break;
        default: diagnostic_data_.airframe_type = "Unknown Type " + std::to_string(heartbeat.type); break;
    }

    // Decode flight mode based on autopilot type
    if (heartbeat.autopilot == MAV_AUTOPILOT_ARDUPILOTMEGA) {
        static const char* copter_modes[] = {
            "STABILIZE", "ACRO", "ALT_HOLD", "AUTO", "GUIDED", "LOITER",
            "RTL", "CIRCLE", "LAND", "DRIFT", "SPORT", "FLIP", "AUTOTUNE",
            "POSHOLD", "BRAKE", "THROW", "AVOID_ADSB", "GUIDED_NOGPS", "SMART_RTL"
        };
        uint32_t cm = heartbeat.custom_mode;
        if (cm < sizeof(copter_modes) / sizeof(copter_modes[0])) {
            vehicle_data_.flight_mode = copter_modes[cm];
        } else {
            vehicle_data_.flight_mode = "UNKNOWN_" + std::to_string(cm);
        }
    } else if (heartbeat.autopilot == MAV_AUTOPILOT_PX4) {
        uint8_t main_mode = (heartbeat.custom_mode >> 16) & 0xFF;
        switch (main_mode) {
            case 1: vehicle_data_.flight_mode = "MANUAL"; break;
            case 2: vehicle_data_.flight_mode = "ALTCTL"; break;
            case 3: vehicle_data_.flight_mode = "POSCTL"; break;
            case 4: vehicle_data_.flight_mode = "AUTO"; break;
            case 5: vehicle_data_.flight_mode = "ACRO"; break;
            case 6: vehicle_data_.flight_mode = "OFFBOARD"; break;
            case 7: vehicle_data_.flight_mode = "STABILIZED"; break;
            case 8: vehicle_data_.flight_mode = "RATTITUDE"; break;
            default:
                vehicle_data_.flight_mode = "UNKNOWN_PX4_" + std::to_string(heartbeat.custom_mode);
                break;
        }
    } else {
        vehicle_data_.flight_mode = "UNKNOWN_" + std::to_string(heartbeat.custom_mode);
    }

    if (vehicle_data_.firmware.empty()) {
        switch (heartbeat.autopilot) {
            case MAV_AUTOPILOT_ARDUPILOTMEGA:
                vehicle_data_.firmware = "ArduPilot";
                break;
            case MAV_AUTOPILOT_PX4:
                vehicle_data_.firmware = "PX4";
                break;
            default:
                break;
        }
    }

    std::cout << "[Collector] Heartbeat from sysid=" << (int)msg.sysid 
              << " compid=" << (int)msg.compid 
              << " armed=" << vehicle_data_.armed 
              << " mode=" << vehicle_data_.flight_mode 
              << " type=" << diagnostic_data_.airframe_type << std::endl;
}

void MAVLinkCollector::handleAutopilotVersion(const mavlink_message_t& msg) {
    uint32_t flight_sw_version = mavlink_msg_autopilot_version_get_flight_sw_version(&msg);
    uint8_t major = (flight_sw_version >> 24) & 0xFF;
    uint8_t minor = (flight_sw_version >> 16) & 0xFF;
    uint8_t patch = (flight_sw_version >> 8) & 0xFF;

    std::stringstream ss;
    std::string firmware_type;
    if (!vehicle_data_.firmware.empty()) {
        size_t space_pos = vehicle_data_.firmware.find(' ');
        if (space_pos != std::string::npos) {
            firmware_type = vehicle_data_.firmware.substr(0, space_pos);
        } else {
            firmware_type = vehicle_data_.firmware;
        }
        ss << firmware_type << " ";
    }

    ss << (int)major << "." << (int)minor << "." << (int)patch;
    vehicle_data_.firmware = ss.str();

    // Also set software_version if not already set by COMPONENT_INFORMATION_BASIC
    if (vehicle_data_.software_version.empty()) {
        std::stringstream ver_ss;
        ver_ss << (int)major << "." << (int)minor << "." << (int)patch;
        vehicle_data_.software_version = ver_ss.str();
    }

    uint8_t flight_custom_version[8];
    mavlink_msg_autopilot_version_get_flight_custom_version(&msg, flight_custom_version);

    if (flight_custom_version[0] != 0) {
        char custom_str[9] = {0};
        memcpy(custom_str, flight_custom_version, 8);
        vehicle_data_.model = std::string(custom_str);
    }

    std::cout << "[Collector] Autopilot version: " << vehicle_data_.firmware;
    if (!vehicle_data_.model.empty()) {
        std::cout << ", model: " << vehicle_data_.model;
    }
    std::cout << std::endl;
}

void MAVLinkCollector::handleSysStatus(const mavlink_message_t& msg) {
    uint16_t voltage_battery = mavlink_msg_sys_status_get_voltage_battery(&msg);

    if (voltage_battery != UINT16_MAX && voltage_battery > 0) {
        vehicle_data_.battery_voltage = voltage_battery / 1000.0f;
        std::cout << "[Collector] SYS_STATUS battery voltage: " 
                  << vehicle_data_.battery_voltage << "V" << std::endl;
    }

    // Extract sensor status for diagnostics
    std::lock_guard<std::mutex> lock(diagnose_mutex_);
    uint32_t onboard_control_sensors_present = mavlink_msg_sys_status_get_onboard_control_sensors_present(&msg);
    uint32_t onboard_control_sensors_enabled = mavlink_msg_sys_status_get_onboard_control_sensors_enabled(&msg);
    uint32_t onboard_control_sensors_health = mavlink_msg_sys_status_get_onboard_control_sensors_health(&msg);

    diagnostic_data_.gyro = getSensorStatus(onboard_control_sensors_present, 
                                            onboard_control_sensors_enabled,
                                            onboard_control_sensors_health, 
                                            MAV_SYS_STATUS_SENSOR_3D_GYRO);
    diagnostic_data_.accelerometer = getSensorStatus(onboard_control_sensors_present,
                                                     onboard_control_sensors_enabled,
                                                     onboard_control_sensors_health,
                                                     MAV_SYS_STATUS_SENSOR_3D_ACCEL);
    diagnostic_data_.compass_0 = getSensorStatus(onboard_control_sensors_present,
                                                 onboard_control_sensors_enabled,
                                                 onboard_control_sensors_health,
                                                 MAV_SYS_STATUS_SENSOR_3D_MAG);
    diagnostic_data_.compass_1 = getSensorStatus(onboard_control_sensors_present,
                                                 onboard_control_sensors_enabled,
                                                 onboard_control_sensors_health,
                                                 MAV_SYS_STATUS_SENSOR_3D_MAG2);
}

void MAVLinkCollector::handleBatteryStatus(const mavlink_message_t& msg) {
    mavlink_battery_status_t battery_status;
    mavlink_msg_battery_status_decode(&msg, &battery_status);

    BatteryStatus status = {};
    status.id = battery_status.id;
    status.battery_function = battery_status.battery_function;
    status.type = battery_status.type;
    status.temperature = battery_status.temperature;
    status.current_battery = battery_status.current_battery;
    status.current_consumed = battery_status.current_consumed;
    status.energy_consumed = battery_status.energy_consumed;
    status.battery_remaining = battery_status.battery_remaining;

    // Store cell voltages (cells 1-10)
    status.voltages.clear();
    for (int i = 0; i < 10; i++) {
        if (battery_status.voltages[i] != UINT16_MAX) {
            status.voltages.push_back(battery_status.voltages[i]);
        }
    }

    // MAVLink v2 specific fields - check message length to determine if v2 fields are present
    if (msg.len >= 41) {  // v2 message has additional fields
        status.time_remaining = battery_status.time_remaining;
        status.charge_state = battery_status.charge_state;
        status.mode = battery_status.mode;
        status.fault_bitmask = battery_status.fault_bitmask;

        // Store extended cell voltages (cells 11-14) for v2
        status.voltages_ext.clear();
        for (int i = 0; i < 4; i++) {
            if (battery_status.voltages_ext[i] != 0 && battery_status.voltages_ext[i] != UINT16_MAX) {
                status.voltages_ext.push_back(battery_status.voltages_ext[i]);
            }
        }
    }

    std::lock_guard<std::mutex> lock(diagnose_mutex_);
    // Map battery by component ID from message
    uint8_t battery_key = status.id;
    if (msg.compid == MAV_COMP_ID_BATTERY) {
        battery_key = 1;
    } else if (msg.compid == MAV_COMP_ID_BATTERY2) {
        battery_key = 2;
    }
    diagnostic_data_.battery_status_map[battery_key] = status;

    if (config_.verbose) {
        std::cout << "[Collector] Battery Status " << (int)battery_key
                  << " (compid: " << (int)msg.compid << ")"
                  << " - Voltage: " << (battery_status.voltages[0] == UINT16_MAX ? 0.0f : battery_status.voltages[0] / 1000.0f) << "V"
                  << ", Current: " << (battery_status.current_battery / 100.0f) << "A"
                  << ", Remaining: " << (int)battery_status.battery_remaining << "%";
        if (msg.len >= 41) {
            std::cout << ", Time Remaining: " << battery_status.time_remaining << "s"
                      << ", Charge State: " << (int)battery_status.charge_state;
        }
        std::cout << std::endl;
    }
}

void MAVLinkCollector::handlePowerStatus(const mavlink_message_t& msg) {
    mavlink_power_status_t power_status;
    mavlink_msg_power_status_decode(&msg, &power_status);

    std::lock_guard<std::mutex> lock(diagnose_mutex_);
    diagnostic_data_.power_status.Vcc = power_status.Vcc;
    diagnostic_data_.power_status.Vservo = power_status.Vservo;
    diagnostic_data_.power_status.flags = power_status.flags;

    if (config_.verbose) {
        std::cout << "[Collector] Power Status - Vcc: " << (power_status.Vcc / 1000.0f) << "V"
                  << ", Vservo: " << (power_status.Vservo / 1000.0f) << "V"
                  << ", Flags: " << (int)power_status.flags << std::endl;
    }
}

void MAVLinkCollector::handleComponentInformationBasic(const mavlink_message_t& msg) {
    mavlink_component_information_basic_t comp_info;
    mavlink_msg_component_information_basic_decode(&msg, &comp_info);

    if (config_.verbose) {
        std::cout << "[Collector] Component Information Basic received:" << std::endl;
        std::cout << "  Vendor: " << std::string((char*)comp_info.vendor_name, 32) << std::endl;
        std::cout << "  Model: " << std::string((char*)comp_info.model_name, 32) << std::endl;
        std::cout << "  Software Version: " << std::string((char*)comp_info.software_version, 24) << std::endl;
        std::cout << "  Hardware Version: " << std::string((char*)comp_info.hardware_version, 24) << std::endl;
    }

    // Store component information in vehicle data
    vehicle_data_.vendor_name = std::string((char*)comp_info.vendor_name, 32);
    vehicle_data_.component_model_name = std::string((char*)comp_info.model_name, 32);
    vehicle_data_.software_version = std::string((char*)comp_info.software_version, 24);
    vehicle_data_.hardware_version = std::string((char*)comp_info.hardware_version, 24);

    // Clean up any null terminators
    vehicle_data_.vendor_name = vehicle_data_.vendor_name.c_str();
    vehicle_data_.component_model_name = vehicle_data_.component_model_name.c_str();
    vehicle_data_.software_version = vehicle_data_.software_version.c_str();
    vehicle_data_.hardware_version = vehicle_data_.hardware_version.c_str();
}

void MAVLinkCollector::handleBatteryInfo(const mavlink_message_t& msg) {
    BatteryInfo info = {};
    uint8_t battery_id = 0;

    if (msg.msgid == 370) {
        // MAVLink v1: SMART_BATTERY_INFO (ID: 370)
        mavlink_smart_battery_info_t battery;
        mavlink_msg_smart_battery_info_decode(&msg, &battery);

        info.id = battery.id;
        info.battery_function = battery.battery_function;
        info.type = battery.type;
        info.state_of_health = 0;  // Not available in v1 SMART_BATTERY_INFO
        info.cells_in_series = battery.cells_in_series;
        info.cycle_count = battery.cycle_count;
        info.weight = battery.weight;
        info.discharge_minimum_voltage = battery.discharge_minimum_voltage / 1000.0f;  // mV to V
        info.charging_minimum_voltage = battery.charging_minimum_voltage / 1000.0f;
        info.resting_minimum_voltage = battery.resting_minimum_voltage / 1000.0f;
        info.charging_maximum_voltage = battery.charging_maximum_voltage / 1000.0f;  // v1 has this field
        info.charging_maximum_current = 0.0f;  // Not in v1
        info.nominal_voltage = 0.0f;  // Not in v1
        info.discharge_maximum_current = battery.discharge_maximum_current / 1000.0f;  // mA to A
        info.discharge_maximum_burst_current = battery.discharge_maximum_burst_current / 1000.0f;
        info.design_capacity = battery.capacity_full_specification / 1000.0f;  // mAh to Ah
        info.full_charge_capacity = battery.capacity_full / 1000.0f;
        info.manufacture_date = std::string(battery.manufacture_date);
        info.serial_number = std::string(battery.serial_number);
        info.name = std::string(battery.device_name);

        if (config_.verbose) {
            std::cout << "[VERBOSE] Received MAVLink v1 SMART_BATTERY_INFO (ID: 370) for battery " 
                      << (int)info.id << std::endl;
        }

    } else if (msg.msgid == 372) {
        // MAVLink v2 BATTERY_INFO
        mavlink_battery_info_t battery;
        mavlink_msg_battery_info_decode(&msg, &battery);

        battery_id = battery.id;
        info.id = battery.id;
        info.battery_function = battery.battery_function;
        info.type = battery.type;
        info.state_of_health = battery.state_of_health;
        info.cells_in_series = battery.cells_in_series;
        info.cycle_count = battery.cycle_count;
        info.weight = battery.weight;
        info.discharge_minimum_voltage = battery.discharge_minimum_voltage;
        info.charging_minimum_voltage = battery.charging_minimum_voltage;
        info.resting_minimum_voltage = battery.resting_minimum_voltage;
        info.charging_maximum_voltage = battery.charging_maximum_voltage;
        info.charging_maximum_current = battery.charging_maximum_current;
        info.nominal_voltage = battery.nominal_voltage;
        info.discharge_maximum_current = battery.discharge_maximum_current;
        info.discharge_maximum_burst_current = battery.discharge_maximum_burst_current;
        info.design_capacity = battery.design_capacity;
        info.full_charge_capacity = battery.full_charge_capacity;

        // String fields
        info.manufacture_date = std::string(battery.manufacture_date);
        info.serial_number = std::string(battery.serial_number);
        info.name = std::string(battery.name);
    }

    // Map battery by component ID from message
    uint8_t battery_key = battery_id;

    // If component ID indicates specific battery, use that
    if (msg.compid == MAV_COMP_ID_BATTERY) {
        battery_key = 1;
    } else if (msg.compid == MAV_COMP_ID_BATTERY2) {
        battery_key = 2;
    }

    std::lock_guard<std::mutex> lock(diagnose_mutex_);
    diagnostic_data_.battery_info_map[battery_key] = info;

    if (config_.verbose) {
        std::cout << "[Collector] Battery " << (int)battery_key 
                  << " (compid: " << (int)msg.compid << ")"
                  << " info - Type: " << (int)info.type 
                  << ", Cells: " << (int)info.cells_in_series 
                  << ", Nominal: " << info.nominal_voltage << "V" << std::endl;
    }
}

std::string MAVLinkCollector::formatDuration(const std::chrono::system_clock::time_point& time_point) {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - time_point);

    double seconds = duration.count() / 1000.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << seconds << "s ago";
    return ss.str();
}

double MAVLinkCollector::calculateMessagesPerSecond() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - vehicle_data_.start_time).count();
    return elapsed > 0 ? static_cast<double>(vehicle_data_.messages_received) / elapsed : 0.0;
}

void MAVLinkCollector::logData() {
    nlohmann::json data;

    // Core Vehicle Data
    if (vehicle_data_.system_id > 0) {
        data["system_id"] = vehicle_data_.system_id;
    } else {
        data["system_id"] = "No Data Provided";
    }

    if (vehicle_data_.component_id > 0) {
        data["component_id"] = vehicle_data_.component_id;
    } else {
        data["component_id"] = "No Data Provided";
    }
    data["armed"] = vehicle_data_.armed;
    data["model"] = !vehicle_data_.model.empty() ? vehicle_data_.model : "No Data Provided";
    data["flight_mode"] = !vehicle_data_.flight_mode.empty() ? vehicle_data_.flight_mode : "No Data Provided";
    data["firmware"] = !vehicle_data_.firmware.empty() ? vehicle_data_.firmware : "No Data Provided";
    data["battery_voltage"] = (vehicle_data_.battery_voltage >= 0) ? nlohmann::json(vehicle_data_.battery_voltage) : nlohmann::json("No Data Provided");

    // Add time-based information
    auto time_since_heartbeat = std::chrono::system_clock::now() - vehicle_data_.last_heartbeat;
    if (vehicle_data_.last_heartbeat.time_since_epoch().count() > 0) {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time_since_heartbeat).count();
        data["heartbeat"] = std::to_string(seconds) + "s ago";
    } else {
        data["heartbeat"] = "No Data Provided";
    }

    auto time_since_activity = std::chrono::system_clock::now() - vehicle_data_.last_activity;
    if (vehicle_data_.last_activity.time_since_epoch().count() > 0) {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time_since_activity).count();
        data["last_activity"] = std::to_string(seconds) + "s ago";
    } else {
        data["last_activity"] = "No Data Provided";
    }

    data["messages_per_sec"] = calculateMessagesPerSecond();

    // Add software version from component information if available
    data["software_version"] = !vehicle_data_.software_version.empty() ? vehicle_data_.software_version : "No Data Provided";

    // Current timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    data["last_update"] = ss.str();

    log_json_ = data;
    saveLogToFile();
}

void MAVLinkCollector::registerMessageCallback(uint32_t msg_id, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    message_callbacks_[msg_id] = callback;
    if (config_.verbose) {
        std::cout << "[Collector] Registered callback for message ID: " << msg_id << std::endl;
    }
}

void MAVLinkCollector::unregisterMessageCallback(uint32_t msg_id) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    message_callbacks_.erase(msg_id);
    if (config_.verbose) {
        std::cout << "[Collector] Unregistered callback for message ID: " << msg_id << std::endl;
    }
}

void MAVLinkCollector::invokeCallbacks(const mavlink_message_t& msg) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    auto it = message_callbacks_.find(msg.msgid);
    if (it != message_callbacks_.end()) {
        if (config_.verbose) {
            std::cout << "[VERBOSE] Executing callback for msg ID " << msg.msgid << std::endl;
        }
        try {
            it->second(msg);
            if (config_.verbose) {
                std::cout << "[VERBOSE] Callback completed successfully for msg ID " << msg.msgid << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Collector] Callback exception for msg ID " << msg.msgid 
                      << ": " << e.what() << std::endl;
        }
    } else if (config_.verbose) {
        std::cout << "[VERBOSE] No callback registered for msg ID " << msg.msgid << std::endl;
    }
}

void MAVLinkCollector::setExpectedRate(uint32_t msg_id, double rate_hz) {
    std::lock_guard<std::mutex> lock(rates_mutex_);
    if (message_rates_.find(msg_id) == message_rates_.end()) {
        message_rates_[msg_id] = MessageRateInfo{0, {}, 0.0, rate_hz, std::chrono::steady_clock::now()};
    } else {
        message_rates_[msg_id].expected_rate_hz = rate_hz;
    }
    if (config_.verbose) {
        std::cout << "[Collector] Set expected rate for msg ID " << msg_id 
                  << ": " << rate_hz << " Hz" << std::endl;
    }
}

void MAVLinkCollector::updateMessageRate(uint32_t msg_id) {
    std::lock_guard<std::mutex> lock(rates_mutex_);
    auto now = std::chrono::steady_clock::now();

    if (message_rates_.find(msg_id) == message_rates_.end()) {
        message_rates_[msg_id] = MessageRateInfo{0, {}, 0.0, 0.0, now};
        if (config_.verbose) {
            std::cout << "[VERBOSE] Created new rate tracking entry for msg ID " << msg_id << std::endl;
        }
    }

    auto& rate_info = message_rates_[msg_id];
    rate_info.count++;
    rate_info.timestamps.push_back(now);
    rate_info.last_update = now;

    if (config_.verbose) {
        std::cout << "[VERBOSE] Msg ID " << msg_id << " count: " << rate_info.count 
                  << ", timestamps: " << rate_info.timestamps.size() << std::endl;
    }

    if (rate_info.timestamps.size() > 100) {
        rate_info.timestamps.pop_front();
        if (config_.verbose) {
            std::cout << "[VERBOSE] Trimmed timestamp queue for msg ID " << msg_id << std::endl;
        }
    }
}

void MAVLinkCollector::calculateMessageRates() {
    std::lock_guard<std::mutex> lock(rates_mutex_);
    auto now = std::chrono::steady_clock::now();

    if (config_.verbose) {
        std::cout << "[VERBOSE] Calculating message rates for " << message_rates_.size() 
                  << " message types" << std::endl;
    }

    for (auto& pair : message_rates_) {
        auto& rate_info = pair.second;

        if (rate_info.timestamps.size() < 2) {
            rate_info.current_rate_hz = 0.0;
            if (config_.verbose) {
                std::cout << "[VERBOSE] Msg ID " << pair.first 
                          << ": insufficient timestamps (" << rate_info.timestamps.size() << ")" << std::endl;
            }
            continue;
        }

        auto cutoff_time = now - std::chrono::seconds(5);
        size_t removed = 0;
        while (!rate_info.timestamps.empty() && 
               rate_info.timestamps.front() < cutoff_time) {
            rate_info.timestamps.pop_front();
            removed++;
        }

        if (config_.verbose && removed > 0) {
            std::cout << "[VERBOSE] Msg ID " << pair.first << ": removed " << removed 
                      << " old timestamps" << std::endl;
        }

        if (rate_info.timestamps.size() >= 2) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                rate_info.timestamps.back() - rate_info.timestamps.front()
            ).count();

            if (duration > 0) {
                double old_rate = rate_info.current_rate_hz;
                rate_info.current_rate_hz = 
                    (rate_info.timestamps.size() - 1) * 1000.0 / duration;

                if (config_.verbose) {
                    std::cout << "[VERBOSE] Msg ID " << pair.first 
                              << ": rate " << old_rate << " Hz -> " << rate_info.current_rate_hz 
                              << " Hz (duration: " << duration << "ms, samples: " 
                              << rate_info.timestamps.size() << ")" << std::endl;
                }
            }
        }
    }
}

MAVLinkCollector::MessageRateInfo MAVLinkCollector::getMessageRateInfo(uint32_t msg_id) {
    std::lock_guard<std::mutex> lock(rates_mutex_);
    auto it = message_rates_.find(msg_id);
    if (it != message_rates_.end()) {
        return it->second;
    }
    return MessageRateInfo{0, {}, 0.0, 0.0, std::chrono::steady_clock::now()};
}

std::map<uint32_t, MAVLinkCollector::MessageRateInfo> MAVLinkCollector::getAllMessageRates() {
    std::lock_guard<std::mutex> lock(rates_mutex_);
    return message_rates_;
}

void MAVLinkCollector::loadExistingDiagnoseLog() {
    std::ifstream log_file(config_.diagnose_data_logfile);
    if (log_file.is_open()) {
        try {
            log_file >> diagnose_log_json_;
            std::cout << "[Collector] Loaded existing diagnose log: " << config_.diagnose_data_logfile << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Collector] Failed to parse diagnose log, creating new: " << e.what() << std::endl;
            diagnose_log_json_ = nlohmann::json::object();
        }
    } else {
        std::cout << "[Collector] Creating new diagnose log: " << config_.diagnose_data_logfile << std::endl;
        diagnose_log_json_ = nlohmann::json::object();
    }
}

void MAVLinkCollector::saveDiagnoseLogToFile() {
    // Clear file content by opening in truncate mode
    std::ofstream log_file(config_.diagnose_data_logfile, std::ios::out | std::ios::trunc);
    if (log_file.is_open()) {
        log_file << diagnose_log_json_.dump(2) << std::endl;
        log_file.close();
    } else {
        std::cerr << "[Collector] Failed to save diagnose log file: " << config_.diagnose_data_logfile << std::endl;
    }
}

void MAVLinkCollector::requestDiagnosticData() {
    if (vehicle_data_.system_id == 0) {
        return;
    }

    mavlink_message_t msg;

    // Request parameter list to get diagnostic configuration
    mavlink_msg_param_request_list_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        vehicle_data_.system_id,
        vehicle_data_.component_id
    );

    if (config_.verbose) {
        std::cout << "[VERBOSE] Requesting parameter list for diagnostics" << std::endl;
    }

    sendMavlinkMessage(msg);
}

std::string MAVLinkCollector::getSensorStatus(uint32_t sensor_present, uint32_t sensor_enabled, 
                                               uint32_t sensor_health, uint32_t bit) {
    bool present = sensor_present & (1 << bit);
    bool enabled = sensor_enabled & (1 << bit);
    bool healthy = sensor_health & (1 << bit);

    if (!present) return "Not Present";
    if (!enabled) return "Disabled";
    if (!healthy) return "Unhealthy";
    return "OK";
}

void MAVLinkCollector::handleParamValue(const mavlink_message_t& msg) {
    mavlink_param_value_t param;
    mavlink_msg_param_value_decode(&msg, &param);

    char param_id[17] = {0};
    memcpy(param_id, param.param_id, 16);

    if (config_.verbose) {
        std::cout << "[Collector] Parameter: " << param_id 
                  << " = " << param.param_value
                  << " (type=" << (int)param.param_type
                  << ", index=" << param.param_index
                  << "/" << param.param_count << ")" << std::endl;
    }

    // Store parameter if stats counter is enabled
    if (config_.mav_stats_counter) {
        std::lock_guard<std::mutex> lock(params_mutex_);
        ParameterInfo param_info;
        param_info.name = std::string(param_id);
        param_info.value = param.param_value;
        param_info.type = param.param_type;
        param_info.timestamp = std::chrono::system_clock::now();

        collected_parameters_[param_info.name] = param_info;

        if (config_.verbose) {
            std::cout << "[Collector] Stored parameter " << param_info.name 
                      << " (total: " << collected_parameters_.size() << ")" << std::endl;
        }
    }

    // Store in diagnostic data based on parameter name
    std::string param_name(param_id);

    if (param_name == "FRAME_TYPE" || param_name == "FRAME_CLASS") {
        diagnostic_data_.airframe_type = std::to_string(static_cast<int>(param.param_value));
    } else if (param_name == "SYSID_THISMAV") {
        // Vehicle system ID
    } else if (param_name.find("COMPASS") != std::string::npos) {
        if (param_name.find("EXTERNAL") != std::string::npos) {
            diagnostic_data_.compass_0 = param_name + "=" + std::to_string(param.param_value);
        }
    } else if (param_name.find("BATT") != std::string::npos) {
        if (param_name == "BATT_CAPACITY") {
            // Store battery capacity
        } else if (param_name == "BATT_LOW_VOLT") {
            diagnostic_data_.battery_empty_voltage = param.param_value;
        }
    } else if (param_name.find("FLTMODE") != std::string::npos) {
        // Flight mode parameters
        if (param_name == "FLTMODE1") diagnostic_data_.flight_mode_1 = std::to_string(static_cast<int>(param.param_value));
        else if (param_name == "FLTMODE2") diagnostic_data_.flight_mode_2 = std::to_string(static_cast<int>(param.param_value));
        else if (param_name == "FLTMODE3") diagnostic_data_.flight_mode_3 = std::to_string(static_cast<int>(param.param_value));
        else if (param_name == "FLTMODE4") diagnostic_data_.flight_mode_4 = std::to_string(static_cast<int>(param.param_value));
        else if (param_name == "FLTMODE5") diagnostic_data_.flight_mode_5 = std::to_string(static_cast<int>(param.param_value));
        else if (param_name == "FLTMODE6") diagnostic_data_.flight_mode_6 = std::to_string(static_cast<int>(param.param_value));
    } else if (param_name.find("FS_") != std::string::npos) {
        // Failsafe parameters
        if (param_name == "FS_BATT_ENABLE") {
            diagnostic_data_.low_battery_failsafe = std::to_string(static_cast<int>(param.param_value));
        } else if (param_name == "FS_THR_ENABLE") {
            diagnostic_data_.rc_loss_failsafe = std::to_string(static_cast<int>(param.param_value));
        }
    }
    // Check if this is the last parameter
    if (param.param_index + 1 == param.param_count) {
        std::cout << "[Collector] ✓ All " << param.param_count << " parameters received" << std::endl;
    }
}

void MAVLinkCollector::logDiagnosticData() {
    std::lock_guard<std::mutex> lock(diagnose_mutex_);

    nlohmann::json diagnose_json;

    // Airframe section
    nlohmann::json airframe;
    if (vehicle_data_.system_id > 0) {
       airframe["system_id"] = vehicle_data_.system_id;
   } else {
       airframe["system_id"] = "No Data Provided";
   }
    airframe["vehicle"] = !vehicle_data_.model.empty() ? vehicle_data_.model : "No Data Provided";
    airframe["airframe_type"] = !diagnostic_data_.airframe_type.empty() ? diagnostic_data_.airframe_type : "No Data Provided";
    airframe["firmware_version"] = !diagnostic_data_.firmware_version.empty() ? diagnostic_data_.firmware_version : "No Data Provided";
    airframe["custom_fw_ver"] = !diagnostic_data_.custom_fw_ver.empty() ? diagnostic_data_.custom_fw_ver : "No Data Provided";
    diagnose_json["airframe"] = airframe;

    // Sensors section
    nlohmann::json sensors;
    sensors["compass_0"] = !diagnostic_data_.compass_0.empty() ? diagnostic_data_.compass_0 : "No Data Provided";
    sensors["compass_1"] = !diagnostic_data_.compass_1.empty() ? diagnostic_data_.compass_1 : "No Data Provided";
    sensors["gyro"] = !diagnostic_data_.gyro.empty() ? diagnostic_data_.gyro : "No Data Provided";
    sensors["accelerometer"] = !diagnostic_data_.accelerometer.empty() ? diagnostic_data_.accelerometer : "No Data Provided";
    diagnose_json["sensors"] = sensors;

    // Power section - extract from battery info and status with component awareness
    diagnose_json["power"] = nlohmann::json::object();

    // Primary battery (ID 1 or MAV_COMP_ID_BATTERY)
    if (diagnostic_data_.battery_info_map.count(1) > 0) {
        auto& battery_info = diagnostic_data_.battery_info_map[1];
        diagnose_json["power"]["battery_1_capacity"] = battery_info.design_capacity;
        diagnose_json["power"]["battery_1_full_voltage"] = battery_info.charging_maximum_voltage;
        diagnose_json["power"]["battery_1_empty_voltage"] = battery_info.discharge_minimum_voltage;
        diagnose_json["power"]["battery_1_cells"] = battery_info.cells_in_series;
        diagnose_json["power"]["battery_1_type"] = battery_info.type;
        diagnose_json["power"]["battery_1_name"] = battery_info.name;
    }

    if (diagnostic_data_.battery_status_map.count(1) > 0) {
        auto& battery_status = diagnostic_data_.battery_status_map[1];
        diagnose_json["power"]["battery_1_remaining"] = battery_status.battery_remaining;
        diagnose_json["power"]["battery_1_current_consumed"] = battery_status.current_consumed;
        diagnose_json["power"]["battery_1_voltage"] = battery_status.voltages.empty() ? 0 : (battery_status.voltages[0] / 1000.0f);
        if (battery_status.charge_state != 0) {
            diagnose_json["power"]["battery_1_charge_state"] = battery_status.charge_state;
        }
    }

    // Secondary battery (ID 2 or MAV_COMP_ID_BATTERY2)
    if (diagnostic_data_.battery_info_map.count(2) > 0) {
        auto& battery_info = diagnostic_data_.battery_info_map[2];
        diagnose_json["power"]["battery_2_capacity"] = battery_info.design_capacity;
        diagnose_json["power"]["battery_2_full_voltage"] = battery_info.charging_maximum_voltage;
        diagnose_json["power"]["battery_2_empty_voltage"] = battery_info.discharge_minimum_voltage;
        diagnose_json["power"]["battery_2_cells"] = battery_info.cells_in_series;
        diagnose_json["power"]["battery_2_type"] = battery_info.type;
        diagnose_json["power"]["battery_2_name"] = battery_info.name;
    }

    if (diagnostic_data_.battery_status_map.count(2) > 0) {
        auto& battery_status = diagnostic_data_.battery_status_map[2];
        diagnose_json["power"]["battery_2_remaining"] = battery_status.battery_remaining;
        diagnose_json["power"]["battery_2_current_consumed"] = battery_status.current_consumed;
        diagnose_json["power"]["battery_2_voltage"] = battery_status.voltages.empty() ? 0 : (battery_status.voltages[0] / 1000.0f);
        if (battery_status.charge_state != 0) {
            diagnose_json["power"]["battery_2_charge_state"] = battery_status.charge_state;
        }
    }

    // Safety section
    nlohmann::json safety;
    safety["low_battery_failsafe"] = !diagnostic_data_.low_battery_failsafe.empty() ? diagnostic_data_.low_battery_failsafe : "No Data Provided";
    safety["rc_loss_failsafe"] = !diagnostic_data_.rc_loss_failsafe.empty() ? diagnostic_data_.rc_loss_failsafe : "No Data Provided";
    safety["rc_loss_timeout"] = (diagnostic_data_.rc_loss_timeout > 0) ? std::to_string(diagnostic_data_.rc_loss_timeout) + " s" : "No Data Provided";
    safety["data_link_loss_failsafe"] = !diagnostic_data_.data_link_loss_failsafe.empty() ? diagnostic_data_.data_link_loss_failsafe : "No Data Provided";
    safety["rtl_climb_to"] = (diagnostic_data_.rtl_climb_to > 0) ? std::to_string(diagnostic_data_.rtl_climb_to) + " m" : "No Data Provided";
    safety["rtl_then"] = !diagnostic_data_.rtl_then.empty() ? diagnostic_data_.rtl_then : "No Data Provided";
    diagnose_json["safety"] = safety;

    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    diagnose_json["timestamp"] = ss.str();

    // Battery status data
    if (!diagnostic_data_.battery_status_map.empty()) {
        nlohmann::json battery_status_array = nlohmann::json::array();
        for (const auto& [id, status] : diagnostic_data_.battery_status_map) {
            nlohmann::json battery_obj;
            battery_obj["id"] = id;
            battery_obj["battery_function"] = status.battery_function;
            battery_obj["type"] = status.type;
            battery_obj["temperature_cdegC"] = status.temperature;
            battery_obj["current_battery_cA"] = status.current_battery;
            battery_obj["current_consumed_mAh"] = status.current_consumed;
            battery_obj["energy_consumed_hJ"] = status.energy_consumed;
            battery_obj["battery_remaining_percent"] = status.battery_remaining;
            battery_obj["voltages_mV"] = status.voltages;

            // Add MAVLink v2 specific fields if present
            if (!status.voltages_ext.empty()) {
                battery_obj["voltages_ext_mV"] = status.voltages_ext;
            }
            if (status.time_remaining != 0) {
                battery_obj["time_remaining_s"] = status.time_remaining;
            }
            battery_obj["charge_state"] = status.charge_state;
            battery_obj["mode"] = status.mode;
            battery_obj["fault_bitmask"] = status.fault_bitmask;

            battery_status_array.push_back(battery_obj);
        }
        diagnose_json["battery_status"] = battery_status_array;
    }

    // Power status data
    nlohmann::json power_obj;
    power_obj["Vcc_mV"] = diagnostic_data_.power_status.Vcc;
    power_obj["Vservo_mV"] = diagnostic_data_.power_status.Vservo;
    power_obj["flags"] = diagnostic_data_.power_status.flags;
    diagnose_json["power_status"] = power_obj;

    diagnose_log_json_ = diagnose_json;
    saveDiagnoseLogToFile();
}

void MAVLinkCollector::requestHighFrequencyStreamsV1() {
    requestStreamRateV1(MAV_DATA_STREAM_EXTENDED_STATUS, 10);
}

void MAVLinkCollector::requestStreamRateV1(uint8_t stream_id, uint16_t rate_hz) {
    mavlink_message_t msg;

    mavlink_msg_request_data_stream_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        vehicle_data_.system_id > 0 ? vehicle_data_.system_id : 0,
        vehicle_data_.component_id > 0 ? vehicle_data_.component_id : 0,
        stream_id,
        rate_hz,
        1  // start streaming
    );

    if (config_.verbose) {
        std::cout << "[Collector] Requesting v1 stream " << (int)stream_id 
                  << " at " << rate_hz << " Hz" << std::endl;
    }

    sendMavlinkMessage(msg);
}

void MAVLinkCollector::requestHighFrequencyStreamsV2() {
    requestMessageIntervalV2(MAVLINK_MSG_ID_SYS_STATUS, 10000);
    requestMessageIntervalV2(MAVLINK_MSG_ID_BATTERY_STATUS, 10000);
    requestMessageIntervalV2(MAVLINK_MSG_ID_POWER_STATUS, 10000); // Request POWER_STATUS at 10Hz
}

void MAVLinkCollector::requestMessageIntervalV2(uint32_t message_id, int32_t interval_us) {
    mavlink_message_t msg;

    mavlink_msg_command_long_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        vehicle_data_.system_id > 0 ? vehicle_data_.system_id : 0,
        vehicle_data_.component_id > 0 ? vehicle_data_.component_id : 0,
        MAV_CMD_SET_MESSAGE_INTERVAL,
        0,  // confirmation
        message_id,        // param1: message ID
        interval_us,       // param2: interval in microseconds
        0, 0, 0, 0, 0     // remaining params unused
    );

    if (config_.verbose) {
        std::cout << "[Collector] Requesting v2 message " << message_id 
                  << " at interval " << interval_us << " us" << std::endl;
    }

    sendMavlinkMessage(msg);
}

void MAVLinkCollector::requestAllParameters() {
    if (vehicle_data_.system_id == 0) {
        std::cout << "[Collector] Cannot request parameters: no vehicle detected" << std::endl;
        return;
    }

    mavlink_message_t msg;

    // Request all parameters using PARAM_REQUEST_LIST
    mavlink_msg_param_request_list_pack(
        config_.request_sysid,
        config_.request_compid,
        &msg,
        vehicle_data_.system_id,
        vehicle_data_.component_id
    );

    if (sendMavlinkMessage(msg)) {
        std::cout << "[Collector] ✓ PARAM_REQUEST_LIST sent to sysid=" 
                  << (int)vehicle_data_.system_id 
                  << " compid=" << (int)vehicle_data_.component_id << std::endl;
    } else {
        std::cout << "[Collector] ✗ Failed to send PARAM_REQUEST_LIST" << std::endl;
    }
}

void MAVLinkCollector::saveParametersToFile() {
    if (!config_.mav_stats_counter) {
        return;
    }

    std::lock_guard<std::mutex> lock(params_mutex_);

    nlohmann::json params_json;
    params_json["collection_timestamp"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    params_json["parameters"] = nlohmann::json::array();

    for (const auto& [name, param] : collected_parameters_) {
        nlohmann::json param_entry;
        param_entry["name"] = name;
        param_entry["value"] = param.value;
        param_entry["type"] = param.type;
        param_entry["timestamp"] = std::chrono::system_clock::to_time_t(param.timestamp);

        // Add readable timestamp
        std::time_t t = std::chrono::system_clock::to_time_t(param.timestamp);
        std::tm tm = *std::localtime(&t);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);
        param_entry["timestamp_readable"] = time_str;

        params_json["parameters"].push_back(param_entry);
    }

    std::ofstream params_file(params_log_file_);
    if (params_file.is_open()) {
        params_file << params_json.dump(2) << std::endl;
        params_file.close();
        std::cout << "[Collector] Saved " << collected_parameters_.size() 
                  << " parameters to " << params_log_file_ << std::endl;
    } else {
        std::cerr << "[Collector] Failed to save parameters file: " << params_log_file_ << std::endl;
    }
}

void MAVLinkCollector::printMessageStats() {
    if (!config_.mav_stats_counter) {
        std::cout << "[Collector] Message statistics disabled in config" << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);

    std::cout << "\n=== MAVLink Message Statistics ===" << std::endl;
    std::cout << "Total messages received: " << total_messages_received_ << std::endl;
    std::cout << "\nMessage breakdown:" << std::endl;

    for (const auto& [msg_id, count] : message_stats_) {
        std::cout << "  MSG ID " << msg_id << ": " << count << " messages" << std::endl;
    }

    std::cout << "==================================\n" << std::endl;
}
