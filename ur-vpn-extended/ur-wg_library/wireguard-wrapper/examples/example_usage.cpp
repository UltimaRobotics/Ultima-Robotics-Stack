
#include <iostream>
#include <unistd.h>
#include "wireguard_wrapper.hpp"

int main() {
    wireguard::WireGuardWrapper wg;
    
    // Set event callback
    wg.setEventCallback([](const wireguard::VPNEvent& event) {
        json j = {
            {"type", event.type},
            {"message", event.message},
            {"state", "connected"},
            {"timestamp", event.timestamp}
        };
        std::cout << j.dump() << std::endl;
    });
    
    // Initialize from file
    if (!wg.initializeFromFile("/etc/wireguard/wg0.conf")) {
        std::cerr << "Failed to initialize" << std::endl;
        return 1;
    }
    
    // Connect
    if (!wg.connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    // Monitor connection
    while (wg.isConnected()) {
        auto stats = wg.getStatsJson();
        std::cout << stats.dump() << std::endl;
        sleep(5);
    }
    
    return 0;
}
