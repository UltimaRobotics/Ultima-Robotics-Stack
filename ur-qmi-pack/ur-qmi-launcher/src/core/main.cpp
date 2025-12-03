
#include "connection/connection_manager.h"
#include "utils/command_logger.h"
#include "utils/timeout_config.h"
#include "recovery/smart_routing.h"
#include "connection/connection_registry.h"
#include "rpc/rpc_client.h"
#include "rpc/rpc_operation_processor.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>  // for exit() and EXIT_FAILURE
#include "core/ThreadManager.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::atomic<bool> g_running(true);
ThreadMgr::ThreadManager* g_threadManager = nullptr;
static unsigned int g_mainloopThreadId = 0;
static unsigned int g_runtimeInitThreadId = 0;

// Global RPC client and operation processor for signal handling
static std::shared_ptr<RpcClient> g_rpcClient;
static std::unique_ptr<RpcOperationProcessor> g_operationProcessor;

// Global connection manager for heartbeat handler access
static ConnectionManager* g_connectionManager = nullptr;

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
    
    // Stop RPC client first
    if (g_rpcClient) {
        std::cout << "Step 0: Stopping RPC client..." << std::endl;
        g_rpcClient->stop();
        std::cout << "RPC client stopped successfully" << std::endl;
    }
    
    g_running.store(false);
    
    // Clean up connection manager
    if (g_connectionManager) {
        std::cout << "Step 0.5: Cleaning up connection manager..." << std::endl;
        try {
            g_connectionManager->disconnect();
            g_connectionManager->stopMonitoring();
            delete g_connectionManager;
            g_connectionManager = nullptr;
            std::cout << "Connection manager cleaned up successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Warning: Error during connection manager cleanup: " << e.what() << std::endl;
        }
    }
    
    // Stop the runtime initialization thread first
    if (g_threadManager && g_runtimeInitThreadId != 0) {
        std::cout << "Step 1: Stopping runtime initialization thread..." << std::endl;
        try {
            g_threadManager->stopThread(g_runtimeInitThreadId);
            std::cout << "Runtime initialization thread stopped successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Warning: Failed to stop runtime initialization thread: " << e.what() << std::endl;
        }
    }
    
    // Stop the mainloop thread second
    if (g_threadManager && g_mainloopThreadId != 0) {
        std::cout << "Step 2: Stopping mainloop thread..." << std::endl;
        try {
            g_threadManager->stopThread(g_mainloopThreadId);
            std::cout << "Mainloop thread stopped successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Warning: Failed to stop mainloop thread: " << e.what() << std::endl;
        }
    }
    
    // First, let connection registry handle its own cleanup
    std::cout << "Step 3: Connection registry cleanup..." << std::endl;
    ConnectionRegistry::handleGlobalTermination();
    
    // Then perform connection manager emergency cleanup
    ConnectionManager* active_manager = ConnectionManager::getActiveInstance();
    if (active_manager) {
        std::cout << "Step 4: Connection manager emergency cleanup..." << std::endl;
        active_manager->performEmergencyCleanup();
        std::cout << "Connection manager emergency cleanup completed" << std::endl;
    } else {
        std::cout << "Step 4: No active connection manager, performing basic cleanup..." << std::endl;
        
        // Perform basic cleanup if no connection manager is available
        std::cout << "Attempting basic WWAN interface cleanup..." << std::endl;
        system("pkill -f dhclient 2>/dev/null || true");
        system("ip route flush table main | grep wwan 2>/dev/null || true");
        
        // Try to bring down any wwan interfaces
        system("for iface in $(ls /sys/class/net/ | grep wwan 2>/dev/null); do ip link set dev $iface down 2>/dev/null || true; done");
        
        std::cout << "Basic cleanup completed" << std::endl;
    }
    
    // Clean up global resources
    std::cout << "Step 5: Global resource cleanup..." << std::endl;
    ConnectionRegistry::cleanup();
    
    // Clean up thread manager
    if (g_threadManager) {
        std::cout << "Step 6: Thread manager cleanup..." << std::endl;
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
              << "  -pkg_config FILE    Package configuration file containing paths to all JSON config files\n"
              << "  -rpc_config FILE    RPC configuration file for MQTT client settings\n"
              << "  -h, --help          Show this help message\n"
              << "\n"
              << "Example:\n"
              << "  " << program_name << " -pkg_config config/package_source.json -rpc_config config/rpc_config.json\n"
              << std::endl;
}

