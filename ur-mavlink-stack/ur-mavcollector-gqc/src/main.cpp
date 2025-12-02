#include <iostream>
#include <iomanip>
#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <variant>
#include <cstdlib>  // For std::getenv

#include "JsonConfig.h"
#include "JsonDataPrinter.h"
#include "MAVLinkUdpConnection.h"
#include "Vehicle.h"
#include "ParameterManager.h"
#include "FactMetaData.h"

// Global variables for signal handling
std::shared_ptr<MAVLinkUdpConnection> g_connection;
std::shared_ptr<Vehicle> g_vehicle;
std::atomic<bool> g_running(true);
std::ofstream g_dataLog;
std::unique_ptr<JsonDataPrinter> g_jsonPrinter;

// Signal handler for graceful shutdown
void signalHandler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    exit(0);
    if (g_dataLog.is_open()) {
        g_dataLog << "\n=== Shutdown at " << std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() << " ===" << std::endl;
        g_dataLog.close();
    }
    g_running = false;
}

// Data logging functions
void logMessage(const std::string& message)
{
    std::cout << message << std::endl;
    if (g_dataLog.is_open()) {
        g_dataLog << message << std::endl;
        g_dataLog.flush(); // Ensure data is written immediately
    }
}

// JSON-based logging functions
void logTelemetryData(Vehicle* vehicle)
{
    if (!vehicle || !g_jsonPrinter) return;
    
    if (g_jsonPrinter->shouldPrint("telemetry_data")) {
        std::string jsonData = g_jsonPrinter->printTelemetryData(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    // Print individual data types based on configuration
    if (g_jsonPrinter->shouldPrint("system_status")) {
        std::string jsonData = g_jsonPrinter->printSystemStatus(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("gps_data")) {
        std::string jsonData = g_jsonPrinter->printGpsData(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("gps2_data")) {
        std::string jsonData = g_jsonPrinter->printGps2Data(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("battery_data")) {
        std::string jsonData = g_jsonPrinter->printBatteryData(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("rc_data")) {
        std::string jsonData = g_jsonPrinter->printRcData(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("vibration_data")) {
        std::string jsonData = g_jsonPrinter->printVibrationData(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("temperature_data")) {
        std::string jsonData = g_jsonPrinter->printTemperatureData(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("estimator_status")) {
        std::string jsonData = g_jsonPrinter->printEstimatorStatus(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("wind_data")) {
        std::string jsonData = g_jsonPrinter->printWindData(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
}

void logParameters(Vehicle* vehicle)
{
    if (!vehicle || !g_jsonPrinter) return;

    if (g_jsonPrinter->shouldPrint("parameters")) {
        std::string jsonData = g_jsonPrinter->printParameters(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
}

void printVehicleInfo(Vehicle* vehicle)
{
    if (!vehicle || !g_jsonPrinter) return;

    if (g_jsonPrinter->shouldPrint("vehicle_info")) {
        std::string jsonData = g_jsonPrinter->printVehicleInfo(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("heartbeat")) {
        std::string jsonData = g_jsonPrinter->printHeartbeat(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
    
    if (g_jsonPrinter->shouldPrint("autopilot_version")) {
        std::string jsonData = g_jsonPrinter->printAutopilotVersion(vehicle);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
}

void printConnectionStats(const MAVLinkUdpConnection* connection)
{
    if (!connection || !g_jsonPrinter) return;

    if (g_jsonPrinter->shouldPrint("connection_stats")) {
        std::string jsonData = g_jsonPrinter->printConnectionStats(connection);
        if (!jsonData.empty()) {
            logMessage(jsonData);
        }
    }
}

int main(int argc, char* argv[])
{
    std::string configFilePath = "config.json";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-pkg_config" && i + 1 < argc) {
            configFilePath = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "MAVLink Data Collector - Enhanced with JSON Output" << std::endl;
            std::cout << "Usage: " << argv[0] << " -pkg_config <config_file>" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -pkg_config <file>    Path to JSON configuration file (default: config.json)" << std::endl;
            std::cout << "  -h, --help           Show this help message" << std::endl;
            std::cout << "\nJSON Configuration Format:" << std::endl;
            std::cout << "  {" << std::endl;
            std::cout << "    \"target_address\": \"127.0.0.1\"," << std::endl;
            std::cout << "    \"port\": 14550," << std::endl;
            std::cout << "    \"system_id\": 255," << std::endl;
            std::cout << "    \"component_id\": 158," << std::endl;
            std::cout << "    \"health_check_enabled\": true," << std::endl;
            std::cout << "    \"auto_restart_enabled\": true," << std::endl;
            std::cout << "    \"connection_timeout_ms\": 5000," << std::endl;
            std::cout << "    \"restart_delay_ms\": 1000," << std::endl;
            std::cout << "    \"verbose_logging\": false," << std::endl;
            std::cout << "    \"show_statistics\": true," << std::endl;
            std::cout << "    \"enable_data_logging\": false," << std::endl;
            std::cout << "    \"log_file_path\": \"mavlink_data.log\"," << std::endl;
            std::cout << "    \"version_check_enabled\": true," << std::endl;
            std::cout << "    \"auto_version_detection\": true," << std::endl;
            std::cout << "    \"output_format\": \"json\"," << std::endl;
            std::cout << "    \"print_config\": {" << std::endl;
            std::cout << "      \"vehicle_info\": true," << std::endl;
            std::cout << "      \"gps_data\": true," << std::endl;
            std::cout << "      \"battery_data\": true," << std::endl;
            std::cout << "      \"parameters\": true," << std::endl;
            std::cout << "      \"connection_stats\": true" << std::endl;
            std::cout << "    }," << std::endl;
            std::cout << "    \"data_fields\": {" << std::endl;
            std::cout << "      \"gps_data\": [\"lat\", \"lon\", \"alt\", \"hdop\", \"satellites_visible\"]," << std::endl;
            std::cout << "      \"battery_data\": [\"voltage\", \"current\", \"percent\"]" << std::endl;
            std::cout << "    }," << std::endl;
            std::cout << "    \"json_output\": {" << std::endl;
            std::cout << "      \"pretty_print\": true," << std::endl;
            std::cout << "      \"include_timestamp\": true," << std::endl;
            std::cout << "      \"include_metadata\": true" << std::endl;
            std::cout << "    }" << std::endl;
            std::cout << "  }" << std::endl;
            return 0;
        }
    }
    
    // Load JSON configuration
    JsonConfig config;
    if (!config.loadFromFile(configFilePath)) {
        std::cerr << "Failed to load configuration from: " << configFilePath << std::endl;
        return 1;
    }
    
    // Initialize JSON printer
    g_jsonPrinter = std::make_unique<JsonDataPrinter>(config);
    
    // Extract configuration values
    std::string targetAddress = config.getString("target_address", "127.0.0.1");
    int port = config.getInt("port", 14550);
    uint8_t systemId = static_cast<uint8_t>(config.getInt("system_id", 255));
    uint8_t componentId = static_cast<uint8_t>(config.getInt("component_id", 158));
    
    bool verbose = config.getBool("verbose_logging", false);
    bool showStats = config.getBool("show_statistics", false);
    bool checkVersion = config.getBool("version_check_enabled", true);
    bool enableLogging = config.getBool("enable_data_logging", false);
    std::string logFileName = config.getString("log_file_path", "mavlink_data.log");
    
    // Get output format
    std::string outputFormat = config.getOutputFormat();
    
    // Health monitoring options
    bool enableHealthCheck = config.getBool("health_check_enabled", true);
    bool enableAutoRestart = config.getBool("auto_restart_enabled", true);
    uint32_t connectionTimeout = static_cast<uint32_t>(config.getInt("connection_timeout_ms", 5000));
    uint32_t restartDelay = static_cast<uint32_t>(config.getInt("restart_delay_ms", 1000));
    
    // Print loaded configuration
    std::cout << "=== Configuration Loaded from: " << configFilePath << " ===" << std::endl;
    config.printConfig();

    // Initialize data logging if enabled
    if (enableLogging) {
        g_dataLog.open(logFileName, std::ios::out | std::ios::app);
        if (g_dataLog.is_open()) {
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            g_dataLog << "\n=== MAVLink Data Collection Started at " << timestamp << " ===" << std::endl;
            g_dataLog << "Config File: " << configFilePath << std::endl;
            g_dataLog << "Target: " << targetAddress << ":" << port << std::endl;
            g_dataLog << "System ID: " << static_cast<int>(systemId) << std::endl;
            g_dataLog << "Component ID: " << static_cast<int>(componentId) << std::endl;
            g_dataLog << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
            g_dataLog << "=============================" << std::endl;
            g_dataLog.flush();
        } else {
            std::cerr << "Warning: Could not open log file " << logFileName << std::endl;
        }
    }

    // Always perform version check at startup
    if (checkVersion) {
        std::ostringstream oss;
        oss << "=== Version Information ===" << std::endl;
        oss << "MAVLink Data Collector v1.0 - Enhanced" << std::endl;
        oss << "Based on QGroundControl Parameter Manager" << std::endl;
        oss << "Build date: " << __DATE__ << " " << __TIME__ << std::endl;
        oss << "MAVLink v2.0 support enabled" << std::endl;
        if (enableLogging) oss << "Data logging enabled: " << logFileName << std::endl;
        oss << "Configuration: " << configFilePath << std::endl;
        oss << "=============================" << std::endl;
        logMessage(oss.str());
    }

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    logMessage("MAVLink Data Collector - Enhanced");
    logMessage("=======================");
    
    // Show configuration summary
    std::ostringstream configInfo;
    configInfo << "Configuration Summary:" << std::endl;
    configInfo << "Target: " << targetAddress << ":" << port << std::endl;
    configInfo << "System ID: " << static_cast<int>(systemId) << std::endl;
    configInfo << "Component ID: " << static_cast<int>(componentId) << std::endl;
    logMessage(configInfo.str());

    // Create connection
    g_connection = std::make_shared<MAVLinkUdpConnection>();
    
    // Configure system ID and component ID from JSON configuration
    g_connection->setSystemId(systemId);
    g_connection->setComponentId(componentId);
    
    // Configure connection health monitoring
    g_connection->enableConnectionHealthCheck(enableHealthCheck);
    g_connection->setAutoRestartEnabled(enableAutoRestart);
    g_connection->setConnectionTimeout(connectionTimeout);
    g_connection->setAutoRestartDelay(restartDelay);
    
    if (enableHealthCheck) {
        logMessage("Connection health monitoring enabled:");
        std::ostringstream healthConfig;
        healthConfig << "- Timeout: " << connectionTimeout << "ms" << std::endl;
        healthConfig << "- Auto-restart: " << (enableAutoRestart ? "enabled" : "disabled") << std::endl;
        if (enableAutoRestart) {
            healthConfig << "- Restart delay: " << restartDelay << "ms" << std::endl;
        }
        logMessage(healthConfig.str());
    } else {
        logMessage("Connection health monitoring disabled");
    }
    
    // Set up callbacks
    g_connection->setConnectionChangedCallback([](bool connected) {
        logMessage("Connection " + std::string(connected ? "established" : "lost"));
    });

    g_connection->setMessageReceivedCallback([verbose, enableLogging](const mavlink_message_t& message) {
        std::ostringstream oss;
        if (verbose) {
            oss << "Message: " << static_cast<int>(message.msgid) 
                << " from sys " << static_cast<int>(message.sysid) 
                << " comp " << static_cast<int>(message.compid);
            logMessage(oss.str());
        }
        
        if (enableLogging) {
            // Log every MAVLink message for complete data collection
            if (g_dataLog.is_open()) {
                auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                g_dataLog << timestamp << ",MSG," << static_cast<int>(message.msgid) 
                         << "," << static_cast<int>(message.sysid) 
                         << "," << static_cast<int>(message.compid) 
                         << "," << static_cast<int>(message.len) << std::endl;
                g_dataLog.flush();
            }
        }
    });

    // Connect to vehicle
    if (!g_connection->connect(targetAddress, port)) {
        std::cerr << "Failed to connect to " << targetAddress << ":" << port << std::endl;
        if (g_dataLog.is_open()) {
            g_dataLog << "ERROR: Failed to connect" << std::endl;
            g_dataLog.close();
        }
        return 1;
    }

    // Create vehicle with system ID from configuration
    g_vehicle = std::make_shared<Vehicle>(g_connection.get());
    
    // Configure system ID and component ID from JSON configuration
    g_vehicle->setSystemId(systemId);
    g_vehicle->setComponentId(componentId);
    
    // Set up parameter manager callbacks
    auto paramManager = g_vehicle->parameterManager();
    if (paramManager) {
        paramManager->setParametersReadyCallback([](bool ready) {
            logMessage("Parameters " + std::string(ready ? "ready!" : "not ready"));
        });
        
        paramManager->setLoadProgressCallback([](double progress) {
            logMessage("Parameter loading progress: " + std::to_string(progress * 100.0) + "%");
        });
        
        // Start parameter loading
        logMessage("Starting parameter loading...");
        paramManager->refreshAllParameters();
    }

    // Set up vehicle change callback
    g_vehicle->setVehicleChangedCallback([](const Vehicle* vehicle) {
        logMessage("Vehicle state changed");
    });

    logMessage("Connected! Waiting for vehicle data...");
    logMessage("Press Ctrl+C to stop.");

    // Main loop with comprehensive data collection
    auto lastPrintTime = std::chrono::steady_clock::now();
    auto printInterval = std::chrono::seconds(1);
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Print and log telemetry data periodically
        auto now = std::chrono::steady_clock::now();
        if (now - lastPrintTime >= printInterval) {
            logParameters(g_vehicle.get());
            logTelemetryData(g_vehicle.get());
            
            if (showStats) {
                printConnectionStats(g_connection.get());
            }
            
            lastPrintTime = now;
        }
    }

    // Final statistics
    logMessage("\n=== Final Statistics ===");
    printConnectionStats(g_connection.get());
    printVehicleInfo(g_vehicle.get());

    logMessage("\nShutting down...");
    
    // Cleanup
    g_vehicle.reset();
    g_connection.reset();
    
    if (g_dataLog.is_open()) {
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        g_dataLog << "\n=== MAVLink Data Collection Ended at " << timestamp << " ===" << std::endl;
        g_dataLog.close();
    }
    
    logMessage("Done.");
    return 0;
}
