
#include "PingAPI.hpp"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

PingConfig parseConfig(const json& j) {
    PingConfig config;
    
    if (j.contains("destination")) {
        config.destination = j["destination"].get<std::string>();
    }
    
    if (j.contains("count")) {
        config.count = j["count"].get<int>();
    }
    
    if (j.contains("timeout_ms")) {
        config.timeout_ms = j["timeout_ms"].get<int>();
    }
    
    if (j.contains("interval_ms")) {
        config.interval_ms = j["interval_ms"].get<int>();
    }
    
    if (j.contains("packet_size")) {
        config.packet_size = j["packet_size"].get<int>();
    }
    
    if (j.contains("ttl")) {
        config.ttl = j["ttl"].get<int>();
    }
    
    if (j.contains("resolve_hostname")) {
        config.resolve_hostname = j["resolve_hostname"].get<bool>();
    }
    
    if (j.contains("export_file_path")) {
        config.export_file_path = j["export_file_path"].get<std::string>();
    }
    
    return config;
}

json resultToJson(const PingResult& result) {
    json j;
    
    j["destination"] = result.destination;
    j["ip_address"] = result.ip_address;
    j["packets_sent"] = result.packets_sent;
    j["packets_received"] = result.packets_received;
    j["packets_lost"] = result.packets_lost;
    j["loss_percentage"] = result.loss_percentage;
    j["success"] = result.success;
    
    if (!result.error_message.empty()) {
        j["error_message"] = result.error_message;
    }
    
    if (result.success) {
        j["rtt_min_ms"] = result.min_rtt_ms;
        j["rtt_max_ms"] = result.max_rtt_ms;
        j["rtt_avg_ms"] = result.avg_rtt_ms;
        j["rtt_stddev_ms"] = result.stddev_rtt_ms;
        
        json rtt_array = json::array();
        for (size_t i = 0; i < result.rtt_times.size(); i++) {
            json ping_item;
            ping_item["sequence"] = result.sequence_numbers[i];
            ping_item["rtt_ms"] = result.rtt_times[i];
            ping_item["ttl"] = result.ttl_values[i];
            rtt_array.push_back(ping_item);
        }
        j["ping_results"] = rtt_array;
    }
    
    return j;
}

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -c, --config <file>     Load configuration from JSON file\n"
              << "  -j, --json <string>     Load configuration from JSON string\n"
              << "  -h, --help              Show this help message\n"
              << "  -e, --example           Show example JSON configuration\n\n"
              << "Examples:\n"
              << "  " << program << " --config ping_config.json\n"
              << "  " << program << " --json '{\"destination\":\"8.8.8.8\",\"count\":5}'\n\n"
              << "Note: This program requires root/CAP_NET_RAW privileges\n";
}

void printExample() {
    std::cout << "\n=== Example JSON Configuration ===\n\n"
              << "{\n"
              << "  \"destination\": \"google.com\",\n"
              << "  \"count\": 4,\n"
              << "  \"timeout_ms\": 1000,\n"
              << "  \"interval_ms\": 1000,\n"
              << "  \"packet_size\": 56,\n"
              << "  \"ttl\": 64,\n"
              << "  \"resolve_hostname\": true,\n"
              << "  \"export_file_path\": \"ping_results.json\"\n"
              << "}\n\n"
              << "=== Field Descriptions ===\n\n"
              << "REQUIRED FIELDS:\n"
              << "  destination       : Target hostname or IP address\n\n"
              << "OPTIONAL FIELDS:\n"
              << "  count             : Number of ping packets to send (default: 4)\n"
              << "  timeout_ms        : Timeout in milliseconds (default: 1000)\n"
              << "  interval_ms       : Interval between pings in ms (default: 1000)\n"
              << "  packet_size       : Packet data size in bytes (default: 56)\n"
              << "  ttl               : Time To Live value (default: 64)\n"
              << "  resolve_hostname  : Resolve hostname to IP (default: true)\n"
              << "  export_file_path  : Path to export real-time results (default: none)\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string configFile;
    std::string jsonString;
    bool useFile = false;
    bool useString = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-e" || arg == "--example") {
            printExample();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
                useFile = true;
            } else {
                std::cerr << "Error: --config requires a file path\n";
                return 1;
            }
        } else if (arg == "-j" || arg == "--json") {
            if (i + 1 < argc) {
                jsonString = argv[++i];
                useString = true;
            } else {
                std::cerr << "Error: --json requires a JSON string\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (!useFile && !useString) {
        std::cerr << "Error: Either --config or --json must be specified\n";
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        json config_json;
        
        if (useFile) {
            std::ifstream file(configFile);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open config file: " << configFile << "\n";
                return 1;
            }
            file >> config_json;
        } else {
            config_json = json::parse(jsonString);
        }
        
        if (!config_json.contains("destination")) {
            std::cerr << "Error: Configuration must contain 'destination' field\n";
            return 1;
        }
        
        PingConfig config = parseConfig(config_json);
        
        PingAPI ping;
        ping.setConfig(config);
        
        // Set up real-time callback to display results as they arrive
        ping.setRealtimeCallback([&config](const PingRealtimeResult& rt_result) {
            json rt_json;
            rt_json["sequence"] = rt_result.sequence;
            rt_json["success"] = rt_result.success;
            
            if (rt_result.success) {
                rt_json["rtt_ms"] = rt_result.rtt_ms;
                rt_json["ttl"] = rt_result.ttl;
                std::cout << "PING_RESULT: " << rt_json.dump() << std::endl;
            } else {
                rt_json["error"] = rt_result.error_message;
                std::cout << "PING_RESULT: " << rt_json.dump() << std::endl;
            }
            std::cout.flush();
        });
        
        PingResult result = ping.execute();
        
        // Output final summary
        json output = resultToJson(result);
        std::cout << "\nFINAL_SUMMARY: " << output.dump(2) << std::endl;
        
        return result.success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
