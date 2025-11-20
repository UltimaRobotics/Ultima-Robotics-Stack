#include "FlightCollector.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <ThreadManager.hpp>
#include <functional>

// Define refresh rate as constant
#define DATA_REFRESH_INTERVAL_MS 1000

std::atomic<bool> g_running{true};

// Forward declaration
void printPrettyOutput(const FlightCollector::FlightDataCollection& data);

// Structure to pass data to the main loop thread
struct MainLoopData {
    FlightCollector::FlightCollector* collector;
    bool json_output;
    bool verbose;
    ThreadMgr::ThreadManager* thread_manager;
    unsigned int thread_id;
};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
    exit(0);
}

// Main loop thread function managed by ThreadManager
void mainLoopThread(MainLoopData* data) {
    auto last_output = std::chrono::steady_clock::now();
    
    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_output);
        
        if (elapsed.count() >= DATA_REFRESH_INTERVAL_MS) {
            try {
                if (data->json_output) {
                    // Clear screen and move cursor to top for in-place JSON updates
                    std::cout << "\033[2J\033[H";
                    std::cout << data->collector->getJsonOutput() << std::endl;
                } else {
                    printPrettyOutput(data->collector->getFlightData());
                }
                last_output = now;
            } catch (const std::exception& e) {
                std::cerr << "Exception in main loop thread: " << e.what() << std::endl;
                break;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --config <file>    Configuration JSON file (required)" << std::endl;
    std::cout << "  -h, --help             Show this help message" << std::endl;
    std::cout << "  -v, --verbose          Enable verbose output" << std::endl;
    std::cout << "  -j, --json             Output in JSON format (default: pretty format)" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: Refresh rate is fixed at " << DATA_REFRESH_INTERVAL_MS << "ms" << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " -c config.json" << std::endl;
}

void printPrettyOutput(const FlightCollector::FlightDataCollection& data) {
    // Clear screen and move cursor to top for in-place updates
    std::cout << "\033[2J\033[H";
    
    std::cout << "=== Flight Data Collection ===" << std::endl;
    std::cout << "Timestamp: " << FlightCollector::JsonFormatter::formatTimestamp(data.last_update) << std::endl;
    
    std::cout << "\n--- Vehicle Data ---" << std::endl;
    std::cout << "Model: " << data.vehicle.model << std::endl;
    std::cout << "System ID: " << static_cast<int>(data.vehicle.system_id) << std::endl;
    std::cout << "Component ID: " << static_cast<int>(data.vehicle.component_id) << std::endl;
    std::cout << "Flight Mode: " << data.vehicle.flight_mode << std::endl;
    std::cout << "Armed: " << (data.vehicle.armed ? "Yes" : "No") << std::endl;
    std::cout << "Battery Voltage: " << std::fixed << std::setprecision(2) << data.vehicle.battery_voltage << " V" << std::endl;
    std::cout << "Firmware: " << data.vehicle.firmware << std::endl;
    std::cout << "Messages Received: " << data.vehicle.messages_received << std::endl;
    
    std::cout << "\n--- Diagnostic Data ---" << std::endl;
    std::cout << "Vehicle Type: " << data.diagnostics.vehicle << std::endl;
    std::cout << "Firmware Version: " << data.diagnostics.firmware_version << std::endl;
    
    std::cout << "\n--- Sensor Status ---" << std::endl;
    std::cout << "Gyro: " << data.diagnostics.sensors.gyro << std::endl;
    std::cout << "Accelerometer: " << data.diagnostics.sensors.accelerometer << std::endl;
    std::cout << "Compass 0: " << data.diagnostics.sensors.compass_0 << std::endl;
    std::cout << "Compass 1: " << data.diagnostics.sensors.compass_1 << std::endl;
    
    std::cout << "\n--- Power Status ---" << std::endl;
    std::cout << "Vcc: " << data.diagnostics.power_status.Vcc << " mV" << std::endl;
    std::cout << "Vservo: " << data.diagnostics.power_status.Vservo << " mV" << std::endl;
    
    if (!data.diagnostics.battery_status_map.empty()) {
        std::cout << "\n--- Battery Status ---" << std::endl;
        for (const auto& pair : data.diagnostics.battery_status_map) {
            const auto& status = pair.second;
            std::cout << "Battery " << static_cast<int>(status.id) << ":" << std::endl;
            std::cout << "  Remaining: " << static_cast<int>(status.battery_remaining) << "%" << std::endl;
            std::cout << "  Temperature: " << (status.temperature / 100.0f) << "°C" << std::endl;
            std::cout << "  Current: " << (status.current_battery / 100.0f) << " A" << std::endl;
        }
    }
    
    if (!data.parameters.empty()) {
        std::cout << "\n--- Key Parameters ---" << std::endl;
        int count = 0;
        for (const auto& pair : data.parameters) {
            if (count >= 10) break; // Limit output
            std::cout << pair.first << ": " << pair.second.value << std::endl;
            count++;
        }
        if (data.parameters.size() > 10) {
            std::cout << "... and " << (data.parameters.size() - 10) << " more parameters" << std::endl;
        }
    }
    
    if (!data.message_rates.empty()) {
        std::cout << "\n--- Message Rates ---" << std::endl;
        for (const auto& pair : data.message_rates) {
            std::cout << "MSG " << pair.first << ": " << std::fixed << std::setprecision(1) 
                      << pair.second.current_rate_hz << " Hz" << std::endl;
        }
    }
    
    std::cout << "\n" << std::string(50, '=') << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file;
    bool verbose = false;
    bool json_output = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: --config requires a filename" << std::endl;
                return 1;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-j" || arg == "--json") {
            json_output = true;
        } else if (arg == "-i" || arg == "--interval") {
            std::cout << "Note: Interval parameter is deprecated. Using fixed refresh rate of " 
                      << DATA_REFRESH_INTERVAL_MS << "ms" << std::endl;
            if (i + 1 < argc) {
                ++i; // Skip the value but don't use it
            }
        } else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (config_file.empty()) {
        std::cerr << "Error: Configuration file is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Parse configuration
        if (verbose) {
            std::cout << "Loading configuration from: " << config_file << std::endl;
        }
        
        FlightCollector::ConnectionConfig config = FlightCollector::ConfigParser::parseConfig(config_file);
        
        if (verbose) {
            std::cout << "Configuration loaded successfully:" << std::endl;
            std::cout << "  Type: " << config.type << std::endl;
            std::cout << "  Address: " << config.address << std::endl;
            if (config.type == "udp" || config.type == "tcp") {
                std::cout << "  Port: " << config.port << std::endl;
            } else {
                std::cout << "  Baudrate: " << config.baudrate << std::endl;
            }
            std::cout << "  System ID: " << config.system_id << std::endl;
            std::cout << "  Component ID: " << config.component_id << std::endl;
            std::cout << "  Timeout: " << config.timeout_s << "s" << std::endl;
        }
        
        // Create and initialize collector
        FlightCollector::FlightCollector collector;
        
        // Set verbose mode for JSON output
        collector.setVerbose(verbose);
        
        if (!collector.initialize(config)) {
            std::cerr << "Failed to initialize collector" << std::endl;
            return 1;
        }
        
        // Setup connection callback
        collector.setConnectionCallback([verbose](bool connected) {
            if (verbose || !connected) {
                std::cout << "Connection status: " << (connected ? "Connected" : "Disconnected") << std::endl;
            }
        });
        
        // Connect to flight controller
        if (!collector.connect()) {
            std::cerr << "Failed to connect to flight controller" << std::endl;
            return 1;
        }
        
        // Start data collection
        if (!collector.startCollection()) {
            std::cerr << "Failed to start data collection" << std::endl;
            collector.disconnect();
            return 1;
        }
        
        std::cout << "Flight Collector started successfully!" << std::endl;
        std::cout << "Output interval: " << DATA_REFRESH_INTERVAL_MS << "ms (fixed)" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        
        // Create ThreadManager and launch main loop as thread
        ThreadMgr::ThreadManager thread_manager(5);
        
        // Prepare data for the thread
        MainLoopData thread_data;
        thread_data.collector = &collector;
        thread_data.json_output = json_output;
        thread_data.verbose = verbose;
        thread_data.thread_manager = &thread_manager;
        
        // Create and start the main loop thread
        try {
            auto main_loop_thread_id = thread_manager.createThread(mainLoopThread, &thread_data);
            thread_data.thread_id = main_loop_thread_id;
            
            std::cout << "Main loop started as thread ID: " << main_loop_thread_id << std::endl;
            
            // Wait for thread to complete (will run until g_running is set to false)
            while (thread_manager.isThreadAlive(main_loop_thread_id) && g_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Stop the thread if still running
            if (thread_manager.isThreadAlive(main_loop_thread_id)) {
                std::cout << "Stopping main loop thread..." << std::endl;
                thread_manager.stopThread(main_loop_thread_id);
                thread_manager.joinThread(main_loop_thread_id, std::chrono::seconds(5));
            }
            
        } catch (const ThreadMgr::ThreadManagerException& e) {
            std::cerr << "ThreadManager error: " << e.what() << std::endl;
        }
        
        // Cleanup
        std::cout << "Stopping collection..." << std::endl;
        collector.stopCollection();
        collector.disconnect();
        
        std::cout << "Flight Collector stopped." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
