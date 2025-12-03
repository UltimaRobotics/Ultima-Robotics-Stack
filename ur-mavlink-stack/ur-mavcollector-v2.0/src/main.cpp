#include "MavlinkUdpConnection.h"
#include "Vehicle.h"
#include "BoardIdentifier.h"
#include <iostream>
#include <string>
#include <getopt.h>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include "../thirdparty/nlohmann/json.hpp"

using json = nlohmann::json;

std::atomic<bool> g_running(true);
Vehicle vehicle;
bool verbose_mode = false;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " -a <address> -p <port> [-s <system_id>] [-c <component_id>] [-v]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -a, --address       UDP address to bind to (default: 0.0.0.0)" << std::endl;
    std::cout << "  -p, --port          UDP port to bind to (default: 14550)" << std::endl;
    std::cout << "  -s, --system-id     MAVLink system ID (default: 255)" << std::endl;
    std::cout << "  -c, --component-id  MAVLink component ID (default: 190)" << std::endl;
    std::cout << "  -v, --verbose       Enable verbose logging output" << std::endl;
    std::cout << "  -h, --help          Show this help message" << std::endl;
}

std::string getAutopilotName(uint8_t autopilot) {
    switch (autopilot) {
        case MAV_AUTOPILOT_GENERIC:
            return "Generic";
        case MAV_AUTOPILOT_ARDUPILOTMEGA:
            return "ArduPilot";
        case MAV_AUTOPILOT_PX4:
            return "PX4";
        default:
            return "Other";
    }
}

std::string getVehicleTypeName(uint8_t type) {
    switch (type) {
        case MAV_TYPE_GENERIC:
            return "Generic";
        case MAV_TYPE_QUADROTOR:
            return "Quadrotor";
        case MAV_TYPE_COAXIAL:
            return "Coaxial";
        case MAV_TYPE_HELICOPTER:
            return "Helicopter";
        case MAV_TYPE_FIXED_WING:
            return "Fixed Wing";
        case MAV_TYPE_GROUND_ROVER:
            return "Ground Rover";
        case MAV_TYPE_SURFACE_BOAT:
            return "Surface Boat";
        case MAV_TYPE_SUBMARINE:
            return "Submarine";
        default:
            return "Other";
    }
}

