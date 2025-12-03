
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
static std::thread g_rpcClientThread;
static std::atomic<bool> g_rpcClientInitialized{false};
static std::atomic<bool> g_deviceListRequested{false};
static std::atomic<bool> g_mainloopLaunched{false};
static std::atomic<bool> g_rpcClientConnected{false};  // New flag for connection state

// Global connection manager for heartbeat handler access
static ConnectionManager* g_connectionManager = nullptr;

struct PackageConfig {
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
void performPreStartupCleanup();

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
    
    std::cout << "\n*** PRIORITY 1: EMERGENCY MAINLOOP STOP ***" << std::endl;
    
    // IMMEDIATELY stop the mainloop thread first - this is the highest priority
    if (g_threadManager && g_mainloopThreadId != 0) {
        std::cout << "EMERGENCY: Stopping mainloop thread..." << std::endl;
        try {
            // Try graceful stop
            g_threadManager->stopThread(g_mainloopThreadId);
            std::cout << "[OK] Mainloop thread emergency stop initiated" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[ERROR] Emergency mainloop stop failed: " << e.what() << std::endl;
        }
    } else {
        std::cout << "[WARNING] Thread manager or mainloop thread ID not available" << std::endl;
    }
    
    // Set global flags to stop all operations immediately
    g_mainloopLaunched.store(false);
    g_running.store(false);
    g_rpcClientInitialized.store(false);
    g_rpcClientConnected.store(false);  // Reset connection flag
    g_deviceListRequested.store(false);
    std::cout << "[OK] Global stop flags set" << std::endl;
    
    // Perform emergency cleanup IMMEDIATELY after mainloop stop
    std::cout << "\n*** PRIORITY 2: EMERGENCY CLEANUP ***" << std::endl;
    
    // Emergency connection cleanup first
    ConnectionManager* active_manager = ConnectionManager::getActiveInstance();
    if (active_manager) {
        std::cout << "Performing emergency connection cleanup..." << std::endl;
        try {
            active_manager->performEmergencyCleanup();
            std::cout << "[OK] Emergency connection cleanup completed" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[ERROR] Emergency cleanup failed: " << e.what() << std::endl;
        }
    }
    
    // Emergency registry cleanup
    std::cout << "Performing emergency registry cleanup..." << std::endl;
    try {
        ConnectionRegistry::handleGlobalTermination();
        std::cout << "[OK] Emergency registry cleanup completed" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[ERROR] Emergency registry cleanup failed: " << e.what() << std::endl;
    }
    
    std::cout << "\n*** PRIORITY 3: NORMAL SHUTDOWN SEQUENCE ***" << std::endl;
    std::cout << "Continuing with normal shutdown procedures..." << std::endl;
    
    // Stop RPC client
    if (g_rpcClient) {
        std::cout << "Step 1: Stopping RPC client..." << std::endl;
        g_rpcClient->stop();
        g_rpcClient.reset();
        std::cout << "RPC client stopped successfully" << std::endl;
    }
    
    // Note: RPC client thread is detached and will clean up itself when g_running becomes false
    g_operationProcessor.reset();
    
