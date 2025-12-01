
#include "connection_manager.h"
#include "command_logger.h"
#include "timeout_config.h"
#include "smart_routing.h"
#include "connection_registry.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>  // for exit() and EXIT_FAILURE
#include "ThreadManager.hpp"

static std::atomic<bool> g_running(true);
static ThreadMgr::ThreadManager* g_threadManager = nullptr;
static unsigned int g_mainloopThreadId = 0;

struct PackageConfig {
    std::string device_json;
    std::string cellular_mode;
    std::string timeouts;
    std::string network;
    std::string ip_monitor;
    std::string routing;
    std::string log_file;
    
    bool verbose = false;
    bool enable_monitoring = true;
    bool enable_auto_recovery = true;
    bool verbose_cmd = false;
    bool disable_auto_routing = false;
};

// Forward declaration
std::string readFile(const std::string& filename);

void signalHandler(int signal) {
    std::cout << "\n=== MAIN SIGNAL HANDLER ACTIVATED ===" << std::endl;
    std::cout << "Received signal " << signal;
    
    switch(signal) {
        case SIGINT:
            std::cout << " (SIGINT - Ctrl+C)";
            break;
        case SIGTERM:
            std::cout << " (SIGTERM - Termination request)";
            break;
        default:
            std::cout << " (Unknown signal)";
            break;
    }
    std::cout << std::endl;
    
    std::cout << "Initiating coordinated shutdown sequence..." << std::endl;
    g_running = false;
    
    // Stop the mainloop thread first
    if (g_threadManager && g_mainloopThreadId != 0) {
        std::cout << "Step 0: Stopping mainloop thread..." << std::endl;
        try {
            g_threadManager->stopThread(g_mainloopThreadId);
            std::cout << "Mainloop thread stopped successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Warning: Failed to stop mainloop thread: " << e.what() << std::endl;
        }
    }
    
    // First, let connection registry handle its own cleanup
    std::cout << "Step 1: Connection registry cleanup..." << std::endl;
    ConnectionRegistry::handleGlobalTermination();
    
    // Then perform connection manager emergency cleanup
    ConnectionManager* active_manager = ConnectionManager::getActiveInstance();
    if (active_manager) {
        std::cout << "Step 2: Connection manager emergency cleanup..." << std::endl;
        active_manager->performEmergencyCleanup();
        std::cout << "Connection manager emergency cleanup completed" << std::endl;
    } else {
        std::cout << "Step 2: No active connection manager, performing basic cleanup..." << std::endl;
        
        // Perform basic cleanup if no connection manager is available
        std::cout << "Attempting basic WWAN interface cleanup..." << std::endl;
        system("pkill -f dhclient 2>/dev/null || true");
        system("ip route flush table main | grep wwan 2>/dev/null || true");
        
        // Try to bring down any wwan interfaces
        system("for iface in $(ls /sys/class/net/ | grep wwan 2>/dev/null); do ip link set dev $iface down 2>/dev/null || true; done");
        
        std::cout << "Basic cleanup completed" << std::endl;
    }
    
    // Clean up global resources
    std::cout << "Step 3: Global resource cleanup..." << std::endl;
    ConnectionRegistry::cleanup();
    
    // Clean up thread manager
    if (g_threadManager) {
        std::cout << "Step 4: Thread manager cleanup..." << std::endl;
        delete g_threadManager;
        g_threadManager = nullptr;
    }
    
    // Give a moment for cleanup to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    std::cout << "=== COORDINATED SHUTDOWN COMPLETED ===" << std::endl;
    std::cout << "Exiting application..." << std::endl;
    exit(EXIT_SUCCESS);
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "Options:\n"
              << "  -pkg_config FILE    Package configuration file containing all settings\n"
              << "  -h, --help          Show this help message\n"
              << "\n"
              << "Example:\n"
              << "  " << program_name << " -pkg_config config/package_config.json\n"
              << std::endl;
}

