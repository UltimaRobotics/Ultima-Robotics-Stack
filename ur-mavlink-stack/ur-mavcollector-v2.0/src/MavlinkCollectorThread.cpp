#include "MavlinkCollectorThread.h"
#include "MavlinkUdpConnection.h"
#include "Vehicle.h"
#include "BoardIdentifier.h"
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include "../thirdparty/nlohmann/json.hpp"
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"

using json = nlohmann::json;

// Global variables
std::atomic<bool> g_running(true);
Vehicle vehicle;
bool verbose_mode = false;

// Helper functions
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
        case MAV_TYPE_FIXED_WING:
            return "Fixed Wing";
        case MAV_TYPE_QUADROTOR:
            return "Quadrotor";
        case MAV_TYPE_COAXIAL:
            return "Coaxial";
        case MAV_TYPE_HELICOPTER:
            return "Helicopter";
        case MAV_TYPE_ANTENNA_TRACKER:
            return "Antenna Tracker";
        case MAV_TYPE_GCS:
            return "Ground Control Station";
        case MAV_TYPE_AIRSHIP:
            return "Airship";
        case MAV_TYPE_FREE_BALLOON:
            return "Free Balloon";
        case MAV_TYPE_ROCKET:
            return "Rocket";
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

// Static variables for the collector
static std::unique_ptr<MavlinkUdpConnection> g_connection = nullptr;
static bool g_collector_initialized = false;

