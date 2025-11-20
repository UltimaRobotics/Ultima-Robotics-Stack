
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

static std::atomic<bool> g_running(true);

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
    
    // Give a moment for cleanup to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    std::cout << "=== COORDINATED SHUTDOWN COMPLETED ===" << std::endl;
    std::cout << "Exiting application..." << std::endl;
    exit(EXIT_SUCCESS);
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] <device_json_file>\n"
              << "Options:\n"
              << "  -h, --help           Show this help message\n"
              << "  -c, --config FILE    Use configuration file\n"
              << "  -t, --timeout FILE   Use timeout configuration file\n"
              << "  -s, --routing FILE   Use smart routing configuration file\n"
              << "  -a, --apn APN        Set APN for connection\n"
              << "  -u, --username USER  Set username\n"
              << "  -p, --password PASS  Set password\n"
              << "  -4, --ipv4           Use IPv4 only (default)\n"
              << "  -6, --ipv6           Use IPv6 only\n"
              << "  -46, --dual-stack    Use dual stack IPv4/IPv6\n"
              << "  -m, --monitor        Enable monitoring\n"
              << "  -r, --auto-recovery  Enable auto recovery\n"
              << "  -l, --log FILE       Log metrics to file\n"
              << "  -v, --verbose        Verbose output\n"
              << "  --verbose-cmd        Enable verbose command logging\n"
              << "  --print-timeouts     Print current timeout configuration\n"
              << "  --save-timeouts FILE Save current timeouts to JSON file\n"
              << "  --print-routing      Print current routing configuration\n"
              << "  --save-routing FILE  Save current routing config to JSON file\n"
              << "  --no-auto-routing    Disable automatic routing management\n"
              << "  --basic              Use basic device profile\n"
              << "  --advanced           Use advanced device profile\n"
              << "\n"
              << "Cellular Configuration:\n"
              << "  --cellular-mode FILE       Load cellular mode configuration from JSON\n"
              << "  --timeouts-config FILE     Load custom timeouts configuration from JSON\n"
              << "  --cellular-ip-monitor FILE Load IP monitoring configuration from JSON\n"
              << "  --dev-config FILE          Load device-specific configuration from JSON\n"
              << "  --cellular-network FILE    Load cellular network settings from JSON\n"
              << "\n"
              << "Connection Management:\n"
              << "  --kill <connection_ref>    Kill existing connection by reference\n"
              << "  --kill-all                 Kill all active connections\n"
              << "  --list-connections         List all active connections\n"
              << "  --connection-status <ref>  Show status of specific connection\n"
              << std::endl;
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