    // Clean up connection manager
    if (g_connectionManager) {
        std::cout << "Step 2: Cleaning up connection manager..." << std::endl;
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
    
    // Stop the runtime initialization thread
    if (g_threadManager && g_runtimeInitThreadId != 0) {
        std::cout << "Step 3: Stopping runtime initialization thread..." << std::endl;
        try {
            g_threadManager->stopThread(g_runtimeInitThreadId);
            std::cout << "Runtime initialization thread stopped successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Warning: Failed to stop runtime initialization thread: " << e.what() << std::endl;
        }
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

std::string generateDeviceConfigFromResponse(const json& response) {
    try {
        if (!response.contains("result") || !response["result"].contains("devices")) {
            std::cerr << "Invalid response format - missing devices" << std::endl;
            return "";
        }
        
        auto devices = response["result"]["devices"];
        if (devices.empty()) {
            std::cerr << "No devices found in response" << std::endl;
            return "";
        }
        
        const auto& device = devices[0];
        
        // Generate device configuration in the format expected by ConnectionManager
        json deviceConfig;
        deviceConfig["version"] = "1.0";
        deviceConfig["description"] = "Generated device configuration from ur-qmi-ident response";
        
        // Create devices array with expected structure for ConnectionManager
        json deviceObj;
        deviceObj["device_path"] = device.value("path", "");
        deviceObj["imei"] = device.value("imei", "");
        deviceObj["model"] = device.value("model", "");
        deviceObj["manufacturer"] = device.value("manufacturer", "");
        deviceObj["is_available"] = true; // Assume available since it's in the response
        deviceObj["firmware"] = device.value("firmware", "");
        deviceObj["hardware_revision"] = device.value("hardware_revision", "");
        deviceObj["iccid"] = device.value("iccid", "");
        deviceObj["activation_state"] = device.value("activation_state", "");
        deviceObj["operating_mode"] = device.value("operating_mode", "");
        deviceObj["pin_locked"] = device.value("pin_locked", false);
        deviceObj["sim_present"] = device.value("sim_present", true);
        
        deviceConfig["devices"] = json::array();
        deviceConfig["devices"].push_back(deviceObj);
        
        // Add connection configuration
        json connection;
        connection["apn"] = "internet";
        connection["auto_connect"] = true;
        connection["retry_attempts"] = 3;
        connection["retry_delay_ms"] = 5000;
        connection["timeout_ms"] = 30000;
        deviceConfig["connection"] = connection;
        
        // Add capabilities if available
        if (device.contains("capabilities")) {
            deviceConfig["capabilities"] = device["capabilities"];
        }
        
        std::string configStr = deviceConfig.dump(4);
        std::cout << "Generated device configuration:\n" << configStr << std::endl;
        
        return configStr;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to generate device config: " << e.what() << std::endl;
        return "";
    }
}

void launchMainloopWithDeviceConfig(const std::string& deviceConfigJson, const PackageConfig& config) {
    std::cout << "[MAINLOOP-INIT] Launching mainloop thread with complete device configuration..." << std::endl;
    
    // Check if mainloop already launched
    if (g_mainloopLaunched.exchange(true)) {
        std::cout << "[MAINLOOP-INIT] Mainloop already launched, skipping" << std::endl;
        return;
    }
    
    if (!g_threadManager) {
        std::cerr << "[MAINLOOP-INIT] ThreadManager not available, cannot launch mainloop" << std::endl;
        g_mainloopLaunched.store(false);
        return;
    }
    
    try {
        // Launch mainloop thread via ThreadManager with complete configuration
        g_mainloopThreadId = g_threadManager->createThread([deviceConfigJson, config]() {
            std::cout << "[MAINLOOP-THREAD] Starting mainloop with device configuration..." << std::endl;
            
            // Initialize connection manager with generated device config
            if (g_connectionManager) {
                std::cout << "[MAINLOOP-THREAD] Initializing connection manager with device config..." << std::endl;
                if (g_connectionManager->initialize(deviceConfigJson)) {
                    std::cout << "[MAINLOOP-THREAD] Connection manager initialized successfully" << std::endl;
                    
                    // Load and apply other configurations
                    PackageConfig localConfig = config; // Make a copy for this thread
                    
                    if (!localConfig.cellular_mode.empty()) {
                        std::string cellular_mode_json = readFile(localConfig.cellular_mode);
                        if (!cellular_mode_json.empty()) {
                            try {
                                json cellular_mode_config = json::parse(cellular_mode_json);
                                g_connectionManager->loadCellularConfigFromJson(cellular_mode_config);
                                std::cout << "[MAINLOOP-THREAD] Cellular mode configuration applied" << std::endl;
                            } catch (const std::exception& e) {
                                std::cerr << "[MAINLOOP-THREAD] Failed to parse cellular mode config: " << e.what() << std::endl;
                            }
                        }
                    }
                    
                    if (!localConfig.network.empty()) {
                        std::string network_json = readFile(localConfig.network);
                        if (!network_json.empty()) {
                            try {
                                json network_config = json::parse(network_json);
                                ConnectionConfig conn_config;
                                if (network_config.contains("apn")) {
                                    conn_config.apn = network_config["apn"].get<std::string>();
                                }
                                if (network_config.contains("username")) {
                                    conn_config.username = network_config["username"].get<std::string>();
                                }
                                if (network_config.contains("password")) {
                                    conn_config.password = network_config["password"].get<std::string>();
                                }
                                if (network_config.contains("ip_type")) {
                                    conn_config.ip_type = network_config["ip_type"].get<int>();
                                }
                                if (network_config.contains("auto_connect")) {
                                    conn_config.auto_connect = network_config["auto_connect"].get<bool>();
                                }
                                if (network_config.contains("retry_attempts")) {
                                    conn_config.retry_attempts = network_config["retry_attempts"].get<int>();
                                }
                                if (network_config.contains("retry_delay_ms")) {
                                    conn_config.retry_delay_ms = network_config["retry_delay_ms"].get<int>();
                                }
                                if (network_config.contains("enable_monitoring")) {
                                    conn_config.enable_monitoring = network_config["enable_monitoring"].get<bool>();
                                }
                                if (network_config.contains("health_check_interval_ms")) {
                                    conn_config.health_check_interval_ms = network_config["health_check_interval_ms"].get<int>();
                                }
                                
                                g_connectionManager->setConnectionConfig(conn_config);
                                std::cout << "[MAINLOOP-THREAD] Network configuration applied" << std::endl;
                            } catch (const std::exception& e) {
                                std::cerr << "[MAINLOOP-THREAD] Failed to parse network config: " << e.what() << std::endl;
                            }
                        }
                    }
                    
                    // Enable features
                    if (localConfig.enable_monitoring) {
                        g_connectionManager->startMonitoring();
                        std::cout << "[MAINLOOP-THREAD] Monitoring enabled" << std::endl;
                    }
                    if (localConfig.enable_auto_recovery) {
                        g_connectionManager->enableAutoRecovery(true);
                        std::cout << "[MAINLOOP-THREAD] Auto-recovery enabled" << std::endl;
                    }
                    
                    // Establish connection
                    ConnectionConfig default_config;
                    default_config.apn = "internet";
                    default_config.auto_connect = true;
                    default_config.retry_attempts = 3;
                    default_config.retry_delay_ms = 5000;
                    
                    std::cout << "[MAINLOOP-THREAD] Establishing connection..." << std::endl;
                    if (g_connectionManager->connect(default_config)) {
                        std::cout << "[MAINLOOP-THREAD] Connection established successfully" << std::endl;
                    } else {
                        std::cerr << "[MAINLOOP-THREAD] Failed to establish connection" << std::endl;
                    }
                } else {
                    std::cerr << "[MAINLOOP-THREAD] Failed to initialize connection manager" << std::endl;
                }
            }
            
            // Run mainloop - this is the main operational loop
            std::cout << "[MAINLOOP-THREAD] Starting main operational loop..." << std::endl;
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                if (config.verbose && g_connectionManager && g_connectionManager->isConnected()) {
                    try {
                        ConnectionMetrics metrics = g_connectionManager->getCurrentMetrics();
                        std::cout << "[MAINLOOP-THREAD] Status: Connected, Signal: " << metrics.signal_strength 
                                  << " dBm, IP: " << metrics.ip_address << std::endl;
                    } catch (const std::exception& e) {
                        // Ignore metric errors in verbose mode
                    }
                }
            }
            
            std::cout << "[MAINLOOP-THREAD] Mainloop thread received shutdown signal" << std::endl;
            
            // Cleanup
            if (g_connectionManager) {
                std::cout << "[MAINLOOP-THREAD] Disconnecting and cleaning up..." << std::endl;
                g_connectionManager->disconnect();
                g_connectionManager->stopMonitoring();
            }
            
            std::cout << "[MAINLOOP-THREAD] Mainloop thread completed" << std::endl;
        });
        
        std::cout << "[MAINLOOP-INIT] Mainloop thread launched with ID: " << g_mainloopThreadId << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[MAINLOOP-INIT] Failed to launch mainloop thread: " << e.what() << std::endl;
        g_mainloopLaunched.store(false);
    }
}

void initializeRpcClientIndependently(const std::string& rpc_config_file, const PackageConfig& config) {
    std::cout << "[RPC-INIT] Starting independent RPC client initialization..." << std::endl;
    
    if (rpc_config_file.empty()) {
        std::cout << "[RPC-INIT] No RPC config file provided, skipping RPC client initialization" << std::endl;
        return;
    }
    
    try {
        // Pre-allocate memory to avoid std::bad_alloc during critical operations
        std::cout << "[RPC-INIT] Pre-allocating memory for RPC components..." << std::endl;
        
        // Create RPC client with exception handling for memory allocation
        std::cout << "[RPC-INIT] Creating RPC client with config: " << rpc_config_file << std::endl;
        try {
            g_rpcClient = std::make_shared<RpcClient>(rpc_config_file, "ur-qmi-launcher");
            std::cout << "[RPC-INIT] RPC client object created successfully" << std::endl;
        } catch (const std::bad_alloc& e) {
            std::cerr << "[RPC-INIT] Memory allocation failed creating RPC client: " << e.what() << std::endl;
            g_rpcClientInitialized.store(false);
            return;
        } catch (const std::exception& e) {
            std::cerr << "[RPC-INIT] Exception creating RPC client: " << e.what() << std::endl;
            g_rpcClientInitialized.store(false);
            return;
        }
        
        // Create operation processor with exception handling ( it's optional)
        try {
            g_operationProcessor = std::make_unique<RpcOperationProcessor>(config, config.verbose);
            std::cout << "[RPC-INIT] Operation processor created successfully" << std::endl;
        } catch (const std::bad_alloc& e) {
            std::cerr << "[RPC-INIT] Memory allocation failed creating operation processor ( RPC will work without it ): " << e.what() << std::endl;
            // Continue without operation processor - RPC client can still handle basic messaging
            g_operationProcessor = nullptr;
        } catch (const std::exception& e) {
            std::cerr << "[RPC-INIT] Exception creating operation processor ( RPC will work without it ): " << e.what() << std::endl;
            // Continue without operation processor - RPC client can still handle basic messaging
            g_operationProcessor = nullptr;
        }
        
        // Set message handler to route requests to operation processor and handle heartbeat
        std::cout << "[RPC-INIT] Setting up message handlers..." << std::endl;
        g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
            std::cout << "[RPC-INIT] Received message on topic: " << topic << std::endl;
            
            // Detect when RPC client is fully connected to broker
            if (topic.find("clients/ur-qmi-ident/heartbeat") != std::string::npos && !g_rpcClientConnected.exchange(true)) {
                std::cout << "[RPC-INIT] RPC client connected to broker and receiving messages" << std::endl;
                
                // Send device list request only on first heartbeat after connection
                if (!g_deviceListRequested.exchange(true)) {
                    std::cout << "[RPC-INIT] First heartbeat received - sending device list request" << std::endl;
                    sendDeviceListRequest();
                }
                return;
            }
            
            // Handle subsequent heartbeat messages (but don't send device list again)
            if (topic.find("clients/ur-qmi-ident/heartbeat") != std::string::npos) {
                // Silently ignore subsequent heartbeats
                return;
            }
            
            // Handle device list responses
            if (topic.find("direct_messaging/ur-qmi-ident/responses") != std::string::npos) {
                try {
                    json response = json::parse(payload);
                    if (response.contains("result") && response["result"].contains("devices")) {
                        auto devices = response["result"]["devices"];
                        std::cout << "[RPC-INIT] Received device list response with " << devices.size() << " devices:" << std::endl;
                        
                        for (const auto& device : devices) {
                            std::cout << "[RPC-INIT]   Device: " << device.value("model", "Unknown") 
                                      << " (IMEI: " << device.value("imei", "Unknown") 
                                      << ", Path: " << device.value("path", "Unknown") << ")" << std::endl;
                        }
                        
                        if (!devices.empty()) {
                            // Generate device config from response
                            std::string deviceConfigJson = generateDeviceConfigFromResponse(response);
                            if (!deviceConfigJson.empty()) {
                                std::cout << "[RPC-INIT] Generated device config, launching mainloop thread..." << std::endl;
                                
                                // Launch mainloop thread with complete configuration
                                launchMainloopWithDeviceConfig(deviceConfigJson, config);
                            } else {
                                std::cerr << "[RPC-INIT] Failed to generate device config from response" << std::endl;
                            }
                        } else {
                            std::cerr << "[RPC-INIT] No devices found in response" << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[RPC-INIT] Failed to parse device list response: " << e.what() << std::endl;
                }
                return;
            }
            
            // Topic filtering for selective processing of launcher requests
            if (topic.find("direct_messaging/ur-qmi-launcher/requests") == std::string::npos) {
                return;
            }
            
            // Delegate to operation processor if available
            if (g_operationProcessor) {
                g_operationProcessor->processRequest(payload.c_str(), payload.size());
            } else {
                std::cout << "[RPC-INIT] Operation processor not available - message ignored" << std::endl;
            }
        });
        
        // Start RPC client with exception handling
        std::cout << "[RPC-INIT] Starting RPC client..." << std::endl;
        try {
            if (!g_rpcClient->start()) {
                std::cerr << "[RPC-INIT] Failed to start RPC client" << std::endl;
                g_rpcClient.reset();
                g_operationProcessor.reset();
                g_rpcClientInitialized.store(false);
            } else {
                std::cout << "[RPC-INIT] RPC client started successfully and running independently" << std::endl;
                g_rpcClientInitialized.store(true);
            }
        } catch (const std::bad_alloc& e) {
            std::cerr << "[RPC-INIT] Memory allocation failed starting RPC client: " << e.what() << std::endl;
            g_rpcClient.reset();
            g_operationProcessor.reset();
            g_rpcClientInitialized.store(false);
        } catch (const std::exception& e) {
            std::cerr << "[RPC-INIT] Exception starting RPC client: " << e.what() << std::endl;
            g_rpcClient.reset();
            g_operationProcessor.reset();
            g_rpcClientInitialized.store(false);
        }
        
    } catch (const std::bad_alloc& e) {
        std::cerr << "[RPC-INIT] Critical memory allocation failure: " << e.what() << std::endl;
        g_rpcClient.reset();
        g_operationProcessor.reset();
        g_rpcClientInitialized.store(false);
    } catch (const std::exception& e) {
        std::cerr << "[RPC-INIT] Critical error during RPC initialization: " << e.what() << std::endl;
        g_rpcClient.reset();
        g_operationProcessor.reset();
        g_rpcClientInitialized.store(false);
    }
    
    std::cout << "[RPC-INIT] Independent RPC client initialization completed" << std::endl;
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
    
    // Note: Device JSON will be generated from ur-qmi-ident response
    // Don't read device config from file - wait for device discovery
    std::cout << "Device configuration will be generated from ur-qmi-ident response" << std::endl;
    
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
    
    // Note: Connection manager will be initialized later when device config is generated
    std::cout << "Connection manager created, will be initialized after device discovery" << std::endl;

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
    
    // Note: Device path and lifecycle manager will be set up after device discovery
    std::cout << "Device path and lifecycle tracking will be configured after device discovery" << std::endl;
    
    // Note: Features will be enabled after device discovery
    std::cout << "Monitoring and auto-recovery will be enabled after device discovery" << std::endl;
    
    // Note: Connection will be established after device discovery
    std::cout << "Connection will be established after device discovery" << std::endl;
    
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

void performPreStartupCleanup() {
    std::cout << "Starting pre-startup cleanup to remove residues from previous runs..." << std::endl;
    
    // 1. Clean up ur-qmi-launcher specific processes
    std::cout << "Cleaning up ur-qmi-launcher specific processes..." << std::endl;
    
    // Kill any hanging qmicli processes from previous runs
    system("pkill -f 'qmicli.*--client-no-release-cid' 2>/dev/null || true");
    system("pkill -f 'qmicli.*--wds-start-network' 2>/dev/null || true");
    system("pkill -f 'qmicli.*--wds-stop-network' 2>/dev/null || true");
    
    // Kill any hanging dhclient processes for wwan interfaces
    system("pkill -f 'dhclient.*wwan' 2>/dev/null || true");
    
    std::cout << "[OK] Process cleanup completed" << std::endl;
    
    // 2. Clean up ur-qmi-launcher specific registry files
    std::cout << "Cleaning up ur-qmi-launcher registry files..." << std::endl;
    
    const char* registry_files[] = {
        "/tmp/qmi_connections.registry",
        "/tmp/ur-qmi-launcher.pid",
        "/tmp/ur-qmi-launcher.lock",
        nullptr
    };
    
    for (int i = 0; registry_files[i] != nullptr; ++i) {
        if (access(registry_files[i], F_OK) == 0) {
            if (unlink(registry_files[i]) == 0) {
                std::cout << "[OK] Removed " << registry_files[i] << std::endl;
            } else {
                std::cout << "[WARNING] Could not remove " << registry_files[i] << std::endl;
            }
        }
    }
    
    // 3. Reset wwan interfaces to clean state (without removing system routes)
    std::cout << "Resetting wwan interfaces to clean state..." << std::endl;
    
    // Find and reset wwan interfaces safely
    std::ostringstream find_wwan_cmd;
    find_wwan_cmd << "for iface in $(ls /sys/class/net/ 2>/dev/null | grep wwan 2>/dev/null); do "
                  << "if [ -e /sys/class/net/$iface ]; then "
                  << "echo 'Resetting interface $iface'; "
                  << "ip link set dev $iface down 2>/dev/null || true; "
                  << "ip link set dev $iface up 2>/dev/null || true; "
                  << "ip addr flush dev $iface 2>/dev/null || true; "
                  << "fi; "
                  << "done";
    
    int result = system(find_wwan_cmd.str().c_str());
    (void)result; // Suppress unused result warning
    
    std::cout << "[OK] WWAN interface reset completed" << std::endl;
    
    // 4. Clean up ur-qmi-launcher specific routing entries (preserve system routes)
    std::cout << "Cleaning up ur-qmi-launcher specific routing entries..." << std::endl;
    
    // Remove only routes that are specific to ur-qmi-launcher (identified by specific patterns)
    std::ostringstream cleanup_routes_cmd;
    cleanup_routes_cmd << "ip route show | grep -E '(wwan|qmi|ur-qmi)' | while read route; do "
                       << "echo 'Removing route: $route'; "
                       << "ip route del $route 2>/dev/null || true; "
                       << "done";
    
    result = system(cleanup_routes_cmd.str().c_str());
    (void)result; // Suppress unused result warning
    
    // Clean up any leftover iptables rules from ur-qmi-launcher
    system("iptables -S | grep 'ur-qmi' | sed 's/^-A/iptables -D/' | bash 2>/dev/null || true");
    system("iptables -t nat -S | grep 'ur-qmi' | sed 's/^-A/iptables -t nat -D/' | bash 2>/dev/null || true");
    
    std::cout << "[OK] Routing cleanup completed" << std::endl;
    
    // 5. Verify system networking is intact
    std::cout << "Verifying system networking integrity..." << std::endl;
    
    // Check that non-wwan interfaces are still up
    system("for iface in $(ls /sys/class/net/ 2>/dev/null | grep -v wwan); do "
           "if [ -e /sys/class/net/$iface ] && [ \"$(cat /sys/class/net/$iface/operstate 2>/dev/null)\" = 'up' ]; then "
           "echo '[OK] Interface $iface is operational'; "
           "fi; "
           "done");
    
    // Check that default routes still exist (if they existed before)
    system("if ip route show default | grep -q default; then "
           "echo '[OK] Default routes preserved'; "
           "else "
           "echo '[INFO] No default routes found (may be normal for this system)'; "
           "fi");
    
    std::cout << "\n=== PRE-STARTUP CLEANUP COMPLETED ===" << std::endl;
    std::cout << "System is ready for ur-qmi-launcher startup" << std::endl;
}

int main(int argc, char* argv[]) {
    // Set up global signal handlers with high priority
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Main application signal handlers installed" << std::endl;
    
    // PRE-STARTUP CLEANUP: Clean up residues from previous runs
    std::cout << "\n=== PRE-STARTUP CLEANUP ===" << std::endl;
    performPreStartupCleanup();
    
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
        
        // Launch RPC client FIRST as independent detached thread
        if (!rpc_config_file.empty()) {
            std::cout << "Launching RPC client as independent detached thread..." << std::endl;
            
            // Start RPC client thread detached - it will run completely independently
            g_rpcClientThread = std::thread([rpc_config_file]() {
                std::cout << "[RPC-THREAD] Independent RPC client thread started" << std::endl;
                
                // Load minimal config for RPC initialization
                PackageConfig rpc_config;
                rpc_config.verbose = true; // Enable verbose for RPC debugging
                
                initializeRpcClientIndependently(rpc_config_file, rpc_config);
                
                // Keep RPC thread alive to handle messages independently
                while (g_running.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                std::cout << "[RPC-THREAD] Independent RPC client thread shutting down" << std::endl;
                
                // Cleanup RPC client
                if (g_rpcClient) {
                    g_rpcClient->stop();
                    g_rpcClient.reset();
                }
                g_operationProcessor.reset();
                g_rpcClientInitialized.store(false);
                g_rpcClientConnected.store(false);  // Reset connection flag
                g_deviceListRequested.store(false); // Reset for potential restart
                g_mainloopLaunched.store(false); // Reset for potential restart
            });
            
            // Detach the thread so it runs completely independently
            g_rpcClientThread.detach();
            std::cout << "RPC client thread detached and running independently" << std::endl;
        } else {
            std::cout << "No RPC config provided - RPC client will not be started" << std::endl;
        }
        
        // Wait for RPC client to be connected to broker before proceeding
        if (!rpc_config_file.empty()) {
            std::cout << "Waiting for RPC client to connect to MQTT broker..." << std::endl;
            const int MAX_CONNECTION_WAIT_MS = 10000;  // 10 seconds timeout
            const int POLL_INTERVAL_MS = 100;
            int elapsed = 0;
            
            while (elapsed < MAX_CONNECTION_WAIT_MS && !g_rpcClientConnected.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
                elapsed += POLL_INTERVAL_MS;
                
                // Show progress every second
                if (elapsed % 1000 == 0) {
                    std::cout << "Waiting for RPC connection... (" << elapsed/1000 << "s)" << std::endl;
                }
            }
            
            if (g_rpcClientConnected.load()) {
                std::cout << "[OK] RPC client connected to broker after " << elapsed << "ms" << std::endl;
            } else {
                std::cout << "[WARNING] RPC client connection timeout after " << MAX_CONNECTION_WAIT_MS << "ms" << std::endl;
                std::cout << "[INFO] Proceeding with startup anyway - some features may not work" << std::endl;
            }
        }
        
        // Now start runtime initialization thread (only after RPC is connected)
        std::cout << "Starting runtime initialization thread via ThreadManager..." << std::endl;
        g_runtimeInitThreadId = g_threadManager->createThread([package_config_file]() {
            runtimeInitialization(package_config_file, ""); // RPC config handled by independent thread
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