void* mavlinkCollectorThreadFunction(void* arg) {
    if (!arg) {
        std::cerr << "Error: mavlinkCollectorThreadFunction received null argument" << std::endl;
        return nullptr;
    }
    
    MavlinkCollectorArgs* collector_args = static_cast<MavlinkCollectorArgs*>(arg);
    const PackageConfig& config = collector_args->config;
    std::atomic<bool>* running = collector_args->running;
    
    if (verbose_mode) {
        std::cout << "MAVLink collector thread started with config:" << std::endl;
        config.print();
    }
    
    // Initialize signal handlers
    std::signal(SIGINT, [](int signal) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_running = false;
    });
    std::signal(SIGTERM, [](int signal) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_running = false;
    });
    
    // Create and initialize connection
    g_connection = std::make_unique<MavlinkUdpConnection>(config.system_id, config.component_id);
    
    if (!g_connection->connect(config.address, config.port)) {
        if (verbose_mode) {
            std::cerr << "Failed to connect to " << config.address << ":" << config.port << std::endl;
        }
        return nullptr;
    }
    
    if (verbose_mode) {
        std::cout << "Connected to " << config.address << ":" << config.port 
                  << " with system ID " << static_cast<int>(config.system_id) 
                  << " and component ID " << static_cast<int>(config.component_id) << std::endl;
    }
    
    // Set up callbacks (moved from main.cpp)
    g_connection->setHeartbeatCallback([](const MavlinkHeartbeatInfo& info) {
        json heartbeat_json = {
            {"type", "heartbeat"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"system_id", info.system_id},
            {"component_id", info.component_id},
            {"vehicle_type", {
                {"type", info.type},
                {"name", getVehicleTypeName(info.type)}
            }},
            {"autopilot", {
                {"type", info.autopilot},
                {"name", getAutopilotName(info.autopilot)}
            }},
            {"base_mode", info.base_mode},
            {"custom_mode", info.custom_mode},
            {"system_status", info.system_status}
        };
        std::cout << heartbeat_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "\n=== HEARTBEAT RECEIVED ===" << std::endl;
            std::cout << "System ID: " << static_cast<int>(info.system_id) << std::endl;
            std::cout << "Component ID: " << static_cast<int>(info.component_id) << std::endl;
            std::cout << "Vehicle Type: " << getVehicleTypeName(info.type) << " (" << static_cast<int>(info.type) << ")" << std::endl;
            std::cout << "Autopilot: " << getAutopilotName(info.autopilot) << " (" << static_cast<int>(info.autopilot) << ")" << std::endl;
            std::cout << "Base Mode: 0x" << std::hex << static_cast<int>(info.base_mode) << std::dec << std::endl;
            std::cout << "Custom Mode: " << info.custom_mode << std::endl;
            std::cout << "System Status: " << static_cast<int>(info.system_status) << std::endl;
            std::cout << "========================" << std::endl;
        }
    });
    
    g_connection->setAutopilotVersionCallback([](const MavlinkAutopilotVersionInfo& info) {
        json autopilot_json = {
            {"type", "autopilot_version"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"capabilities", info.capabilities},
            {"flight_sw_version", info.flight_sw_version},
            {"middleware_sw_version", info.middleware_sw_version},
            {"os_sw_version", info.os_sw_version},
            {"board_version", info.board_version},
            {"vendor_id", info.vendor_id},
            {"product_id", info.product_id},
            {"uid", info.uid}
        };
        std::cout << autopilot_json.dump() << std::endl;
        
        vehicle.setAutopilotVersionInfo(info);
        
        if (verbose_mode) {
            std::cout << "\n=== AUTOPILOT VERSION RECEIVED ===" << std::endl;
            std::cout << "Capabilities: " << info.capabilities << std::endl;
            std::cout << "Flight SW Version: " << info.flight_sw_version << std::endl;
            std::cout << "Middleware SW Version: " << info.middleware_sw_version << std::endl;
            std::cout << "OS SW Version: " << info.os_sw_version << std::endl;
            std::cout << "Board Version: " << info.board_version << std::endl;
            std::cout << "Vendor ID: 0x" << std::hex << info.vendor_id << std::dec << std::endl;
            std::cout << "Product ID: 0x" << std::hex << info.product_id << std::dec << std::endl;
            std::cout << "UID: " << info.uid << std::endl;
            std::cout << "Board Identification: " << BoardIdentifier::instance().identifyBoard(info.vendor_id, info.product_id) << std::endl;
            std::cout << "===================================" << std::endl;
        }
    });
    
    g_connection->setBatteryInfoCallback([](const MavlinkBatteryInfo& info) {
        json battery_info_json = {
            {"type", "battery_info"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"id", info.id},
            {"battery_function", info.battery_function},
            {"type", info.type},
            {"state_of_health", info.state_of_health},
            {"cells_in_series", info.cells_in_series},
            {"cycle_count", info.cycle_count},
            {"weight", info.weight},
            {"discharge_minimum_voltage", info.discharge_minimum_voltage},
            {"charging_minimum_voltage", info.charging_minimum_voltage},
            {"resting_minimum_voltage", info.resting_minimum_voltage},
            {"charging_maximum_voltage", info.charging_maximum_voltage},
            {"charging_maximum_current", info.charging_maximum_current},
            {"nominal_voltage", info.nominal_voltage},
            {"discharge_maximum_current", info.discharge_maximum_current},
            {"discharge_maximum_burst_current", info.discharge_maximum_burst_current},
            {"design_capacity", info.design_capacity},
            {"full_charge_capacity", info.full_charge_capacity},
            {"manufacture_date", std::string(info.manufacture_date)},
            {"serial_number", std::string(info.serial_number)},
            {"name", std::string(info.name)}
        };
        std::cout << battery_info_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "\n=== BATTERY INFO RECEIVED ===" << std::endl;
            std::cout << "ID: " << static_cast<int>(info.id) << std::endl;
            std::cout << "Function: " << static_cast<int>(info.battery_function) << std::endl;
            std::cout << "Type: " << static_cast<int>(info.type) << std::endl;
            std::cout << "State of Health: " << static_cast<int>(info.state_of_health) << std::endl;
            std::cout << "Cells in Series: " << static_cast<int>(info.cells_in_series) << std::endl;
            std::cout << "Cycle Count: " << info.cycle_count << std::endl;
            std::cout << "Weight: " << info.weight << std::endl;
            std::cout << "Nominal Voltage: " << info.nominal_voltage << "V" << std::endl;
            std::cout << "Design Capacity: " << info.design_capacity << "Ah" << std::endl;
            std::cout << "Full Charge Capacity: " << info.full_charge_capacity << "Ah" << std::endl;
            std::cout << "Serial Number: " << std::string(info.serial_number) << std::endl;
            std::cout << "Name: " << std::string(info.name) << std::endl;
            std::cout << "=============================" << std::endl;
        }
    });
    
    g_connection->setBatteryStatusCallback([](const MavlinkBatteryStatus& status) {
        json battery_status_json = {
            {"type", "battery_status"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"id", status.id},
            {"battery_function", status.battery_function},
            {"type", status.type},
            {"temperature_c", status.temperature},
            {"voltages", status.voltages},
            {"current_battery", status.current_battery},
            {"current_consumed", status.current_consumed},
            {"energy_consumed", status.energy_consumed},
            {"battery_remaining", status.battery_remaining},
            {"time_remaining", status.time_remaining},
            {"charge_state", status.charge_state},
            {"voltages_ext", status.voltages_ext},
            {"mode", status.mode},
            {"fault_bitmask", status.fault_bitmask}
        };
        std::cout << battery_status_json.dump() << std::endl;
        
        if (verbose_mode) {
            std::cout << "\n=== BATTERY STATUS RECEIVED ===" << std::endl;
            std::cout << "ID: " << static_cast<int>(status.id) << std::endl;
            std::cout << "Function: " << static_cast<int>(status.battery_function) << std::endl;
            std::cout << "Type: " << static_cast<int>(status.type) << std::endl;
            std::cout << "Temperature: " << status.temperature << "°C" << std::endl;
            std::cout << "Current Battery: " << status.current_battery << "cA" << std::endl;
            std::cout << "Battery Remaining: " << static_cast<int>(status.battery_remaining) << "%" << std::endl;
            std::cout << "Time Remaining: " << status.time_remaining << " seconds" << std::endl;
            std::cout << "Charge State: " << static_cast<int>(status.charge_state) << std::endl;
            std::cout << "Mode: " << static_cast<int>(status.mode) << std::endl;
            std::cout << "Fault Bitmask: " << status.fault_bitmask << std::endl;
            std::cout << "=============================" << std::endl;
        }
    });
    
    // GPS callback with comprehensive JSON output
    g_connection->setGPSDataCallback([](const MavlinkGPSData& gps) {
        // Create comprehensive GPS JSON
        json gps_json = {
            {"type", "gps_data"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"state", static_cast<int>(gps.state)},
            {"fix_type", static_cast<int>(gps.fix_type)},
            {"satellites_visible", static_cast<int>(gps.satellites_visible)},
            {"satellites_used", static_cast<int>(gps.satellites_used)},
            {"has_origin", gps.has_origin},
            {"has_position", gps.has_position},
            {"position_time_usec", gps.position_time_usec},
            {"position", {
                {"latitude_deg", gps.latitude / 1e7},
                {"longitude_deg", gps.longitude / 1e7},
                {"altitude_m", gps.altitude / 1000.0f},
                {"relative_altitude_m", gps.relative_altitude / 1000.0f},
                {"velocity_x_ms", gps.velocity_x},
                {"velocity_y_ms", gps.velocity_y},
                {"velocity_z_ms", gps.velocity_z},
                {"speed_ms", std::sqrt(gps.velocity_x * gps.velocity_x + gps.velocity_y * gps.velocity_y)},
                {"time_usec", gps.position_time_usec}
            }},
            {"estimator_type", static_cast<int>(gps.estimator_type)}
        };
        
        std::cout << gps_json.dump() << std::endl;
        
        // Update vehicle GPS data
        vehicle.setGPSData(gps);
        
        if (verbose_mode) {
            std::cout << "\n=== GPS DATA RECEIVED ===" << std::endl;
            std::cout << "State: " << static_cast<int>(gps.state) << std::endl;
            std::cout << "Fix Type: " << static_cast<int>(gps.fix_type) << std::endl;
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
    g_connection->setSystemStatusCallback([](const MavlinkSystemStatus& status) {
        // Create comprehensive system status JSON
        json system_status_json = {
            {"type", "system_status"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"sensors", {
                {"present_bitmap", status.onboard_control_sensors_present},
                {"enabled_bitmap", status.onboard_control_sensors_enabled},
                {"health_bitmap", status.onboard_control_sensors_health}
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
                {"remaining_percent", status.battery_remaining}
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
            }}
        };
        
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
    
    g_connection->startReceiving();
    
    if (verbose_mode) {
        std::cout << "MAVLink UDP Collector started on " << config.address << ":" << config.port << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
    }
    
    // Main loop
    auto last_heartbeat_sent = std::chrono::steady_clock::now();
    const auto heartbeat_interval = std::chrono::seconds(1);
    bool received_first_heartbeat = false;
    bool data_collection_complete = false;
    
    while (*running && !data_collection_complete) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_heartbeat_sent >= heartbeat_interval) {
            g_connection->sendHeartbeat();
            last_heartbeat_sent = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Continuous monitoring loop
    if (data_collection_complete && verbose_mode) {
        std::cout << "Initial data collection complete. Starting continuous monitoring..." << std::endl;
    }
    
    auto last_battery_request_sent = std::chrono::steady_clock::now();
    auto last_gps_request_sent = std::chrono::steady_clock::now();
    auto last_system_status_request_sent = std::chrono::steady_clock::now();
    const auto battery_request_interval = std::chrono::seconds(5);
    const auto gps_request_interval = std::chrono::seconds(10);
    const auto system_status_request_interval = std::chrono::seconds(2);
    
    while (*running) {
        auto now = std::chrono::steady_clock::now();
        
        // Send heartbeat periodically
        if (now - last_heartbeat_sent >= heartbeat_interval) {
            g_connection->sendHeartbeat();
            last_heartbeat_sent = now;
        }
        
        // Send battery requests periodically
        if (now - last_battery_request_sent >= battery_request_interval) {
            if (verbose_mode) {
                std::cout << "Requesting battery information..." << std::endl;
            }
            g_connection->requestBatteryInfo();
            g_connection->requestBatteryStatus();
            last_battery_request_sent = now;
        }
        
        // Send GPS requests periodically
        if (now - last_gps_request_sent >= gps_request_interval) {
            if (verbose_mode) {
                std::cout << "Requesting GPS data..." << std::endl;
            }
            g_connection->requestGPSData(1, 1, 4);
            last_gps_request_sent = now;
        }
        
        // Send system status requests periodically
        if (now - last_system_status_request_sent >= system_status_request_interval) {
            if (verbose_mode) {
                std::cout << "Requesting system status..." << std::endl;
            }
            g_connection->requestSystemStatus(1, 1, 1);
            last_system_status_request_sent = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup
    if (verbose_mode) {
        std::cout << "Shutting down MAVLink collector..." << std::endl;
    }
    
    g_connection->stopReceiving();
    g_connection->disconnect();
    g_connection.reset();
    
    if (verbose_mode) {
        std::cout << "MAVLink collector stopped" << std::endl;
    }
    
    return nullptr;
}

bool initializeMavlinkCollector(const PackageConfig& config) {
    // This function can be used for any additional initialization
    // that needs to happen before starting the thread
    g_collector_initialized = true;
    return true;
}

void cleanupMavlinkCollector() {
    if (g_connection) {
        g_connection->stopReceiving();
        g_connection->disconnect();
        g_connection.reset();
    }
    g_collector_initialized = false;
}

bool isMavlinkCollectorInitialized() {
    return g_collector_initialized;
}

// Global thread manager for MAVLink collector
static std::unique_ptr<ThreadMgr::ThreadManager> g_collectorThreadManager;
static unsigned int g_collectorThreadId = 0;

unsigned int startMavlinkCollector(const PackageConfig& config, std::atomic<bool>* running) {
    if (g_collectorThreadId > 0) {
        std::cout << "MAVLink collector is already running" << std::endl;
        return g_collectorThreadId;
    }
    
    try {
        // Initialize thread manager if not already done
        if (!g_collectorThreadManager) {
            g_collectorThreadManager = std::make_unique<ThreadMgr::ThreadManager>(10);
        }
        
        // Initialize MAVLink collector
        if (!initializeMavlinkCollector(config)) {
            std::cerr << "Failed to initialize MAVLink collector" << std::endl;
            return 0;
        }
        
        // Create thread arguments
        auto collectorArgs = std::make_unique<MavlinkCollectorArgs>(config, running);
        
        // Create the MAVLink collector thread using ThreadManager
        g_collectorThreadId = g_collectorThreadManager->createThread([args = collectorArgs.get()]() {
            mavlinkCollectorThreadFunction(const_cast<void*>(static_cast<const void*>(args)));
        });
        
        if (verbose_mode) {
            std::cout << "MAVLink collector thread started with ID: " << g_collectorThreadId << std::endl;
        }
        
        return g_collectorThreadId;
        
    } catch (const ThreadMgr::ThreadManagerException& e) {
        std::cerr << "ThreadManager error: " << e.what() << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error starting MAVLink collector: " << e.what() << std::endl;
        return 0;
    }
}

void stopMavlinkCollector(unsigned int threadId) {
    if (g_collectorThreadId == 0 || threadId != g_collectorThreadId) {
        std::cout << "MAVLink collector is not running or thread ID mismatch" << std::endl;
        return;
    }
    
    try {
        if (verbose_mode) {
            std::cout << "Stopping MAVLink collector thread..." << std::endl;
        }
        
        // Stop the thread using ThreadManager
        if (g_collectorThreadManager && g_collectorThreadId > 0) {
            g_collectorThreadManager->stopThread(g_collectorThreadId);
            
            // Wait for thread to finish (with timeout)
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::seconds(5);
            
            while (g_collectorThreadManager->getThreadState(g_collectorThreadId) != ThreadMgr::ThreadState::Stopped) {
                if (std::chrono::steady_clock::now() - startTime > timeout) {
                    std::cerr << "Warning: MAVLink collector thread did not stop within timeout, forcing cleanup" << std::endl;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        // Cleanup
        cleanupMavlinkCollector();
        
        g_collectorThreadId = 0;
        
        if (verbose_mode) {
            std::cout << "MAVLink collector stopped successfully" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error stopping MAVLink collector: " << e.what() << std::endl;
    }
}