int main(int argc, char* argv[]) {
    std::string address = "0.0.0.0";
    uint16_t port = 14550;
    uint8_t system_id = 255;
    uint8_t component_id = MAV_COMP_ID_MISSIONPLANNER;
    
    static struct option long_options[] = {
        {"address", required_argument, 0, 'a'},
        {"port", required_argument, 0, 'p'},
        {"system-id", required_argument, 0, 's'},
        {"component-id", required_argument, 0, 'c'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "a:p:s:c:vh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'a':
                address = std::string(optarg);
                break;
            case 'p':
                port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 's':
                system_id = static_cast<uint8_t>(std::stoi(optarg));
                break;
            case 'c':
                component_id = static_cast<uint8_t>(std::stoi(optarg));
                break;
            case 'v':
                verbose_mode = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            case '?':
                std::cerr << "Unknown option. Use -h for help." << std::endl;
                return 1;
            default:
                break;
        }
    }
    
    if (optind < argc) {
        std::cerr << "Unexpected arguments: ";
        for (int i = optind; i < argc; ++i) {
            std::cerr << argv[i] << " ";
        }
        std::cerr << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    MavlinkUdpConnection connection(system_id, component_id);
    
    if (!connection.connect(address, port)) {
        if (verbose_mode) {
            std::cerr << "Failed to connect to " << address << ":" << port << std::endl;
        }
        return 1;
    }
    
    if (verbose_mode) {
        std::cout << "Connected to " << address << ":" << port 
                  << " with system ID " << static_cast<int>(system_id) 
                  << " and component ID " << static_cast<int>(component_id) << std::endl;
    }
    
    connection.setHeartbeatCallback([](const MavlinkHeartbeatInfo& info) {
        json heartbeat_json = {
            {"type", "heartbeat"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"system_id", info.system_id},
            {"component_id", info.component_id},
            {"vehicle_type", {
                {"name", getVehicleTypeName(info.type)},
                {"id", info.type}
            }},
            {"autopilot", {
                {"name", getAutopilotName(info.autopilot)},
                {"id", info.autopilot}
            }},
            {"base_mode", info.base_mode},
            {"custom_mode", info.custom_mode},
            {"system_status", info.system_status}
        };
        
        if (vehicle.hasAutopilotVersion()) {
            heartbeat_json["board_identification"] = vehicle.getBoardIdentification();
        }
        
        std::cout << heartbeat_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "=== HEARTBEAT RECEIVED ===" << std::endl;
            std::cout << "System ID: " << (int)info.system_id << std::endl;
            std::cout << "Component ID: " << (int)info.component_id << std::endl;
            std::cout << "Vehicle Type: " << getVehicleTypeName(info.type) << " (" << (int)info.type << ")" << std::endl;
            std::cout << "Autopilot: " << getAutopilotName(info.autopilot) << " (" << (int)info.autopilot << ")" << std::endl;
            std::cout << "Base Mode: 0x" << std::hex << (int)info.base_mode << std::dec << std::endl;
            std::cout << "Custom Mode: " << info.custom_mode << std::endl;
            std::cout << "System Status: " << (int)info.system_status << std::endl;
            
            if (vehicle.hasAutopilotVersion()) {
                std::cout << "Board: " << vehicle.getBoardIdentification() << std::endl;
            }
            std::cout << "=========================" << std::endl;
        }
    });
    
    connection.setAutopilotVersionCallback([](const MavlinkAutopilotVersionInfo& info) {
        json autopilot_json = {
            {"type", "autopilot_version"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"capabilities", info.capabilitiesString()},
            {"hardware_uid", {
                {"legacy", info.uid},
                {"extended", info.uid2String()}
            }},
            {"flight_software", {
                {"version", info.flightSwVersionString()},
                {"git_hash", info.flightCustomVersionString()}
            }},
            {"middleware_software", {
                {"version", info.middlewareSwVersionString()},
                {"git_hash", info.middlewareCustomVersionString()}
            }},
            {"os_software", {
                {"version", info.osSwVersionString()},
                {"git_hash", info.osCustomVersionString()}
            }},
            {"hardware", {
                {"board_version", info.boardVersionString()},
                {"vendor_id", info.vendor_id},
                {"product_id", info.product_id},
                {"identification", BoardIdentifier::instance().identifyBoard(info.vendor_id, info.product_id)},
                {"class", BoardIdentifier::instance().getBoardClass(info.vendor_id, info.product_id)},
                {"name", BoardIdentifier::instance().getBoardName(info.vendor_id, info.product_id)}
            }}
        };
        
        std::cout << autopilot_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "\n=== AUTOPILOT VERSION RECEIVED ===" << std::endl;
            std::cout << "Capabilities: " << info.capabilitiesString() << std::endl;
            std::cout << "Hardware UID (legacy): 0x" << std::hex << info.uid << std::dec << std::endl;
            std::cout << "Flight SW Version: " << info.flightSwVersionString() << std::endl;
            std::cout << "Flight Git Hash: " << info.flightCustomVersionString() << std::endl;
            std::cout << "Middleware SW Version: " << info.middlewareSwVersionString() << std::endl;
            std::cout << "Middleware Git Hash: " << info.middlewareCustomVersionString() << std::endl;
            std::cout << "OS SW Version: " << info.osSwVersionString() << std::endl;
            std::cout << "OS Git Hash: " << info.osCustomVersionString() << std::endl;
            std::cout << "Board Version: " << info.boardVersionString() << std::endl;
            std::cout << "Vendor ID: " << info.vendor_id << std::endl;
            std::cout << "Product ID: " << info.product_id << std::endl;
            std::cout << "Hardware UID (extended): " << info.uid2String() << std::endl;
            std::cout << "Board Identification: " << BoardIdentifier::instance().identifyBoard(info.vendor_id, info.product_id) << std::endl;
            std::cout << "===================================" << std::endl;
        }
        
        vehicle.setAutopilotVersionInfo(info);
    });
    
    connection.startReceiving();
    
    if (verbose_mode) {
        std::cout << "MAVLink UDP Collector started on " << address << ":" << port << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
    }
    
    auto last_heartbeat_sent = std::chrono::steady_clock::now();
    const auto heartbeat_interval = std::chrono::seconds(1);
    
    // Don't request autopilot version immediately - wait for first heartbeat
    // to establish remote address
    bool received_first_heartbeat = false;
    bool data_collection_complete = false;
    
    connection.setHeartbeatCallback([&received_first_heartbeat, &connection, &data_collection_complete](const MavlinkHeartbeatInfo& info) {
        // Set flag when we receive the first heartbeat
        static bool firstHeartbeat = true;
        if (firstHeartbeat) {
            if (verbose_mode) {
                std::cout << "First heartbeat received, requesting autopilot version information..." << std::endl;
            }
            connection.requestAutopilotVersion();
            firstHeartbeat = false;
        }
        received_first_heartbeat = true;
        
        json heartbeat_json = {
            {"type", "heartbeat"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"system_id", info.system_id},
            {"component_id", info.component_id},
            {"vehicle_type", {
                {"name", getVehicleTypeName(info.type)},
                {"id", info.type}
            }},
            {"autopilot", {
                {"name", getAutopilotName(info.autopilot)},
                {"id", info.autopilot}
            }},
            {"base_mode", info.base_mode},
            {"custom_mode", info.custom_mode},
            {"system_status", info.system_status},
            {"board_identification", vehicle.getBoardIdentification()}
        };
        std::cout << heartbeat_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "=== HEARTBEAT RECEIVED ===" << std::endl;
            std::cout << "System ID: " << static_cast<int>(info.system_id) << std::endl;
            std::cout << "Component ID: " << static_cast<int>(info.component_id) << std::endl;
            std::cout << "Vehicle Type: " << getVehicleTypeName(info.type) << " (" << static_cast<int>(info.type) << ")" << std::endl;
            std::cout << "Autopilot: " << getAutopilotName(info.autopilot) << " (" << static_cast<int>(info.autopilot) << ")" << std::endl;
            std::cout << "Base Mode: 0x" << std::hex << static_cast<int>(info.base_mode) << std::dec << std::endl;
            std::cout << "Custom Mode: " << info.custom_mode << std::endl;
            std::cout << "System Status: " << static_cast<int>(info.system_status) << std::endl;
            std::cout << "=========================" << std::endl;
        }
    });
    
    connection.setAutopilotVersionCallback([&data_collection_complete](const MavlinkAutopilotVersionInfo& info) {
        json autopilot_json = {
            {"type", "autopilot_version"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"capabilities", info.capabilitiesString()},
            {"hardware_uid", {
                {"legacy", info.uid},
                {"extended", info.uid2String()}
            }},
            {"flight_software", {
                {"version", info.flightSwVersionString()},
                {"git_hash", info.flightCustomVersionString()}
            }},
            {"middleware_software", {
                {"version", info.middlewareSwVersionString()},
                {"git_hash", info.middlewareCustomVersionString()}
            }},
            {"os_software", {
                {"version", info.osSwVersionString()},
                {"git_hash", info.osCustomVersionString()}
            }},
            {"hardware", {
                {"board_version", info.boardVersionString()},
                {"vendor_id", info.vendor_id},
                {"product_id", info.product_id},
                {"identification", BoardIdentifier::instance().identifyBoard(info.vendor_id, info.product_id)},
                {"class", BoardIdentifier::instance().getBoardClass(info.vendor_id, info.product_id)},
                {"name", BoardIdentifier::instance().getBoardName(info.vendor_id, info.product_id)}
            }}
        };
        std::cout << autopilot_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "\n=== AUTOPILOT VERSION RECEIVED ===" << std::endl;
            std::cout << "Capabilities: " << info.capabilitiesString() << std::endl;
            std::cout << "Hardware UID (legacy): " << info.uid << std::endl;
            std::cout << "Hardware UID (extended): " << info.uid2String() << std::endl;
            std::cout << "Board Identification: " << BoardIdentifier::instance().identifyBoard(info.vendor_id, info.product_id) << std::endl;
            std::cout << "===================================" << std::endl;
        }
        
        vehicle.setAutopilotVersionInfo(info);
        data_collection_complete = true;
    });
    
    connection.setBatteryInfoCallback([](const MavlinkBatteryInfo& info) {
        json battery_info_json = {
            {"type", "battery_info"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"battery_id", info.id},
            {"function", info.battery_function},
            {"type", info.type},
            {"state_of_health", info.state_of_health},
            {"cells_in_series", info.cells_in_series},
            {"cycle_count", info.cycle_count},
            {"weight", info.weight},
            {"voltages", {
                {"discharge_minimum", info.discharge_minimum_voltage},
                {"charging_minimum", info.charging_minimum_voltage},
                {"resting_minimum", info.resting_minimum_voltage},
                {"charging_maximum", info.charging_maximum_voltage},
                {"nominal", info.nominal_voltage}
            }},
            {"currents", {
                {"charging_maximum", info.charging_maximum_current},
                {"discharge_maximum", info.discharge_maximum_current},
                {"discharge_maximum_burst", info.discharge_maximum_burst_current}
            }},
            {"capacity", {
                {"design", info.design_capacity},
                {"full_charge", info.full_charge_capacity}
            }},
            {"manufacture_date", std::string(info.manufacture_date)},
            {"serial_number", std::string(info.serial_number)},
            {"name", std::string(info.name)}
        };
        
        std::cout << battery_info_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "\n=== BATTERY INFO RECEIVED ===" << std::endl;
            std::cout << "Battery ID: " << static_cast<int>(info.id) << std::endl;
            std::cout << "Function: " << static_cast<int>(info.battery_function) << std::endl;
            std::cout << "Type: " << static_cast<int>(info.type) << std::endl;
            std::cout << "State of Health: " << static_cast<int>(info.state_of_health) << "%" << std::endl;
            std::cout << "Cells in Series: " << static_cast<int>(info.cells_in_series) << std::endl;
            std::cout << "Cycle Count: " << info.cycle_count << std::endl;
            std::cout << "Weight: " << info.weight << "g" << std::endl;
            std::cout << "Design Capacity: " << info.design_capacity << "Ah" << std::endl;
            std::cout << "Full Charge Capacity: " << info.full_charge_capacity << "Ah" << std::endl;
            std::cout << "Serial Number: " << std::string(info.serial_number) << std::endl;
            std::cout << "Name: " << std::string(info.name) << std::endl;
            std::cout << "===============================" << std::endl;
        }
    });
    
    connection.setBatteryStatusCallback([](const MavlinkBatteryStatus& status) {
        // Create voltage array for JSON
        std::vector<uint16_t> voltages;
        std::vector<uint16_t> voltages_ext;
        for (int i = 0; i < 10; i++) {
            if (status.voltages[i] != UINT16_MAX) {
                voltages.push_back(status.voltages[i]);
            }
        }
        for (int i = 0; i < 4; i++) {
            if (status.voltages_ext[i] != 0) {
                voltages_ext.push_back(status.voltages_ext[i]);
            }
        }
        
        json battery_status_json = {
            {"type", "battery_status"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"battery_id", status.id},
            {"function", status.battery_function},
            {"type", status.type},
            {"temperature", status.temperature},
            {"voltages", voltages},
            {"voltages_ext", voltages_ext},
            {"current_battery", status.current_battery},
            {"current_consumed", status.current_consumed},
            {"energy_consumed", status.energy_consumed},
            {"battery_remaining", status.battery_remaining},
            {"time_remaining", status.time_remaining},
            {"charge_state", status.charge_state},
            {"mode", status.mode},
            {"fault_bitmask", status.fault_bitmask}
        };
        
        std::cout << battery_status_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "\n=== BATTERY STATUS RECEIVED ===" << std::endl;
            std::cout << "Battery ID: " << static_cast<int>(status.id) << std::endl;
            std::cout << "Temperature: " << status.temperature << "°C" << std::endl;
            std::cout << "Current: " << status.current_battery << "cA" << std::endl;
            std::cout << "Battery Remaining: " << static_cast<int>(status.battery_remaining) << "%" << std::endl;
            std::cout << "Time Remaining: " << status.time_remaining << "s" << std::endl;
            std::cout << "Charge State: " << static_cast<int>(status.charge_state) << std::endl;
            std::cout << "Mode: " << static_cast<int>(status.mode) << std::endl;
            std::cout << "Fault Bitmask: " << status.fault_bitmask << std::endl;
            std::cout << "================================" << std::endl;
        }
    });
    
    // GPS callback with comprehensive JSON output
    connection.setGPSDataCallback([](const MavlinkGPSData& gps) {
        // Create comprehensive GPS JSON matching gps-collector-api format
        json gps_json = {
            {"type", "gps_data"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"gps_state", static_cast<int>(gps.state)},
            {"gps_fix_type", static_cast<int>(gps.fix_type)},
            {"has_position", gps.has_position},
            {"has_origin", gps.has_origin},
            {"position", {
                {"latitude", gps.has_position ? gps.latitude / 1e7 : 0.0},
                {"longitude", gps.has_position ? gps.longitude / 1e7 : 0.0},
                {"altitude", gps.has_position ? gps.altitude / 1000.0f : 0.0},
                {"relative_altitude", gps.has_position ? gps.relative_altitude / 1000.0f : 0.0}
            }},
            {"raw_position", {
                {"latitude_degE7", gps.latitude},
                {"longitude_degE7", gps.longitude},
                {"altitude_mm", gps.altitude}
            }},
            {"velocity", {
                {"x_ms", gps.velocity_x},
                {"y_ms", gps.velocity_y},
                {"z_ms", gps.velocity_z},
                {"x_kmh", gps.velocity_x * 3.6f},
                {"y_kmh", gps.velocity_y * 3.6f},
                {"z_kmh", gps.velocity_z * 3.6f},
                {"ground_speed_ms", std::sqrt(gps.velocity_x * gps.velocity_x + gps.velocity_y * gps.velocity_y)},
                {"ground_speed_kmh", std::sqrt(gps.velocity_x * gps.velocity_x + gps.velocity_y * gps.velocity_y) * 3.6f}
            }},
            {"satellites", {
                {"visible", static_cast<int>(gps.satellites_visible)},
                {"used", static_cast<int>(gps.satellites_used)}
            }},
            {"estimator_type", static_cast<int>(gps.estimator_type)},
            {"timing", {
                {"position_time_usec", gps.position_time_usec},
                {"last_update_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                    gps.last_update.time_since_epoch()).count()}
            }},
            {"metadata", {
                {"has_origin", gps.has_origin},
                {"satellite_count", static_cast<int>(gps.satellites_visible)},
                {"satellites_used_count", static_cast<int>(gps.satellites_used)},
                {"data_available", gps.has_position}
            }}
        };
        
        // Add satellite details if available
        json sat_array = json::array();
        for (int i = 0; i < 20; ++i) {
            if (gps.satellites[i].prn != 0) {
                json sat_json = {
                    {"prn", static_cast<int>(gps.satellites[i].prn)},
                    {"used", static_cast<bool>(gps.satellites[i].used)},
                    {"elevation", static_cast<int>(gps.satellites[i].elevation)},
                    {"azimuth", static_cast<int>(gps.satellites[i].azimuth)},
                    {"snr", static_cast<int>(gps.satellites[i].snr)}
                };
                sat_array.push_back(sat_json);
            }
        }
        gps_json["satellites"]["details"] = sat_array;
        
        // Add origin if available
        if (gps.has_origin) {
            gps_json["origin"] = {
                {"latitude", gps.origin_latitude / 1e7},
                {"longitude", gps.origin_longitude / 1e7},
                {"altitude", gps.origin_altitude / 1000.0f},
                {"time_usec", gps.origin_time_usec}
            };
            gps_json["raw_origin"] = {
                {"latitude_degE7", gps.origin_latitude},
                {"longitude_degE7", gps.origin_longitude},
                {"altitude_mm", gps.origin_altitude},
                {"time_usec", gps.origin_time_usec}
            };
        } else {
            gps_json["origin"] = nullptr;
            gps_json["raw_origin"] = nullptr;
        }
        
        // Add covariance matrix
        json covariance_array = json::array();
        for (int i = 0; i < 36; ++i) {
            covariance_array.push_back(gps.covariance[i]);
        }
        gps_json["covariance_matrix"] = covariance_array;
        
        // Add accuracy information
        gps_json["accuracy"] = {
            {"horizontal_variance", gps.covariance[0]},
            {"vertical_variance", gps.covariance[7]},
            {"latitude_variance", gps.covariance[0]},
            {"longitude_variance", gps.covariance[7]},
            {"altitude_variance", gps.covariance[14]}
        };
        
        std::cout << gps_json.dump() << std::endl;
        
        // Update vehicle GPS data
        vehicle.setGPSData(gps);
        
        if (verbose_mode) {
            std::cout << "\n=== GPS DATA RECEIVED ===" << std::endl;
            std::cout << "GPS State: " << static_cast<int>(gps.state) << std::endl;
            std::cout << "GPS Fix Type: " << static_cast<int>(gps.fix_type) << std::endl;
            std::cout << "Satellites Visible: " << static_cast<int>(gps.satellites_visible) << std::endl;
            std::cout << "Satellites Used: " << static_cast<int>(gps.satellites_used) << std::endl;
            if (gps.has_position) {
                std::cout << "Position: " << (gps.latitude / 1e7) << "°, " << (gps.longitude / 1e7) << "°" << std::endl;
                std::cout << "Altitude: " << (gps.altitude / 1000.0f) << "m" << std::endl;
                std::cout << "Velocity: " << std::sqrt(gps.velocity_x * gps.velocity_x + gps.velocity_y * gps.velocity_y) << " m/s" << std::endl;
            }
            std::cout << "=========================" << std::endl;
        }
    });
    
    // System status callback with comprehensive JSON output
    connection.setSystemStatusCallback([](const MavlinkSystemStatus& status) {
        // Create comprehensive system status JSON
        json system_status_json = {
            {"type", "system_status"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"sensors", {
                {"present_bitmap", status.onboard_control_sensors_present},
                {"enabled_bitmap", status.onboard_control_sensors_enabled},
                {"health_bitmap", status.onboard_control_sensors_health},
                {"present_hex", "0x" + std::to_string(status.onboard_control_sensors_present)},
                {"enabled_hex", "0x" + std::to_string(status.onboard_control_sensors_enabled)},
                {"health_hex", "0x" + std::to_string(status.onboard_control_sensors_health)}
            }},
            {"performance", {
                {"cpu_load_percent", status.load / 10.0f},
                {"cpu_load_raw", status.load}
            }},
            {"battery", {
                {"voltage_mv", status.voltage_battery},
                {"voltage_v", status.voltage_battery / 1000.0f},
                {"current_ca", status.current_battery},
                {"current_a", status.current_battery / 100.0f},
                {"remaining_percent", status.battery_remaining},
                {"voltage_available", status.voltage_battery != UINT16_MAX},
                {"current_available", status.current_battery != -1},
                {"remaining_available", status.battery_remaining != -1}
            }},
            {"communication", {
                {"drop_rate_percent", status.drop_rate_comm / 100.0f},
                {"drop_rate_raw", status.drop_rate_comm},
                {"errors_count", status.errors_comm}
            }},
            {"autopilot_errors", {
                {"errors_count1", status.errors_count1},
                {"errors_count2", status.errors_count2},
                {"errors_count3", status.errors_count3},
                {"errors_count4", status.errors_count4}
            }},
            {"timing", {
                {"last_update_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                    status.last_update.time_since_epoch()).count()}
            }}
        };
        
        // Add sensor status analysis
        json sensor_analysis = json::object();
        
        // Analyze sensor presence
        if (status.onboard_control_sensors_present & (1 << 0)) sensor_analysis["gyroscope"] = "present";
        if (status.onboard_control_sensors_present & (1 << 1)) sensor_analysis["accelerometer"] = "present";
        if (status.onboard_control_sensors_present & (1 << 2)) sensor_analysis["magnetometer"] = "present";
        if (status.onboard_control_sensors_present & (1 << 3)) sensor_analysis["absolute_pressure"] = "present";
        if (status.onboard_control_sensors_present & (1 << 4)) sensor_analysis["differential_pressure"] = "present";
        if (status.onboard_control_sensors_present & (1 << 5)) sensor_analysis["gps"] = "present";
        if (status.onboard_control_sensors_present & (1 << 6)) sensor_analysis["optical_flow"] = "present";
        if (status.onboard_control_sensors_present & (1 << 7)) sensor_analysis["vision_position"] = "present";
        if (status.onboard_control_sensors_present & (1 << 8)) sensor_analysis["laser_position"] = "present";
        if (status.onboard_control_sensors_present & (1 << 9)) sensor_analysis["external_ground_truth"] = "present";
        if (status.onboard_control_sensors_present & (1 << 10)) sensor_analysis["angular_rate_control"] = "present";
        if (status.onboard_control_sensors_present & (1 << 11)) sensor_analysis["attitude_stabilization"] = "present";
        if (status.onboard_control_sensors_present & (1 << 12)) sensor_analysis["yaw_position"] = "present";
        if (status.onboard_control_sensors_present & (1 << 13)) sensor_analysis["z_altitude_control"] = "present";
        if (status.onboard_control_sensors_present & (1 << 14)) sensor_analysis["x_y_position_control"] = "present";
        if (status.onboard_control_sensors_present & (1 << 15)) sensor_analysis["motor_outputs"] = "present";
        if (status.onboard_control_sensors_present & (1 << 16)) sensor_analysis["rc_receiver"] = "present";
        if (status.onboard_control_sensors_present & (1 << 17)) sensor_analysis["gyro2"] = "present";
        if (status.onboard_control_sensors_present & (1 << 18)) sensor_analysis["accel2"] = "present";
        if (status.onboard_control_sensors_present & (1 << 19)) sensor_analysis["magnetometer2"] = "present";
        
        // Analyze sensor health
        json sensor_health = json::object();
        if ((status.onboard_control_sensors_health & (1 << 0)) == 0) sensor_health["gyroscope"] = "error";
        if ((status.onboard_control_sensors_health & (1 << 1)) == 0) sensor_health["accelerometer"] = "error";
        if ((status.onboard_control_sensors_health & (1 << 2)) == 0) sensor_health["magnetometer"] = "error";
        if ((status.onboard_control_sensors_health & (1 << 3)) == 0) sensor_health["absolute_pressure"] = "error";
        if ((status.onboard_control_sensors_health & (1 << 4)) == 0) sensor_health["differential_pressure"] = "error";
        if ((status.onboard_control_sensors_health & (1 << 5)) == 0) sensor_health["gps"] = "error";
        if ((status.onboard_control_sensors_health & (1 << 16)) == 0) sensor_health["rc_receiver"] = "error";
        
        system_status_json["sensor_analysis"] = sensor_analysis;
        system_status_json["sensor_health"] = sensor_health;
        
        std::cout << system_status_json.dump() << std::endl;
        
        // Update vehicle system status
        vehicle.setSystemStatus(status);
        
        if (verbose_mode) {
            std::cout << "\n=== SYSTEM STATUS RECEIVED ===" << std::endl;
            std::cout << "CPU Load: " << (status.load / 10.0f) << "%" << std::endl;
            std::cout << "Battery Voltage: " << (status.voltage_battery / 1000.0f) << "V" << std::endl;
            if (status.current_battery != -1) {
                std::cout << "Battery Current: " << (status.current_battery / 100.0f) << "A" << std::endl;
            }
            if (status.battery_remaining != -1) {
                std::cout << "Battery Remaining: " << static_cast<int>(status.battery_remaining) << "%" << std::endl;
            }
            std::cout << "Communication Drop Rate: " << (status.drop_rate_comm / 100.0f) << "%" << std::endl;
            std::cout << "Communication Errors: " << status.errors_comm << std::endl;
            std::cout << "Sensors Present: 0x" << std::hex << status.onboard_control_sensors_present << std::dec << std::endl;
            std::cout << "Sensors Enabled: 0x" << std::hex << status.onboard_control_sensors_enabled << std::dec << std::endl;
            std::cout << "Sensors Healthy: 0x" << std::hex << status.onboard_control_sensors_health << std::dec << std::endl;
            std::cout << "=============================" << std::endl;
        }
    });
    
    connection.startReceiving();
    
    if (verbose_mode) {
        std::cout << "MAVLink UDP Collector started on " << address << ":" << port << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
    }
    
    while (g_running && !data_collection_complete) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_heartbeat_sent >= heartbeat_interval) {
            connection.sendHeartbeat();
            last_heartbeat_sent = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Data collection complete, start continuous battery, GPS, and system status monitoring
    if (data_collection_complete && verbose_mode) {
        std::cout << "Initial data collection complete. Starting continuous battery, GPS, and system status monitoring..." << std::endl;
    }
    
    // Start continuous battery, GPS, and system status request loop
    auto last_battery_request_sent = std::chrono::steady_clock::now();
    auto last_gps_request_sent = std::chrono::steady_clock::now();
    auto last_system_status_request_sent = std::chrono::steady_clock::now();
    const auto battery_request_interval = std::chrono::seconds(5);  // Request battery data every 5 seconds
    const auto gps_request_interval = std::chrono::seconds(10);     // Request GPS data every 10 seconds
    const auto system_status_request_interval = std::chrono::seconds(2); // Request system status every 2 seconds
    
    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        
        // Send battery requests periodically
        if (now - last_battery_request_sent >= battery_request_interval) {
            if (verbose_mode) {
                std::cout << "Requesting battery information..." << std::endl;
            }
            connection.requestBatteryInfo();
            connection.requestBatteryStatus();
            last_battery_request_sent = now;
        }
        
        // Send GPS requests periodically
        if (now - last_gps_request_sent >= gps_request_interval) {
            if (verbose_mode) {
                std::cout << "Requesting GPS data..." << std::endl;
            }
            connection.requestGPSData(1, 1, 4); // Request GPS data at 4 Hz
            last_gps_request_sent = now;
        }
        
        // Send system status requests periodically
        if (now - last_system_status_request_sent >= system_status_request_interval) {
            if (verbose_mode) {
                std::cout << "Requesting system status..." << std::endl;
            }
            connection.requestSystemStatus(1, 1, 1); // Request system status at 1 Hz
            last_system_status_request_sent = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    connection.stopReceiving();
    connection.disconnect();
    
    if (verbose_mode) {
        std::cout << "MAVLink UDP Collector stopped" << std::endl;
    }
    return 0;
}