bool loadPackageConfig(const std::string& config_file, PackageConfig& config) {
    std::string content = readFile(config_file);
    if (content.empty()) {
        std::cerr << "Error: Cannot read package config file: " << config_file << std::endl;
        return false;
    }
    
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(content, root)) {
        std::cerr << "Error: Failed to parse package config JSON: " << reader.getFormattedErrorMessages() << std::endl;
        return false;
    }
    
    // Load config file paths
    if (root.isMember("config_files")) {
        const Json::Value& config_files = root["config_files"];
        config.device_json = config_files.get("device_json", "").asString();
        config.cellular_mode = config_files.get("cellular_mode", "").asString();
        config.timeouts = config_files.get("timeouts", "").asString();
        config.network = config_files.get("network", "").asString();
        config.ip_monitor = config_files.get("ip_monitor", "").asString();
        config.routing = config_files.get("routing", "").asString();
        config.log_file = config_files.get("log_file", "").asString();
    }
    
    // Load flags
    if (root.isMember("flags")) {
        const Json::Value& flags = root["flags"];
        config.verbose = flags.get("verbose", false).asBool();
        config.enable_monitoring = flags.get("enable_monitoring", true).asBool();
        config.enable_auto_recovery = flags.get("enable_auto_recovery", true).asBool();
        config.verbose_cmd = flags.get("verbose_cmd", false).asBool();
        config.disable_auto_routing = flags.get("disable_auto_routing", false).asBool();
    }
    
    std::cout << "Package configuration loaded from: " << config_file << std::endl;
    return true;
}

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}

void mainloopThread(ConnectionManager* manager, bool verbose) {
    std::cout << "Mainloop thread started, managing connection..." << std::endl;
    
    // Main loop - now running in a separate thread
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (verbose && manager->isConnected()) {
            ConnectionMetrics metrics = manager->getCurrentMetrics();
            std::cout << "Status: Connected, Signal: " << metrics.signal_strength 
                      << " dBm, IP: " << metrics.ip_address << std::endl;
        }
    }
    
    std::cout << "Mainloop thread received shutdown signal" << std::endl;
}

