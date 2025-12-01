
#include "qmi_watchdog.h"
#include "watchdog_thread_args.h"
#include "rpc_client.hpp"
#include "rpc_operation_processor.hpp"
#include "device_watchdog_manager.hpp"
#include "ThreadManager.hpp"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <string>
#include <atomic>
#include <chrono>
#include <json/json.h>
#include <algorithm>

ThreadMgr::ThreadManager* g_thread_manager = nullptr;
unsigned int g_watchdog_thread_id = 0;
static std::atomic<bool> g_running(true);
std::string packageConfigPath;
bool hasPackageConfig = false;

// RPC client globals
std::string rpcConfigPath;
bool hasRpcConfig = false;
std::shared_ptr<RpcClient> g_rpcClient;
std::unique_ptr<RpcOperationProcessor> g_operationProcessor;

// Verbose mode flag
bool verboseMode = false;

// Device watchdog manager
std::unique_ptr<DeviceWatchdogManager> g_device_manager;

// Startup cron job state
std::atomic<bool> g_startup_request_sent{false};
std::atomic<bool> g_device_list_received{false};

Json::Value monitoringConfig;
Json::Value failureDetectionConfig;

bool ValidatedStartupInstance_0 = false;

void signalHandler(int /* signal */) {
    std::cout << "Shutting down system..." << std::endl;
    g_running = false;
    exit(0);
    // Stop device manager first
    if (g_device_manager) {
        std::cout << "Stopping device manager..." << std::endl;
        g_device_manager->stopAllMonitoring();
    }
    
    // Stop RPC client
    if (g_rpcClient) {
        std::cout << "Stopping RPC client..." << std::endl;
        g_rpcClient->stop();
    }
    
    if (g_thread_manager && g_watchdog_thread_id != 0) {
        std::cout << "Stopping watchdog thread..." << std::endl;
        g_thread_manager->stopThread(g_watchdog_thread_id);
    }
    
}

void threads_monitor_lookfor(){
    #ifdef __THREAD_MON
    std::cout << "\nMonitoring thread states..." << std::endl;
    auto threadIds = manager.getAllThreadIds();
    for (auto id : threadIds) {
        auto info = manager.getThreadInfo(id);
        std::cout << "Thread " << id << " state: ";
        switch (info.state) {
            case ThreadState::Created: std::cout << "Created"; break;
            case ThreadState::Running: std::cout << "Running"; break;
            case ThreadState::Paused: std::cout << "Paused"; break;
            case ThreadState::Stopped: std::cout << "Stopped"; break;
            case ThreadState::Error: std::cout << "Error"; break;
        }
        std::cout << std::endl;
    }
    #endif
}

void watchdogThreadFunction(void* arg) {
    WatchdogThreadArgs* thread_args = static_cast<WatchdogThreadArgs*>(arg);
    
    QMIWatchdog watchdog;
    watchdog.setVerbose(verboseMode);
    
    if (!watchdog.loadDeviceConfigFromFile(*thread_args->config_path)) {
        std::cerr << "Error: Failed to load device configuration\n";
        return;
    }
    
    watchdog.setFailureDetectionCallback([](const std::string& event_type, const std::vector<std::string>& failures) {
        std::cout << "\n!!! FAILURE DETECTED !!!\n";
        std::cout << "Event: " << event_type << "\n";
        for (const auto& failure : failures) {
            std::cout << "- " << failure << "\n";
        }
    });
    
    std::cout << "Starting continuous monitoring...\n";
    if (!watchdog.startMonitoring()) {
        std::cerr << "Error: Failed to start monitoring\n";
        return;
    }
    
    // Run the monitoring loop directly
    watchdog.monitoringLoop();
    
    std::cout << "Watchdog thread exiting...\n";
}

