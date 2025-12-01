#include "qmi_scanner.h"
#include "rpc_client.hpp"
#include "rpc_operation_processor.hpp"
#include <iostream>
#include <signal.h>
#include <chrono>
#include <thread>
#include <string>
#include <atomic>

#include <vector>
#include <nlohmann/json.hpp>
#include <qmi_device_registry.h>


static QMIScanner* g_scanner = nullptr;
static std::atomic<bool> g_running(true);

// Global RPC client and operation processor for signal handling
std::shared_ptr<RpcClient> g_rpcClient;
std::unique_ptr<RpcOperationProcessor> g_operationProcessor;


static std::atomic<int> message_counter(0);


void signalHandler(int signal) {
    std::cout << "Shutting down system..." << std::endl;
    g_running = false;
    
    // Stop RPC client first
    if (g_rpcClient) {
        g_rpcClient->stop();
    }
    
    if (g_scanner) {
        g_scanner->stopMonitoring();
    }
    exit(-1);
}


void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n"
              << "Options:\n"
              << "  -basic          Run in basic mode (default)\n"
              << "  -advanced       Run in advanced mode\n"
              << "  -manager        Run in manager mode\n"
              << "  --rpc-config    Enable RPC mode with specified config file\n"
              << "  -h, --help      Show this help message\n"
              << "\n"
              << "Example:\n"
              << "  " << programName << " -manager\n"
              << "  " << programName << " -advanced\n"
              << "  " << programName << " -basic\n"
              << "  " << programName << " --rpc-config ur-rpc-config.json\n"
              << "  " << programName << " -manager --rpc-config ur-rpc-config.json\n";
}

void ScannerThread(ProfileMode* mode, std::shared_ptr<RpcClient> rpcClient) {
    std::cout << "Starting QMI Device Scanner in " 
              << (*mode == ProfileMode::BASIC ? "BASIC" : 
                  *mode == ProfileMode::ADVANCED ? "ADVANCED" : "MANAGER") 
              << " mode..." << std::endl;

    QMIScanner scanner;
    g_scanner = &scanner;
    
    // Set RPC client for publishing device discovery events
    scanner.setRpcClient(rpcClient);

    if (*mode == ProfileMode::BASIC) {
        scanner.setProfileCallback([](const DeviceProfile& profile, bool added) {
            std::cerr << "Device " << (added ? "added" : "removed") << ": " 
                      << profile.path << " (IMEI: " << profile.imei << ")" << std::endl;
        });
    } else if (*mode == ProfileMode::ADVANCED) {
        scanner.setAdvancedProfileCallback([](const AdvancedDeviceProfile& profile, bool added) {
            std::cerr << "Device " << (added ? "added" : "removed") << ": " 
                      << profile.basic.path << " (IMEI: " << profile.basic.imei << ")" << std::endl;
        });
    } else if (*mode == ProfileMode::MANAGER) {
        scanner.setDeviceCallback([&scanner](const QMIDevice& device, bool added) {
            std::cerr << "Device " << (added ? "added" : "removed") << ": " 
                      << device.device_path << " (IMEI: " << device.imei << ")" << std::endl;
            
            std::string device_json = scanner.generateDeviceWithSimStatusJson(device, true);
            std::cout << device_json << std::endl;
        });
    }

    if (!scanner.initialize(*mode)) {
        std::cerr << "Failed to initialize scanner" << std::endl;
        g_running = false;
        return;
    }

    scanner.startMonitoring();

    std::cout << "Scanner initialized and monitoring started. Press Ctrl+C to stop." << std::endl;
    std::vector<QMIDevice> devices = scanner.getCurrentDevices();
    if (!devices.empty()) {
        std::string json_validation = scanner.validateAndExtractSIMJson(
            scanner.generateDevicesArrayWithSimStatusJson(devices, false)
        );
    }
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    scanner.stopMonitoring();
    g_scanner = nullptr;
    std::cout << "Scanner thread finished." << std::endl;
}