int main(int argc, char* argv[]) {
    // Set up global signal handlers with high priority
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Main application signal handlers installed" << std::endl;
    
    // Parse command line arguments
    std::string package_config_file;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-pkg_config") {
            if (i + 1 < argc) {
                package_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing package config file argument" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (package_config_file.empty()) {
        std::cerr << "Error: Package config file is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Load package configuration
    PackageConfig config;
    if (!loadPackageConfig(package_config_file, config)) {
        return 1;
    }
    
    // Initialize connection registry
    ConnectionRegistry::initialize();
    
    // Load timeout configuration if specified
    if (!config.timeouts.empty()) {
        std::cout << "Loading timeout configuration from: " << config.timeouts << std::endl;
        if (!g_timeout_config.loadFromFile(config.timeouts)) {
            std::cerr << "Warning: Failed to load timeout configuration, using defaults" << std::endl;
        }
    }
    
    // Load smart routing configuration
    SmartRoutingConfig routing_config;
    if (!config.routing.empty()) {
        std::cout << "Loading smart routing configuration from: " << config.routing << std::endl;
        if (!routing_config.loadFromFile(config.routing)) {
            std::cerr << "Warning: Failed to load routing configuration, using defaults" << std::endl;
        }
    }
    
    // Disable auto routing if requested
    if (config.disable_auto_routing) {
        routing_config.auto_routing_enabled = false;
        std::cout << "Automatic routing disabled" << std::endl;
    }
    
    // Initialize smart routing manager
    if (!g_smart_routing.initialize(routing_config)) {
        std::cerr << "Error: Failed to initialize smart routing manager" << std::endl;
        return 1;
    }
    
    // Set up routing change callback
    g_smart_routing.setRoutingChangeCallback([config](RoutingOperation operation, const RoutingRule& rule, bool success, const std::string& error) {
        if (config.verbose) {
            std::cout << "Routing change - Operation: " << static_cast<int>(operation)
                      << ", Rule: " << rule.destination << " via " << rule.gateway 
                      << " dev " << rule.interface << ", Success: " << (success ? "Yes" : "No");
            if (!error.empty()) {
                std::cout << ", Error: " << error;
            }
            std::cout << std::endl;
        }
    });
    
    // Enable verbose command logging if requested
    if (config.verbose_cmd) {
        CommandLogger::setVerboseEnabled(true);
        std::cout << "Verbose command logging enabled" << std::endl;
    }
    
    // Read device JSON
    std::string device_json = readFile(config.device_json);
    if (device_json.empty()) {
        return 1;
    }
    
    // Create connection manager
    ConnectionManager manager;
    
    // Initialize connection lifecycle manager for registry tracking
    std::unique_ptr<ConnectionLifecycleManager> lifecycle_manager;
    
    // Set up callbacks
    manager.setStateChangeCallback([config](ConnectionState state, const std::string& reason) {
        std::cout << "State changed to: " << static_cast<int>(state) 
                  << " (" << reason << ")" << std::endl;
        if (config.verbose) {
            std::cout << "Reason: " << reason << std::endl;
        }
    });
    
    manager.setMetricsCallback([config](const ConnectionMetrics& metrics) {
        if (config.verbose) {
            std::cout << "Metrics - Signal: " << metrics.signal_strength 
                      << ", IP: " << metrics.ip_address 
                      << ", Connected: " << (metrics.is_connected ? "Yes" : "No") 
                      << std::endl;
        }
    });
    
    // Initialize
    if (!manager.initialize(device_json)) {
        std::cerr << "Error: Failed to initialize connection manager" << std::endl;
        return 1;
    }
    
    std::cout << "Connection manager initialized successfully" << std::endl;

    // Load cellular mode configuration if specified
    if (!config.cellular_mode.empty()) {
        std::cout << "Loading cellular mode configuration from: " << config.cellular_mode << std::endl;
        std::string cellular_mode_json = readFile(config.cellular_mode);
        if (!cellular_mode_json.empty()) {
            Json::Value cellular_mode_config;
            Json::Reader reader;
            if (reader.parse(cellular_mode_json, cellular_mode_config)) {
                if (manager.loadCellularConfigFromJson(cellular_mode_config)) {
                    std::cout << "Cellular mode configuration loaded successfully" << std::endl;
                } else {
                    std::cerr << "Warning: Failed to apply cellular mode configuration" << std::endl;
                }
            } else {
                std::cerr << "Warning: Failed to parse cellular mode configuration JSON" << std::endl;
            }
        } else {
            std::cerr << "Warning: Failed to read cellular mode configuration file" << std::endl;
        }
    }

    // Load network configuration if specified
    if (!config.network.empty()) {
        std::cout << "Loading network configuration from: " << config.network << std::endl;
        std::string network_json = readFile(config.network);
        if (!network_json.empty()) {
            Json::Value network_config;
            Json::Reader reader;
            if (reader.parse(network_json, network_config)) {
                // Apply network configurations
                ConnectionConfig conn_config;
                if (network_config.isMember("apn")) {
                    conn_config.apn = network_config["apn"].asString();
                    std::cout << "Set APN from config: " << conn_config.apn << std::endl;
                }
                if (network_config.isMember("username")) {
                    conn_config.username = network_config["username"].asString();
                    std::cout << "Set username from config: " << conn_config.username << std::endl;
                }
                if (network_config.isMember("password")) {
                    conn_config.password = network_config["password"].asString();
                    std::cout << "Set password from config" << std::endl;
                }
                if (network_config.isMember("ip_type")) {
                    conn_config.ip_type = network_config["ip_type"].asInt();
                    std::cout << "Set IP type from config: " << conn_config.ip_type << std::endl;
                }
                if (network_config.isMember("auto_connect")) {
                    conn_config.auto_connect = network_config["auto_connect"].asBool();
                    std::cout << "Set auto connect from config: " << (conn_config.auto_connect ? "true" : "false") << std::endl;
                }
                if (network_config.isMember("retry_attempts")) {
                    conn_config.retry_attempts = network_config["retry_attempts"].asInt();
                    std::cout << "Set retry attempts from config: " << conn_config.retry_attempts << std::endl;
                }
                if (network_config.isMember("retry_delay_ms")) {
                    conn_config.retry_delay_ms = network_config["retry_delay_ms"].asInt();
                    std::cout << "Set retry delay from config: " << conn_config.retry_delay_ms << "ms" << std::endl;
                }
                if (network_config.isMember("enable_monitoring")) {
                    conn_config.enable_monitoring = network_config["enable_monitoring"].asBool();
                    config.enable_monitoring = conn_config.enable_monitoring;  // Update local flag
                    std::cout << "Set monitoring from config: " << (conn_config.enable_monitoring ? "enabled" : "disabled") << std::endl;
                }
                if (network_config.isMember("health_check_interval_ms")) {
                    conn_config.health_check_interval_ms = network_config["health_check_interval_ms"].asInt();
                    std::cout << "Set health check interval from config: " << conn_config.health_check_interval_ms << "ms" << std::endl;
                }
                
                manager.setConnectionConfig(conn_config);
                std::cout << "Network configuration processed" << std::endl;
            } else {
                std::cerr << "Warning: Failed to parse network configuration JSON" << std::endl;
            }
        } else {
            std::cerr << "Warning: Failed to read network configuration file" << std::endl;
        }
    }

    // Load cellular IP monitor configuration if specified
    if (!config.ip_monitor.empty()) {
        std::cout << "Loading cellular IP monitor configuration from: " << config.ip_monitor << std::endl;
        // Note: IP monitor configuration loading will be handled by the ConnectionManager
        std::cout << "Cellular IP monitor configuration file set: " << config.ip_monitor << std::endl;
    }
    
    // Initialize lifecycle manager for connection tracking
    // Parse device path from the JSON configuration
    std::string device_path = "";
    std::string interface_name = "";
    
    // Simple JSON parsing to extract device path and interface
    size_t device_pos = device_json.find("\"device_path\"");
    if (device_pos != std::string::npos) {
        size_t colon_pos = device_json.find(":", device_pos);
        if (colon_pos != std::string::npos) {
            size_t quote_start = device_json.find("\"", colon_pos);
            size_t quote_end = device_json.find("\"", quote_start + 1);
            if (quote_start != std::string::npos && quote_end != std::string::npos) {
                device_path = device_json.substr(quote_start + 1, quote_end - quote_start - 1);
            }
        }
    }
    
    size_t interface_pos = device_json.find("\"interface_name\"");
    if (interface_pos != std::string::npos) {
        size_t colon_pos = device_json.find(":", interface_pos);
        if (colon_pos != std::string::npos) {
            size_t quote_start = device_json.find("\"", colon_pos);
            size_t quote_end = device_json.find("\"", quote_start + 1);
            if (quote_start != std::string::npos && quote_end != std::string::npos) {
                interface_name = device_json.substr(quote_start + 1, quote_end - quote_start - 1);
            }
        }
    }
    
    if (!device_path.empty()) {
        lifecycle_manager = std::make_unique<ConnectionLifecycleManager>(device_path, interface_name, "internet");
        std::cout << "Connection registry tracking enabled for " << device_path << std::endl;
        std::cout << "Note: Both main and connection-specific signal handlers are active" << std::endl;
    }
    
    // Enable features
    if (config.enable_monitoring) {
        manager.startMonitoring();
        std::cout << "Monitoring enabled" << std::endl;
    }
    
    if (config.enable_auto_recovery) {
        manager.enableAutoRecovery(true);
        std::cout << "Auto recovery enabled" << std::endl;
    }
    
    // Connect
    std::cout << "Connecting..." << std::endl;
    ConnectionConfig default_config;
    default_config.apn = "internet";
    default_config.auto_connect = true;
    default_config.retry_attempts = 3;
    default_config.retry_delay_ms = 5000;
    
    if (!manager.connect(default_config)) {
        std::cerr << "Error: Failed to establish connection" << std::endl;
        return 1;
    }
    
    std::cout << "Connection established successfully" << std::endl;
    
    // Initialize thread manager
    try {
        std::cout << "Initializing thread manager..." << std::endl;
        g_threadManager = new ThreadMgr::ThreadManager(4); // Initial capacity of 4 threads
        
        // Create mainloop thread using ThreadManager
        std::cout << "Starting mainloop thread via ThreadManager..." << std::endl;
        g_mainloopThreadId = g_threadManager->createThread([&manager, config]() {
            mainloopThread(&manager, config.verbose);
        });
        
        std::cout << "Mainloop thread started with ID: " << g_mainloopThreadId << std::endl;
        
        // Main thread now waits for signals or thread completion
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Check if mainloop thread is still alive
            if (!g_threadManager->isThreadAlive(g_mainloopThreadId)) {
                std::cout << "Mainloop thread has terminated unexpectedly" << std::endl;
                break;
            }
        }
        
        std::cout << "Main thread received shutdown signal" << std::endl;
        
    } catch (const ThreadMgr::ThreadManagerException& e) {
        std::cerr << "Thread manager error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    // Cleanup
    std::cout << "Disconnecting..." << std::endl;
    manager.disconnect();
    manager.stopMonitoring();
    
    // Clean up thread manager if not already done
    if (g_threadManager) {
        std::cout << "Cleaning up thread manager..." << std::endl;
        try {
            if (g_mainloopThreadId != 0 && g_threadManager->isThreadAlive(g_mainloopThreadId)) {
                g_threadManager->stopThread(g_mainloopThreadId);
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Error during thread manager cleanup: " << e.what() << std::endl;
        }
        delete g_threadManager;
        g_threadManager = nullptr;
    }
    
    std::cout << "Connection manager shutdown complete" << std::endl;
    return 0;
}