bool loadPackageConfig(const std::string& config_file, PackageConfig& config) {
    std::string content = readFile(config_file);
    if (content.empty()) {
        std::cerr << "Error: Cannot read package config file: " << config_file << std::endl;
        return false;
    }
    
    try {
        json root = json::parse(content);
        
        // Load config file paths
        if (root.contains("config_files")) {
            const auto& config_files = root["config_files"];
            config.device_json = config_files.value("device_json", "");
            config.cellular_mode = config_files.value("cellular_mode", "");
            config.timeouts = config_files.value("timeouts", "");
            config.network = config_files.value("network", "");
            config.ip_monitor = config_files.value("ip_monitor", "");
            config.routing = config_files.value("routing", "");
            config.log_file = config_files.value("log_file", "");
        }
        
        // Load flags
        if (root.contains("flags")) {
            const auto& flags = root["flags"];
            config.verbose = flags.value("verbose", false);
            config.enable_monitoring = flags.value("enable_monitoring", true);
            config.enable_auto_recovery = flags.value("enable_auto_recovery", true);
            config.verbose_cmd = flags.value("verbose_cmd", false);
            config.disable_auto_routing = flags.value("disable_auto_routing", false);
        }
        
        std::cout << "Package configuration loaded from: " << config_file << std::endl;
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "Error: Failed to parse package config JSON: " << e.what() << std::endl;
        return false;
    }
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

void sendDeviceListRequest() {
    std::cout << "Sending device list request to ur-qmi-ident..." << std::endl;
    
    // Create JSON-RPC request
    json request;
    request["id"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    request["jsonrpc"] = "2.0";
    request["method"] = "list_devices";
    request["params"] = json::object();
    
    // Send request via RPC client if available
    if (g_rpcClient) {
        try {
            std::string requestStr = request.dump();
            g_rpcClient->publishMessage("direct_messaging/ur-qmi-ident/requests", requestStr);
            std::cout << "Device list request sent successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to send device list request: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "RPC client not available - cannot send device list request" << std::endl;
    }
}

void startMainloopThread(ConnectionManager* manager, bool verbose) {
    std::cout << "Starting mainloop thread via ThreadManager..." << std::endl;
    g_mainloopThreadId = g_threadManager->createThread([manager, verbose]() {
        mainloopThread(manager, verbose);
    });
    std::cout << "Mainloop thread started with ID: " << g_mainloopThreadId << std::endl;
}

void runtimeInitialization(const std::string& package_source_file, const std::string& rpc_config_file) {
    std::cout << "Runtime initialization started in thread" << std::endl;
    std::cout << "Loading package source configuration from: " << package_source_file << std::endl;
    
    // Load package source configuration that contains paths to all JSON config files
    PackageConfig config;
    if (!loadPackageConfig(package_source_file, config)) {
        std::cerr << "Error: Failed to load package source configuration" << std::endl;
        g_running = false;
        return;
    }
    
    std::cout << "Package source configuration loaded successfully" << std::endl;
    std::cout << "Now loading individual JSON configuration files..." << std::endl;
    
    // Initialize RPC client if RPC config is provided
    if (!rpc_config_file.empty()) {
        std::cout << "Initializing RPC client with config: " << rpc_config_file << std::endl;
        try {
            // Create RPC client
            g_rpcClient = std::make_shared<RpcClient>(rpc_config_file, "ur-qmi-launcher");
            
            // Create operation processor
            g_operationProcessor = std::make_unique<RpcOperationProcessor>(config, config.verbose);
            
            // Set message handler to route requests to operation processor and handle heartbeat
            g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
                // Handle heartbeat messages from ur-qmi-ident
                if (topic.find("clients/ur-qmi-ident/heartbeat") != std::string::npos) {
                    std::cout << "Received heartbeat from ur-qmi-ident" << std::endl;
                    
                    // Send device list request
                    sendDeviceListRequest();
                    return;
                }
                
                // Handle device list responses
                if (topic.find("direct_messaging/ur-qmi-ident/responses") != std::string::npos) {
                    try {
                        json response = json::parse(payload);
                        if (response.contains("result") && response["result"].contains("devices")) {
                            auto devices = response["result"]["devices"];
                            std::cout << "Received device list response with " << devices.size() << " devices:" << std::endl;
                            
                            for (const auto& device : devices) {
                                std::cout << "  Device: " << device.value("model", "Unknown") 
                                          << " (IMEI: " << device.value("imei", "Unknown") 
                                          << ", Path: " << device.value("path", "Unknown") << ")" << std::endl;
                            }
                            
                            // Start mainloop thread after receiving device list
                            if (g_mainloopThreadId == 0 && g_threadManager && g_connectionManager) {
                                std::cout << "Starting mainloop thread after device discovery..." << std::endl;
                                startMainloopThread(g_connectionManager, config.verbose);
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to parse device list response: " << e.what() << std::endl;
                    }
                    return;
                }
                
                // Topic filtering for selective processing of launcher requests
                if (topic.find("direct_messaging/ur-qmi-launcher/requests") == std::string::npos) {
                    return;
                }
                
                // Delegate to operation processor
                if (g_operationProcessor) {
                    g_operationProcessor->processRequest(payload.c_str(), payload.size());
                }
            });
            
            // Start RPC client
            if (!g_rpcClient->start()) {
                std::cerr << "Failed to start RPC client" << std::endl;
                g_rpcClient.reset();
                g_operationProcessor.reset();
            } else {
                std::cout << "RPC client started successfully" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error initializing RPC client: " << e.what() << std::endl;
            g_rpcClient.reset();
            g_operationProcessor.reset();
        }
    } else {
        std::cout << "No RPC config file provided, RPC functionality disabled" << std::endl;
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
        g_running = false;
        return;
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
        g_running = false;
        return;
    }
    
    // Create connection manager (heap allocated for global access)
    g_connectionManager = new ConnectionManager();
    
    // Initialize connection lifecycle manager for registry tracking
    std::unique_ptr<ConnectionLifecycleManager> lifecycle_manager;
    
    // Set up callbacks
    g_connectionManager->setStateChangeCallback([config](ConnectionState state, const std::string& reason) {
        std::cout << "State changed to: " << static_cast<int>(state) 
                  << " (" << reason << ")" << std::endl;
        if (config.verbose) {
            std::cout << "Reason: " << reason << std::endl;
        }
    });
    
    g_connectionManager->setMetricsCallback([config](const ConnectionMetrics& metrics) {
        if (config.verbose) {
            std::cout << "Metrics - Signal: " << metrics.signal_strength 
                      << ", IP: " << metrics.ip_address 
                      << ", Connected: " << (metrics.is_connected ? "Yes" : "No") 
                      << std::endl;
        }
    });
    
    // Initialize
    if (!g_connectionManager->initialize(device_json)) {
        std::cerr << "Error: Failed to initialize connection manager" << std::endl;
        g_running = false;
        return;
    }
    
    std::cout << "Connection manager initialized successfully" << std::endl;

    // Load cellular mode configuration if specified
    if (!config.cellular_mode.empty()) {
        std::cout << "Loading cellular mode configuration from: " << config.cellular_mode << std::endl;
        std::string cellular_mode_json = readFile(config.cellular_mode);
        if (!cellular_mode_json.empty()) {
            try {
                json cellular_mode_config = json::parse(cellular_mode_json);
                if (g_connectionManager->loadCellularConfigFromJson(cellular_mode_config)) {
                    std::cout << "Cellular mode configuration loaded successfully" << std::endl;
                } else {
                    std::cerr << "Warning: Failed to apply cellular mode configuration" << std::endl;
                }
            } catch (const json::parse_error& e) {
                std::cerr << "Warning: Failed to parse cellular mode configuration JSON: " << e.what() << std::endl;
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
            try {
                json network_config = json::parse(network_json);
                // Apply network configurations
                ConnectionConfig conn_config;
                if (network_config.contains("apn")) {
                    conn_config.apn = network_config["apn"].get<std::string>();
                    std::cout << "Set APN from config: " << conn_config.apn << std::endl;
                }
                if (network_config.contains("username")) {
                    conn_config.username = network_config["username"].get<std::string>();
                    std::cout << "Set username from config: " << conn_config.username << std::endl;
                }
                if (network_config.contains("password")) {
                    conn_config.password = network_config["password"].get<std::string>();
                    std::cout << "Set password from config" << std::endl;
                }
                if (network_config.contains("ip_type")) {
                    conn_config.ip_type = network_config["ip_type"].get<int>();
                    std::cout << "Set IP type from config: " << conn_config.ip_type << std::endl;
                }
                if (network_config.contains("auto_connect")) {
                    conn_config.auto_connect = network_config["auto_connect"].get<bool>();
                    std::cout << "Set auto connect from config: " << (conn_config.auto_connect ? "true" : "false") << std::endl;
                }
                if (network_config.contains("retry_attempts")) {
                    conn_config.retry_attempts = network_config["retry_attempts"].get<int>();
                    std::cout << "Set retry attempts from config: " << conn_config.retry_attempts << std::endl;
                }
                if (network_config.contains("retry_delay_ms")) {
                    conn_config.retry_delay_ms = network_config["retry_delay_ms"].get<int>();
                    std::cout << "Set retry delay from config: " << conn_config.retry_delay_ms << "ms" << std::endl;
                }
                if (network_config.contains("enable_monitoring")) {
                    conn_config.enable_monitoring = network_config["enable_monitoring"].get<bool>();
                    config.enable_monitoring = conn_config.enable_monitoring;  // Update local flag
                    std::cout << "Set monitoring from config: " << (conn_config.enable_monitoring ? "enabled" : "disabled") << std::endl;
                }
                if (network_config.contains("health_check_interval_ms")) {
                    conn_config.health_check_interval_ms = network_config["health_check_interval_ms"].get<int>();
                    std::cout << "Set health check interval from config: " << conn_config.health_check_interval_ms << "ms" << std::endl;
                }
                
                g_connectionManager->setConnectionConfig(conn_config);
                std::cout << "Network configuration processed" << std::endl;
            } catch (const json::parse_error& e) {
                std::cerr << "Warning: Failed to parse network configuration JSON: " << e.what() << std::endl;
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
        g_connectionManager->startMonitoring();
        std::cout << "Monitoring enabled" << std::endl;
    }
    
    if (config.enable_auto_recovery) {
        g_connectionManager->enableAutoRecovery(true);
        std::cout << "Auto recovery enabled" << std::endl;
    }
    
    // Connect
    std::cout << "Connecting..." << std::endl;
    ConnectionConfig default_config;
    default_config.apn = "internet";
    default_config.auto_connect = true;
    default_config.retry_attempts = 3;
    default_config.retry_delay_ms = 5000;
    
    if (!g_connectionManager->connect(default_config)) {
        std::cerr << "Error: Failed to establish connection" << std::endl;
        g_running = false;
        return;
    }
    
    std::cout << "Connection established successfully" << std::endl;
    
    // Don't start mainloop thread automatically - wait for heartbeat trigger
    std::cout << "Mainloop thread autostart disabled - waiting for heartbeat trigger" << std::endl;
    
    // Main initialization loop - wait for shutdown signal
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "Runtime initialization received shutdown signal" << std::endl;
    
    // Cleanup
    std::cout << "Disconnecting..." << std::endl;
    g_connectionManager->disconnect();
    g_connectionManager->stopMonitoring();
    
    std::cout << "Runtime initialization complete" << std::endl;
}

int main(int argc, char* argv[]) {
    // Set up global signal handlers with high priority
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Main application signal handlers installed" << std::endl;
    
    // Parse command line arguments
    std::string package_config_file;
    std::string rpc_config_file;
    
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
        } else if (arg == "-rpc_config") {
            if (i + 1 < argc) {
                rpc_config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing RPC config file argument" << std::endl;
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
        std::cerr << "Error: Package config file is required (use -pkg_config)" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Initialize thread manager
    try {
        std::cout << "Initializing thread manager..." << std::endl;
        g_threadManager = new ThreadMgr::ThreadManager(4); // Initial capacity of 4 threads
        
        // Create runtime initialization thread using ThreadManager
        std::cout << "Starting runtime initialization thread via ThreadManager..." << std::endl;
        g_runtimeInitThreadId = g_threadManager->createThread([package_config_file, rpc_config_file]() {
            runtimeInitialization(package_config_file, rpc_config_file);
        });
        
        std::cout << "Runtime initialization thread started with ID: " << g_runtimeInitThreadId << std::endl;
        
        // Main thread now waits for signals or thread completion
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Check if runtime initialization thread is still alive
            if (!g_threadManager->isThreadAlive(g_runtimeInitThreadId)) {
                std::cout << "Runtime initialization thread has terminated" << std::endl;
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
    
    // Clean up thread manager if not already done
    if (g_threadManager) {
        std::cout << "Cleaning up thread manager..." << std::endl;
        try {
            if (g_runtimeInitThreadId != 0 && g_threadManager->isThreadAlive(g_runtimeInitThreadId)) {
                g_threadManager->stopThread(g_runtimeInitThreadId);
            }
            if (g_mainloopThreadId != 0 && g_threadManager->isThreadAlive(g_mainloopThreadId)) {
                g_threadManager->stopThread(g_mainloopThreadId);
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Error during thread manager cleanup: " << e.what() << std::endl;
        }
        delete g_threadManager;
        g_threadManager = nullptr;
    }
    
    std::cout << "Application shutdown complete" << std::endl;
    return 0;
}
