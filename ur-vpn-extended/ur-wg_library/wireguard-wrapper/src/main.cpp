
#include <iostream>
#include <csignal>
#include <atomic>
#include <unistd.h>
#include "wireguard_wrapper.hpp"

static std::atomic<bool> g_running(true);

void signal_handler(int signum) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config-file>" << std::endl;
        return 1;
    }
    
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    wireguard::WireGuardWrapper wg;
    
    // Set event callback to output JSON events
    wg.setEventCallback([](const wireguard::VPNEvent& event) {
        json j;
        j["type"] = event.type;
        j["message"] = event.message;
        j["state"] = static_cast<int>(event.state);
        j["timestamp"] = event.timestamp;
        if (!event.data.empty()) {
            j["data"] = event.data;
        }
        std::cout << j.dump() << std::endl;
    });
    
    // Set stats callback
    wg.setStatsCallback([](const wireguard::VPNStats& stats) {
        // Stats are already emitted via events
    });
    
    // Initialize from config file
    if (!wg.initializeFromFile(argv[1])) {
        std::cerr << wg.getLastErrorJson().dump() << std::endl;
        return 1;
    }
    
    // Connect
    if (!wg.connect()) {
        std::cerr << wg.getLastErrorJson().dump() << std::endl;
        return 1;
    }
    
    // Main loop
    while (g_running && wg.isConnected()) {
        sleep(1);
        
        // Periodic status output
        static int counter = 0;
        if (++counter % 10 == 0) {
            auto status = wg.getStatusJson();
            status["type"] = "status";
            std::cout << status.dump() << std::endl;
        }
    }
    
    // Disconnect
    wg.disconnect();
    
    return 0;
}
