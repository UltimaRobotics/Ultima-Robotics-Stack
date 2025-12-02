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
    std::cout << "Usage: " << program_name << " -a <address> -p <port> [-v]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -a, --address    UDP address to bind to (default: 0.0.0.0)" << std::endl;
    std::cout << "  -p, --port       UDP port to bind to (default: 14550)" << std::endl;
    std::cout << "  -v, --verbose    Enable verbose logging output" << std::endl;
    std::cout << "  -h, --help       Show this help message" << std::endl;
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
    
    static struct option long_options[] = {
        {"address", required_argument, 0, 'a'},
        {"port", required_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "a:p:vh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'a':
                address = std::string(optarg);
                break;
            case 'p':
                port = static_cast<uint16_t>(std::stoi(optarg));
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
    
    MavlinkUdpConnection connection;
    
    if (!connection.connect(address, port)) {
        if (verbose_mode) {
            std::cerr << "Failed to connect to " << address << ":" << port << std::endl;
        }
        return 1;
    }
    
    if (verbose_mode) {
        std::cout << "Connected to " << address << ":" << port << std::endl;
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
    
    // Data collection complete, stop sending heartbeats
    if (data_collection_complete && verbose_mode) {
        std::cout << "Data collection complete. Stopping heartbeat transmission..." << std::endl;
    }
    
    // Wait a bit to ensure final messages are processed
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    connection.stopReceiving();
    connection.disconnect();
    
    if (verbose_mode) {
        std::cout << "MAVLink UDP Collector stopped" << std::endl;
    }
    return 0;
}