void handleDeviceDiscoveryEvent(const std::string& payload) {
    try {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(payload, root)) {
            std::cerr << "[Discovery] Failed to parse device discovery JSON" << std::endl;
            return;
        }
        
        // Validate JSON-RPC 2.0 format
        if (!root.isMember("jsonrpc") || root["jsonrpc"].asString() != "2.0") {
            std::cerr << "[Discovery] Invalid JSON-RPC version" << std::endl;
            return;
        }
        
        if (!root.isMember("method") || root["method"].asString() != "device_discovery_event") {
            std::cerr << "[Discovery] Not a device discovery event" << std::endl;
            return;
        }
        
        if (!root.isMember("params")) {
            std::cerr << "[Discovery] Missing params in device discovery event" << std::endl;
            return;
        }
        
        const Json::Value& params = root["params"];
        if (!params.isMember("event_type") || !params.isMember("device_data")) {
            std::cerr << "[Discovery] Missing event_type or device_data in params" << std::endl;
            return;
        }
        
        std::string event_type = params["event_type"].asString();
        const Json::Value& device_data = params["device_data"];
        
        if (!device_data.isMember("path")) {
            std::cerr << "[Discovery] Missing device path in device_data" << std::endl;
            return;
        }
        
        std::string device_path = device_data["path"].asString();
        std::cout << "[Discovery] Processing device discovery event: " << event_type 
                  << " for device: " << device_path << std::endl;
        
        if (event_type == "device_added") {
            // Start monitoring for the new device
            if (g_device_manager) {
                std::cout << "[Discovery] Device add event received for: " << device_path << std::endl;
                
                // Force cleanup of any stale device entries before checking
                g_device_manager->cleanupUnavailableDevices();
                g_device_manager->cleanupStoppedDevices();
                
                // Always attempt to start monitoring for added devices
                // even if they appear to be monitored, to handle reconnection cases
                bool was_already_monitored = g_device_manager->isDeviceMonitored(device_path);
                
                if (was_already_monitored) {
                    std::cout << "[Discovery] Device " << device_path 
                              << " appears to be monitored, but this is a reconnection - forcing restart" << std::endl;
                    
                    // Force remove the existing monitoring immediately
                    g_device_manager->forceRemoveDevice(device_path);
                }
                
                std::cout << "[Discovery] Starting fresh monitoring for device: " << device_path << std::endl;
                if (g_device_manager->startDeviceMonitoring(device_path, monitoringConfig, failureDetectionConfig)) {
                    std::cout << "[Discovery] Successfully started monitoring for: " << device_path << std::endl;
                } else {
                    std::cerr << "[Discovery] Failed to start monitoring for: " << device_path << std::endl;
                }
            }
        } else if (event_type == "device_removed") {
            // Stop monitoring for the removed device
            if (g_device_manager) {
                std::cout << "[Discovery] Stopping monitoring for removed device: " << device_path << std::endl;
                if (g_device_manager->stopDeviceMonitoring(device_path)) {
                    std::cout << "[Discovery] Successfully stopped monitoring for: " << device_path << std::endl;
                } else {
                    std::cerr << "[Discovery] Failed to stop monitoring for: " << device_path << std::endl;
                }
            }
        } else {
            std::cout << "[Discovery] Unknown event type: " << event_type << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[Discovery] Exception handling device discovery event: " << e.what() << std::endl;
    }
}

void sendDeviceListRequest() {
    if (!g_rpcClient || g_startup_request_sent.load()) {
        return;
    }
    
    // Create device list request
    Json::Value request;
    request["jsonrpc"] = "2.0";
    request["id"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    request["method"] = "list_devices";
    request["params"] = Json::Value(Json::objectValue);
    
    Json::StreamWriterBuilder builder;
    std::string request_json = Json::writeString(builder, request);
    
    std::cout << "[Startup] Sending device list request to ur-qmi-ident" << std::endl;
    g_rpcClient->sendResponse("direct_messaging/ur-qmi-ident/requests", request_json);
    g_startup_request_sent.store(true);
}

void handleDeviceListResponse(const std::string& payload) {
    try {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(payload, root)) {
            std::cerr << "[Startup] Failed to parse device list response JSON" << std::endl;
            return;
        }
        
        // Validate JSON-RPC 2.0 format
        if (!root.isMember("jsonrpc") || root["jsonrpc"].asString() != "2.0") {
            std::cerr << "[Startup] Invalid JSON-RPC version in device list response" << std::endl;
            return;
        }
        
        if (!root.isMember("result")) {
            std::cerr << "[Startup] Missing result in device list response" << std::endl;
            return;
        }
        
        const Json::Value& result = root["result"];
        if (!result.isMember("devices")) {
            std::cerr << "[Startup] Missing devices array in device list response" << std::endl;
            return;
        }
        
        const Json::Value& devices = result["devices"];
        std::cout << "[Startup] Received device list response with " << devices.size() << " devices" << std::endl;
        
        // Start monitoring for each existing device
        if (g_device_manager) {
            for (const auto& device : devices) {
                if (!device.isMember("path")) {
                    std::cerr << "[Startup] Device entry missing path field" << std::endl;
                    continue;
                }
                
                std::string device_path = device["path"].asString();
                std::cout << "[Startup] Starting monitoring for existing device: " << device_path << std::endl;
                
                if (!g_device_manager->isDeviceMonitored(device_path)) {
                    if (g_device_manager->startDeviceMonitoring(device_path, monitoringConfig, failureDetectionConfig)) {
                        std::cout << "[Startup] Successfully started monitoring for: " << device_path << std::endl;
                    } else {
                        std::cerr << "[Startup] Failed to start monitoring for: " << device_path << std::endl;
                    }
                } else {
                    std::cout << "[Startup] Device " << device_path << " is already being monitored" << std::endl;
                }
            }
        }
        
        g_device_list_received.store(true);
        std::cout << "[Startup] Device list processing completed" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[Startup] Exception handling device list response: " << e.what() << std::endl;
    }
}

