
#include "traceroute.hpp"
#include <iostream>
#include <fstream>

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <config.json>" << std::endl;
    std::cerr << "Example config:" << std::endl;
    std::cerr << R"({
  "target": "google.com",
  "max_hops": 30,
  "timeout_ms": 5000,
  "packet_size": 60,
  "num_queries": 3,
  "export_file_path": "traceroute_export.json"
})" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string config_file = argv[1];
    
    // Read config file
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << config_file << std::endl;
        return 1;
    }
    
    nlohmann::json config_json;
    try {
        file >> config_json;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON config: " << e.what() << std::endl;
        return 1;
    }
    file.close();
    
    // Parse configuration
    traceroute::TracerouteConfig config;
    try {
        config = traceroute::TracerouteConfig::from_json(config_json);
    } catch (const std::exception& e) {
        std::cerr << "Error in config: " << e.what() << std::endl;
        return 1;
    }
    
    if (config.target.empty()) {
        std::cerr << "Error: 'target' must be specified in config" << std::endl;
        return 1;
    }
    
    // Execute traceroute with real-time hop display
    std::cout << "Tracing route to " << config.target << "..." << std::endl;
    std::cout << std::endl;
    
    traceroute::Traceroute tracer;
    
    // Callback to display hops in real-time
    auto hop_callback = [](const traceroute::HopInfo& hop) {
        std::cout << "Hop " << hop.hop_number << ": ";
        if (hop.timeout) {
            std::cout << "* * * (timeout)" << std::endl;
        } else {
            std::cout << hop.ip_address;
            if (hop.hostname != hop.ip_address) {
                std::cout << " (" << hop.hostname << ")";
            }
            std::cout << " - " << hop.rtt_ms << " ms" << std::endl;
        }
        std::cout.flush();
    };
    
    traceroute::TracerouteResult result = tracer.execute(config, hop_callback);
    
    // Output final result as JSON
    std::cout << std::endl;
    std::cout << "=== Final Result ===" << std::endl;
    nlohmann::json result_json = result.to_json();
    std::cout << result_json.dump(2) << std::endl;
    
    return result.success ? 0 : 1;
}