int main(int argc, char* argv[]) {
    ProfileMode mode = ProfileMode::BASIC; 
    bool modeSpecified = false;
    std::string rpc_config_path;
    bool rpc_mode = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-basic") {
            if (modeSpecified) {
                std::cerr << "Error: Multiple mode options specified. Use only one of -basic, -advanced, or -manager" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            mode = ProfileMode::BASIC;
            modeSpecified = true;
        } 
        else if (arg == "-advanced") {
            if (modeSpecified) {
                std::cerr << "Error: Multiple mode options specified. Use only one of -basic, -advanced, or -manager" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            mode = ProfileMode::ADVANCED;
            modeSpecified = true;
        } 
        else if (arg == "-manager") {
            if (modeSpecified) {
                std::cerr << "Error: Multiple mode options specified. Use only one of -basic, -advanced, or -manager" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            mode = ProfileMode::MANAGER;
            modeSpecified = true;
        } 
        else if (arg == "--rpc-config") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --rpc-config requires a configuration file path" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            rpc_config_path = argv[++i];
            rpc_mode = true;
        }
        else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } 
        else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        } 
        else {
            std::cerr << "Error: Unexpected argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (!modeSpecified && !rpc_mode) {
        std::cout << "Warning: No mode specified, using default: BASIC" << std::endl;
    }
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Initialize RPC client if RPC mode is enabled
        if (rpc_mode) {
            std::cout << "\n[Main] Starting RPC mode..." << std::endl;
            std::cout << "[Main] RPC config: " << rpc_config_path << std::endl;
            
            // Initialize RPC client
            g_rpcClient = std::make_shared<RpcClient>(rpc_config_path, "ur-qmi-ident");
            
            // Initialize operation processor
            g_operationProcessor = std::make_unique<RpcOperationProcessor>(true);
            
            // CRITICAL: Set message handler BEFORE starting the client
            std::cout << "[Main] Setting up message handler..." << std::endl;
            g_rpcClient->setMessageHandler(
                [&](const std::string &topic, const std::string &payload) {
                    // Only process messages on the request topic
                    if (topic.find("direct_messaging/ur-qmi-ident/requests") == std::string::npos) return;
                    g_operationProcessor->processRequest(payload.c_str(), payload.size());
                });

            std::cout << "[Main] Starting RPC client..." << std::endl;
            bool rpcStarted = g_rpcClient->start();
            if (!rpcStarted) {
                std::cerr << "[Main] Failed to start RPC client, continuing with scanner only..." << std::endl;
                // Don't return error, continue with scanner but without RPC publishing
            } else {
                std::cout << "[Main] RPC client started successfully" << std::endl;
                std::cout << "[Main] Responding on: direct_messaging/ur-qmi-ident/responses"
                  << std::endl;
            }
            std::cout
                << "[Main] Listening on: direct_messaging/ur-qmi-ident/requests"
                << std::endl;
            std::cout
                << "[Main] Responding on: direct_messaging/ur-qmi-ident/responses"
                << std::endl;
        }
        
        // Start scanner in parallel with RPC or standalone
        if (rpc_mode || modeSpecified) {
            std::cout << "\n[Main] Starting QMI device identification..." << std::endl;
            
            // Create scanner thread
            std::thread scanner_thread(ScannerThread, &mode, g_rpcClient);
            
            // Set scanner reference for RPC operations
            if (rpc_mode && g_operationProcessor) {
                // Wait a moment for scanner to initialize
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                g_operationProcessor->setScanner(g_scanner);
            }
            
            // Main loop - keep running until signal received
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            scanner_thread.join();
        } else {
            // Default behavior - just run scanner
            std::cout << "\n[Main] Starting QMI device identification in BASIC mode..." << std::endl;
            
            // Create scanner thread (no RPC client for standalone mode)
            std::thread scanner_thread(ScannerThread, &mode, nullptr);
            
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            scanner_thread.join();
        }
        
        // Graceful shutdown for RPC mode
        if (rpc_mode && g_rpcClient) {
            std::cout << "[Main] Shutting down RPC client..." << std::endl;
            g_rpcClient->stop();
        }
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "[Main] Application stopped" << std::endl;
    return 0;
}