void handleHeartbeat(const std::string& /* payload */) {
    std::cout << "[Heartbeat] Received heartbeat from ur-qmi-ident" << std::endl;
    
    // Trigger startup device list request on first heartbeat (one-time cron job)
    if (!g_startup_request_sent.load()) {
        std::cout << "[Heartbeat] Triggering startup device list request" << std::endl;
        sendDeviceListRequest();
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " -package_config <file> [-rpc_config <file>] [-v]\n"
              << "Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -package_config <file>     Path to package config JSON file (required)\n"
              << "  -rpc_config <file>         Path to RPC config JSON file (optional)\n"
              << "  -v, --verbose              Enable verbose logging (show MONITORING_SNAPSHOT and WATCHDOG_STATS)\n";
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-package_config") {
            if (hasPackageConfig) {
                std::cerr << "Error: Multiple -package_config options specified\n";
                printUsage(argv[0]);
                return 1;
            }
            if (i + 1 < argc) {
                packageConfigPath = argv[++i];
                hasPackageConfig = true;
            } else {
                std::cerr << "Error: -package_config requires a file path argument\n";
                printUsage(argv[0]);
                return 1;
            }
        } else if (arg == "-rpc_config") {
            if (hasRpcConfig) {
                std::cerr << "Error: Multiple -rpc_config options specified\n";
                printUsage(argv[0]);
                return 1;
            }
            if (i + 1 < argc) {
                rpcConfigPath = argv[++i];
                hasRpcConfig = true;
            } else {
                std::cerr << "Error: -rpc_config requires a file path argument\n";
                printUsage(argv[0]);
                return 1;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            verboseMode = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            std::cerr << "Error: Unexpected argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (!hasPackageConfig) {
        std::cerr << "Error: -package_config is required\n";
        printUsage(argv[0]);
        return 1;
    }
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        monitoringConfig = loadMonitoringConfig(packageConfigPath);
        failureDetectionConfig = loadFailureDetectionConfig(packageConfigPath);
        std::cout << "Monitoring config loaded: " << monitoringConfig.toStyledString() << std::endl;
        std::cout << "Failure detection config loaded: " << failureDetectionConfig.toStyledString() << std::endl;
        
        // Initialize ThreadManager
        g_thread_manager = new ThreadMgr::ThreadManager(4); // Support up to 4 threads
        
        // Initialize device manager
        g_device_manager = std::make_unique<DeviceWatchdogManager>();
        g_device_manager->setVerbose(verboseMode);
        std::cout << "Device manager initialized" << std::endl;
        
        // Initialize RPC client if config is provided
        if (hasRpcConfig) {
            std::cout << "Initializing RPC client with config: " << rpcConfigPath << std::endl;
            
            // Create RPC client and operation processor
            g_rpcClient = std::make_shared<RpcClient>(rpcConfigPath, "ur-qmi-watchdog");
            g_operationProcessor = std::make_unique<RpcOperationProcessor>(true); // Enable verbose logging
            
            // Set message handler BEFORE starting the client
            g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
                std::cout << "[RPC] Received message on topic: " << topic << std::endl;
                
                // Handle heartbeat from ur-qmi-ident (startup cron job trigger)
                if (topic.find("clients/ur-qmi-ident/heartbeat") != std::string::npos) {
                    handleHeartbeat(payload);
                    return;
                }
                
                // Handle device list response from ur-qmi-ident
                if (topic.find("direct_messaging/ur-qmi-ident/responses") != std::string::npos) {
                    Json::Value root;
                    Json::Reader reader;
                    if (reader.parse(payload, root)) {
                        // Check if this is a response to our startup device list request
                        if (root.isMember("id") && root["id"].isString()) {
                            std::string response_id = root["id"].asString();
                            // Check if the ID is a numeric timestamp (device list response)
                            if (!response_id.empty() && std::all_of(response_id.begin(), response_id.end(), ::isdigit)) {
                                std::cout << "[RPC] Handling device list response" << std::endl;
                                handleDeviceListResponse(payload);
                                return;
                            }
                        }
                    }
                }
                
                // Handle device discovery events (for dynamic device addition/removal)
                if (topic.find("direct_messaging/ur-qmi-watchdog/requests") != std::string::npos) {
                    std::cout << "[RPC] Received message on device discovery topic: " << topic << std::endl;
                    std::cout << "[RPC] Message payload: " << payload << std::endl;
                    
                    // Check if this is a device discovery event
                    Json::Value root;
                    Json::Reader reader;
                    if (reader.parse(payload, root)) {
                        if (root.isMember("method") && root["method"].asString() == "device_discovery_event") {
                            std::cout << "[RPC] Handling device discovery event" << std::endl;
                            handleDeviceDiscoveryEvent(payload);
                            return;
                        } else {
                            std::cout << "[RPC] Message is not a device discovery event, method: " 
                                      << (root.isMember("method") ? root["method"].asString() : "none") << std::endl;
                        }
                    } else {
                        std::cout << "[RPC] Failed to parse JSON message" << std::endl;
                    }
                    
                    // Delegate to operation processor for other RPC requests
                    if (g_operationProcessor) {
                        g_operationProcessor->setResponseTopic("direct_messaging/ur-qmi-watchdog/responses");
                        g_operationProcessor->processRequest(payload.c_str(), payload.size());
                    }
                }
            });
            
            // Start RPC client
            if (!g_rpcClient->start()) {
                std::cerr << "Warning: Failed to start RPC client, continuing without RPC functionality" << std::endl;
                g_rpcClient.reset();
                g_operationProcessor.reset();
            } else {
                std::cout << "RPC client started successfully" << std::endl;
                
                // Set RPC client on device manager for publishing data
                if (g_device_manager) {
                    g_device_manager->setRpcClient(g_rpcClient);
                    std::cout << "RPC client configured for device manager data publishing" << std::endl;
                }
            }
        }
        
        // No automatic watchdog startup - wait for device discovery events
        std::cout << "QMI Watchdog started - waiting for device discovery events..." << std::endl;
        if (hasRpcConfig) {
            std::cout << "RPC client is listening for device discovery events on: direct_messaging/ur-qmi-watchdog/requests" << std::endl;
        } else {
            std::cout << "Running in standalone mode - no RPC client configured" << std::endl;
        }
        
        // Main loop - wait for device discovery events or shutdown
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Show monitored devices status periodically
            static int status_counter = 0;
            static int device_check_counter = 0;
            if (++status_counter >= 30) { // Every 30 seconds
                status_counter = 0;
                
                // Show startup status
                if (hasRpcConfig) {
                    std::cout << "[Status] Startup: ";
                    if (!g_startup_request_sent.load()) {
                        std::cout << "Waiting for heartbeat from ur-qmi-ident";
                    } else if (!g_device_list_received.load()) {
                        std::cout << "Device list request sent, waiting for response";
                    } else {
                        std::cout << "Startup completed";
                    }
                    std::cout << std::endl;
                }
                
                // Show monitored devices
                if (g_device_manager) {
                    // Cleanup devices that have stopped monitoring (e.g., due to device removal)
                    g_device_manager->cleanupStoppedDevices();
                    g_device_manager->cleanupUnavailableDevices();
                    
                    auto devices = g_device_manager->getMonitoredDevices();
                    if (!devices.empty()) {
                        std::cout << "[Status] Currently monitoring " << devices.size() << " devices: ";
                        for (const auto& device : devices) {
                            std::cout << device << " ";
                        }
                        std::cout << std::endl;
                    } else if (g_device_list_received.load()) {
                        std::cout << "[Status] No devices currently being monitored (ready for device connections)" << std::endl;
                    }
                }
                
                // Periodic device availability check (every 60 seconds)
                if (++device_check_counter >= 60) {
                    device_check_counter = 0;
                    
                    if (g_device_manager && hasRpcConfig && g_device_list_received.load()) {
                        // Check if we should request a fresh device list
                        auto devices = g_device_manager->getMonitoredDevices();
                        if (devices.empty()) {
                            std::cout << "[DeviceCheck] No devices monitored, requesting fresh device list..." << std::endl;
                            
                            // Reset device list received flag to trigger a new request
                            g_device_list_received.store(false);
                            g_startup_request_sent.store(false);
                            
                            // Trigger immediate device list request on next heartbeat
                            std::cout << "[DeviceCheck] Will request device list on next heartbeat" << std::endl;
                        }
                    }
                }
            }
        }
        
        // Cleanup
        std::cout << "Cleaning up resources..." << std::endl;
        
        // Stop device manager first
        if (g_device_manager) {
            std::cout << "Stopping device manager..." << std::endl;
            g_device_manager->stopAllMonitoring();
            g_device_manager.reset();
        }
        
        // Stop RPC client
        if (g_rpcClient) {
            std::cout << "Stopping RPC client..." << std::endl;
            g_rpcClient->stop();
            g_rpcClient.reset();
            g_operationProcessor.reset();
        }
        
        delete g_thread_manager;
        g_thread_manager = nullptr;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        if (g_thread_manager) {
            delete g_thread_manager;
        }
        return 1;
    }
    
    return 0;
}
