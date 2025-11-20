#include "openvpn_wrapper.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <fstream>
#include <thread>

using namespace openvpn;

std::atomic<bool> g_running(true);
OpenVPNWrapper* g_vpn_wrapper = nullptr;

void signalHandler(int signum) {
    std::cout << json({
        {"type", "signal"},
        {"signal", signum},
        {"message", "Received signal, shutting down..."}
    }).dump() << std::endl;
    
    g_running = false;
    
    if (g_vpn_wrapper) {
        g_vpn_wrapper->disconnect();
    }
}

void printUsage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <config_file_path>" << std::endl;
    std::cerr << "Example: " << program_name << " /path/to/config.ovpn" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check arguments
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string config_file = argv[1];
    
    // Check if config file exists
    std::ifstream file(config_file);
    if (!file.good()) {
        json error = {
            {"type", "error"},
            {"message", "Config file not found"},
            {"file", config_file}
        };
        std::cout << error.dump() << std::endl;
        return 1;
    }
    file.close();
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Initialize OpenVPN wrapper
    OpenVPNWrapper vpn;
    g_vpn_wrapper = &vpn;
    
    // Set event callback to print JSON events
    vpn.setEventCallback([](const VPNEvent& event) {
        json event_json = {
            {"type", event.type},
            {"message", event.message},
            {"state", ""},
            {"timestamp", event.timestamp}
        };
        
        if (!event.data.empty()) {
            event_json["data"] = event.data;
        }
        
        std::cout << event_json.dump() << std::endl;
    });
    
    // Set stats callback to print JSON stats periodically
    vpn.setStatsCallback([](const VPNStats& stats) {
        json stats_json = {
            {"type", "stats"},
            {"bytes_sent", stats.bytes_sent},
            {"bytes_received", stats.bytes_received},
            {"tun_read_bytes", stats.tun_read_bytes},
            {"tun_write_bytes", stats.tun_write_bytes},
            {"ping_ms", stats.ping_ms},
            {"local_ip", stats.local_ip},
            {"remote_ip", stats.remote_ip},
            {"server_ip", stats.server_ip}
        };
        
        if (stats.connected_since > 0) {
            stats_json["uptime_seconds"] = time(nullptr) - stats.connected_since;
        }
        
        std::cout << stats_json.dump() << std::endl;
    });
    
    // Print startup info
    json startup = {
        {"type", "startup"},
        {"message", "OpenVPN wrapper starting"},
        {"config_file", config_file},
        {"pid", getpid()}
    };
    std::cout << startup.dump() << std::endl;
    
    // Initialize with config file
    if (!vpn.initializeFromFile(config_file)) {
        json error = vpn.getLastErrorJson();
        error["type"] = "initialization_error";
        std::cout << error.dump() << std::endl;
        return 1;
    }
    
    // Connect to VPN
    if (!vpn.connect()) {
        json error = {
            {"type", "connection_error"},
            {"message", "Failed to start VPN connection"}
        };
        std::cout << error.dump() << std::endl;
        return 1;
    }
    
    // Main loop - wait for connection and monitor
    while (g_running) {
        // Print periodic status
        if (vpn.isConnected()) {
            json status = vpn.getStatusJson();
            status["type"] = "status";
            std::cout << status.dump() << std::endl;
        }
        
        // Check for errors
        if (vpn.getState() == ConnectionState::ERROR_STATE) {
            json error = vpn.getLastErrorJson();
            error["type"] = "runtime_error";
            std::cout << error.dump() << std::endl;
            
            // Try to reconnect
            json reconnect_msg = {
                {"type", "reconnecting"},
                {"message", "Attempting to reconnect after error"}
            };
            std::cout << reconnect_msg.dump() << std::endl;
            
            vpn.reconnect();
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    // Cleanup
    vpn.disconnect();
    
    json shutdown = {
        {"type", "shutdown"},
        {"message", "OpenVPN wrapper stopped successfully"}
    };
    std::cout << shutdown.dump() << std::endl;
    
    g_vpn_wrapper = nullptr;
    return 0;
}