int main(int argc, char* argv[]) {
    // Set up global signal handlers with high priority
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Main application signal handlers installed" << std::endl;
    
    // Parse command line arguments
    std::string device_json_file;
    std::string config_file;
    std::string timeout_config_file;
    std::string routing_config_file;
    std::string save_timeouts_file;
    std::string save_routing_file;
    std::string cellular_mode_config_file;
    std::string timeouts_config_file;
    std::string cellular_ip_monitor_config_file;
    std::string dev_config_file;
    std::string cellular_network_config_file;
    std::string apn = "internet";
    std::string username;
    std::string password;
    std::string log_file;
    int ip_type = 4;
    bool enable_monitoring = false;
    bool enable_auto_recovery = false;
    bool verbose = false;
    bool verbose_cmd = false;
    bool print_timeouts = false;
    bool print_routing = false;
    bool disable_auto_routing = false;
    bool use_advanced = false;
    std::string kill_connection_ref;
    bool kill_all_connections = false;
    bool list_connections = false;
    std::string connection_status_ref;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing config file argument" << std::endl;
                return 1;
            }
        } else if (arg == "-t" || arg == "--timeout") {
            if (i + 1 < argc) {
                timeout_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing timeout config file argument" << std::endl;
                return 1;
            }
        } else if (arg == "-s" || arg == "--routing") {
            if (i + 1 < argc) {
                routing_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing routing config file argument" << std::endl;
                return 1;
            }
        } else if (arg == "-a" || arg == "--apn") {
            if (i + 1 < argc) {
                apn = argv[++i];
            } else {
                std::cerr << "Error: Missing APN argument" << std::endl;
                return 1;
            }
        } else if (arg == "-u" || arg == "--username") {
            if (i + 1 < argc) {
                username = argv[++i];
            }
        } else if (arg == "-p" || arg == "--password") {
            if (i + 1 < argc) {
                password = argv[++i];
            }
        } else if (arg == "-4" || arg == "--ipv4") {
            ip_type = 4;
        } else if (arg == "-6" || arg == "--ipv6") {
            ip_type = 6;
        } else if (arg == "-46" || arg == "--dual-stack") {
            ip_type = 46;
        } else if (arg == "-m" || arg == "--monitor") {
            enable_monitoring = true;
        } else if (arg == "-r" || arg == "--auto-recovery") {
            enable_auto_recovery = true;
        } else if (arg == "-l" || arg == "--log") {
            if (i + 1 < argc) {
                log_file = argv[++i];
            }
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--verbose-cmd") {
            verbose_cmd = true;
        } else if (arg == "--print-timeouts") {
            print_timeouts = true;
        } else if (arg == "--save-timeouts") {
            if (i + 1 < argc) {
                save_timeouts_file = argv[++i];
            } else {
                std::cerr << "Error: Missing save timeouts file argument" << std::endl;
                return 1;
            }
        } else if (arg == "--print-routing") {
            print_routing = true;
        } else if (arg == "--save-routing") {
            if (i + 1 < argc) {
                save_routing_file = argv[++i];
            } else {
                std::cerr << "Error: Missing save routing file argument" << std::endl;
                return 1;
            }
        } else if (arg == "--no-auto-routing") {
            disable_auto_routing = true;
        } else if (arg == "--basic") {
            use_advanced = false;
        } else if (arg == "--advanced") {
            use_advanced = true;
        } else if (arg == "--cellular-mode") {
            if (i + 1 < argc) {
                cellular_mode_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing cellular mode config file argument" << std::endl;
                return 1;
            }
        } else if (arg == "--timeouts-config") {
            if (i + 1 < argc) {
                timeouts_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing timeouts config file argument" << std::endl;
                return 1;
            }
        } else if (arg == "--cellular-ip-monitor") {
            if (i + 1 < argc) {
                cellular_ip_monitor_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing cellular IP monitor config file argument" << std::endl;
                return 1;
            }
        } else if (arg == "--dev-config") {
            if (i + 1 < argc) {
                dev_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing device config file argument" << std::endl;
                return 1;
            }
        } else if (arg == "--cellular-network") {
            if (i + 1 < argc) {
                cellular_network_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing cellular network config file argument" << std::endl;
                return 1;
            }
        } else if (arg == "--kill") {
            if (i + 1 < argc) {
                kill_connection_ref = argv[++i];
            } else {
                std::cerr << "Error: Missing connection reference argument for --kill" << std::endl;
                return 1;
            }
        } else if (arg == "--kill-all") {
            kill_all_connections = true;
        } else if (arg == "--list-connections") {
            list_connections = true;
        } else if (arg == "--connection-status") {
            if (i + 1 < argc) {
                connection_status_ref = argv[++i];
            } else {
                std::cerr << "Error: Missing connection reference argument for --connection-status" << std::endl;
                return 1;
            }
        } else if (arg[0] != '-') {
            device_json_file = arg;
        } else {
            std::cerr << "Error: Unknown argument " << arg << std::endl;
            return 1;
        }
    }
    
    // Initialize connection registry
    ConnectionRegistry::initialize();
    
    // Handle connection management commands
    if (!kill_connection_ref.empty()) {
        std::cout << "Killing connection: " << kill_connection_ref << std::endl;
        return ConnectionRegistry::killConnection(kill_connection_ref) ? 0 : 1;
    }
    
    if (kill_all_connections) {
        std::cout << "Killing all connections..." << std::endl;
        return ConnectionRegistry::killAllConnections() ? 0 : 1;
    }
    
    if (list_connections) {
        ConnectionRegistry::printConnectionsList();
        return 0;
    }
    
    if (!connection_status_ref.empty()) {
        ConnectionRegistry::printConnectionStatus(connection_status_ref);
        return 0;
    }
    
    if (device_json_file.empty() && !print_timeouts && save_timeouts_file.empty() && 
        !print_routing && save_routing_file.empty()) {
        std::cerr << "Error: Device JSON file is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Load timeout configuration if specified (original flag)
    if (!timeout_config_file.empty()) {
        std::cout << "Loading timeout configuration from: " << timeout_config_file << std::endl;
        if (!g_timeout_config.loadFromFile(timeout_config_file)) {
            std::cerr << "Warning: Failed to load timeout configuration, using defaults" << std::endl;
        }
    }

    // Load timeouts configuration if specified (new flag)
    if (!timeouts_config_file.empty()) {
        std::cout << "Loading timeouts configuration from: " << timeouts_config_file << std::endl;
        if (!g_timeout_config.loadFromFile(timeouts_config_file)) {
            std::cerr << "Warning: Failed to load timeouts configuration, using defaults" << std::endl;
        }
    }
    
    // Handle timeout-related commands
    if (print_timeouts) {
        g_timeout_config.printConfiguration();
        if (!save_timeouts_file.empty() || device_json_file.empty()) {
            // If only printing timeouts or saving, exit after printing
            return 0;
        }
    }
    
    if (!save_timeouts_file.empty()) {
        std::cout << "Saving timeout configuration to: " << save_timeouts_file << std::endl;
        if (!g_timeout_config.saveToFile(save_timeouts_file)) {
            std::cerr << "Error: Failed to save timeout configuration" << std::endl;
            return 1;
        }
        if (device_json_file.empty()) {
            // If only saving timeouts, exit after saving
            return 0;
        }
    }
    
    // Load smart routing configuration
    SmartRoutingConfig routing_config;
    if (!routing_config_file.empty()) {
        std::cout << "Loading smart routing configuration from: " << routing_config_file << std::endl;
        if (!routing_config.loadFromFile(routing_config_file)) {
            std::cerr << "Warning: Failed to load routing configuration, using defaults" << std::endl;
        }
    }
    
    // Disable auto routing if requested
    if (disable_auto_routing) {
        routing_config.auto_routing_enabled = false;
        std::cout << "Automatic routing disabled" << std::endl;
    }
    
    // Handle routing-related commands
    if (print_routing) {
        routing_config.printConfiguration();
        if (!save_routing_file.empty() || device_json_file.empty()) {
            return 0;
        }
    }
    
    if (!save_routing_file.empty()) {
        std::cout << "Saving routing configuration to: " << save_routing_file << std::endl;
        if (!routing_config.saveToFile(save_routing_file)) {
            std::cerr << "Error: Failed to save routing configuration" << std::endl;
            return 1;
        }
        if (device_json_file.empty()) {
            return 0;
        }
    }
    
    // Initialize smart routing manager
    if (!g_smart_routing.initialize(routing_config)) {
        std::cerr << "Error: Failed to initialize smart routing manager" << std::endl;
        return 1;
    }
    
    // Set up routing change callback
    g_smart_routing.setRoutingChangeCallback([verbose](RoutingOperation operation, const RoutingRule& rule, bool success, const std::string& error) {
        if (verbose) {
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
    if (verbose_cmd) {
        CommandLogger::setVerboseEnabled(true);
        std::cout << "Verbose command logging enabled" << std::endl;
    }
    
    // Read device JSON
    std::string device_json = readFile(device_json_file);
    if (device_json.empty()) {
        return 1;
    }
    
    // Create connection manager
    ConnectionManager manager;
    
    // Initialize connection lifecycle manager for registry tracking
    std::unique_ptr<ConnectionLifecycleManager> lifecycle_manager;
    
    // Set up callbacks
    manager.setStateChangeCallback([verbose](ConnectionState state, const std::string& reason) {
        std::cout << "State changed to: " << static_cast<int>(state) 
                  << " (" << reason << ")" << std::endl;
        if (verbose) {
            std::cout << "Reason: " << reason << std::endl;
        }
    });
    
    manager.setMetricsCallback([verbose](const ConnectionMetrics& metrics) {
        if (verbose) {
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
    if (!cellular_mode_config_file.empty()) {
        std::cout << "Loading cellular mode configuration from: " << cellular_mode_config_file << std::endl;
        std::string cellular_mode_json = readFile(cellular_mode_config_file);
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

    // Load device-specific configuration if specified
    if (!dev_config_file.empty()) {
        std::cout << "Loading device configuration from: " << dev_config_file << std::endl;
        std::string dev_config_json = readFile(dev_config_file);
        if (!dev_config_json.empty()) {
            Json::Value dev_config;
            Json::Reader reader;
            if (reader.parse(dev_config_json, dev_config)) {
                // Apply device-specific configurations
                if (dev_config.isMember("device_path")) {
                    std::string new_device_path = dev_config["device_path"].asString();
                    std::cout << "Device path from config: " << new_device_path << std::endl;
                    if (!manager.selectDevice(new_device_path)) {
                        std::cerr << "Warning: Failed to select device from config" << std::endl;
                    }
                }
                std::cout << "Device configuration processed" << std::endl;
            } else {
                std::cerr << "Warning: Failed to parse device configuration JSON" << std::endl;
            }
        } else {
            std::cerr << "Warning: Failed to read device configuration file" << std::endl;
        }
    }

    // Load cellular IP monitor configuration if specified
    if (!cellular_ip_monitor_config_file.empty()) {
        std::cout << "Loading cellular IP monitor configuration from: " << cellular_ip_monitor_config_file << std::endl;
        // Note: IP monitor configuration loading will be handled by the ConnectionManager
        // when it initializes the IP monitor component
        std::cout << "Cellular IP monitor configuration file set: " << cellular_ip_monitor_config_file << std::endl;
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
        lifecycle_manager = std::make_unique<ConnectionLifecycleManager>(device_path, interface_name, apn);
        std::cout << "Connection registry tracking enabled for " << device_path << std::endl;
        std::cout << "Note: Both main and connection-specific signal handlers are active" << std::endl;
    }
    
    // Configure connection
    ConnectionConfig config;
    config.apn = apn;
    config.username = username;
    config.password = password;
    config.ip_type = ip_type;
    config.auto_connect = true;
    config.retry_attempts = 3;
    config.retry_delay_ms = 5000;
    config.enable_monitoring = enable_monitoring;
    config.health_check_interval_ms = 30000;

    // Load cellular network configuration if specified
    if (!cellular_network_config_file.empty()) {
        std::cout << "Loading cellular network configuration from: " << cellular_network_config_file << std::endl;
        std::string cellular_network_json = readFile(cellular_network_config_file);
        if (!cellular_network_json.empty()) {
            Json::Value cellular_config;
            Json::Reader reader;
            if (reader.parse(cellular_network_json, cellular_config)) {
                if (cellular_config.isMember("apn")) {
                    config.apn = cellular_config["apn"].asString();
                    std::cout << "Set APN from config: " << config.apn << std::endl;
                }
                if (cellular_config.isMember("username")) {
                    config.username = cellular_config["username"].asString();
                    std::cout << "Set username from config: " << config.username << std::endl;
                }
                if (cellular_config.isMember("password")) {
                    config.password = cellular_config["password"].asString();
                    std::cout << "Set password from config" << std::endl;
                }
                if (cellular_config.isMember("ip_type")) {
                    config.ip_type = cellular_config["ip_type"].asInt();
                    std::cout << "Set IP type from config: " << config.ip_type << std::endl;
                }
                if (cellular_config.isMember("auto_connect")) {
                    config.auto_connect = cellular_config["auto_connect"].asBool();
                    std::cout << "Set auto connect from config: " << (config.auto_connect ? "true" : "false") << std::endl;
                }
                if (cellular_config.isMember("retry_attempts")) {
                    config.retry_attempts = cellular_config["retry_attempts"].asInt();
                    std::cout << "Set retry attempts from config: " << config.retry_attempts << std::endl;
                }
                if (cellular_config.isMember("retry_delay_ms")) {
                    config.retry_delay_ms = cellular_config["retry_delay_ms"].asInt();
                    std::cout << "Set retry delay from config: " << config.retry_delay_ms << "ms" << std::endl;
                }
                if (cellular_config.isMember("enable_monitoring")) {
                    config.enable_monitoring = cellular_config["enable_monitoring"].asBool();
                    enable_monitoring = config.enable_monitoring;  // Update local flag
                    std::cout << "Set monitoring from config: " << (config.enable_monitoring ? "enabled" : "disabled") << std::endl;
                }
                if (cellular_config.isMember("health_check_interval_ms")) {
                    config.health_check_interval_ms = cellular_config["health_check_interval_ms"].asInt();
                    std::cout << "Set health check interval from config: " << config.health_check_interval_ms << "ms" << std::endl;
                }
            } else {
                std::cerr << "Warning: Failed to parse cellular network configuration JSON" << std::endl;
            }
        } else {
            std::cerr << "Warning: Failed to read cellular network configuration file" << std::endl;
        }
    }
    
    manager.setConnectionConfig(config);
    
    // Enable features
    if (enable_monitoring) {
        manager.startMonitoring();
        std::cout << "Monitoring enabled" << std::endl;
    }
    
    if (enable_auto_recovery) {
        manager.enableAutoRecovery(true);
        std::cout << "Auto recovery enabled" << std::endl;
    }
    
    // Connect
    std::cout << "Connecting with APN: " << apn << std::endl;
    if (!manager.connect(config)) {
        std::cerr << "Error: Failed to establish connection" << std::endl;
        return 1;
    }
    
    std::cout << "Connection established successfully" << std::endl;
    
    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (verbose && manager.isConnected()) {
            ConnectionMetrics metrics = manager.getCurrentMetrics();
            std::cout << "Status: Connected, Signal: " << metrics.signal_strength 
                      << " dBm, IP: " << metrics.ip_address << std::endl;
        }
    }
    
    // Cleanup
    std::cout << "Disconnecting..." << std::endl;
    manager.disconnect();
    manager.stopMonitoring();
    
    std::cout << "Connection manager shutdown complete" << std::endl;
    return 0;
}
