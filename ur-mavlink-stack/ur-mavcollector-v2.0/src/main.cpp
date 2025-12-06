#include "MavlinkUdpConnection.h"
#include "Vehicle.h"
#include "BoardIdentifier.h"
#include "PackageConfig.h"
#include "MavlinkCollectorThread.h"
#include "rpc_client.hpp"
#include "rpc_operation_processor.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <unistd.h>  // For _exit
#include "../thirdparty/nlohmann/json.hpp"

// Include ur-threadder-api headers
extern "C" {
    #include "../thirdparty/ur-threadder-api/include/thread_manager.h"
}
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"

using json = nlohmann::json;

// External variables
extern std::atomic<bool> g_running;
extern bool verbose_mode;

// Global RPC client and operation processor for signal handling
std::shared_ptr<RpcClient> g_rpcClient;
std::unique_ptr<RpcOperationProcessor> g_operationProcessor;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    
    // Set global shutdown flags
    g_running = false;
    extern std::atomic<bool> g_collector_running;
    g_collector_running = false;
    
    // Also stop publisher thread
    extern std::atomic<bool> g_publisher_running;
    g_publisher_running = false;
    
    // Don't exit immediately - let main loop handle graceful shutdown
    std::cout << "Shutdown flags set, waiting for main loop to exit..." << std::endl;
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " --pkg_config <config_file_path> -rpc_client <rpc_config_file_path>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --pkg_config        Path to package configuration JSON file" << std::endl;
    std::cout << "  -rpc_client         Path to RPC configuration JSON file (required)" << std::endl;
    std::cout << "  -h, --help          Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file_path;
    std::string rpc_config_file_path;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--pkg_config" && i + 1 < argc) {
            config_file_path = argv[++i];
        } else if (arg == "-rpc_client" && i + 1 < argc) {
            rpc_config_file_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Validate required arguments
    if (config_file_path.empty()) {
        std::cerr << "Error: --pkg_config is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    if (rpc_config_file_path.empty()) {
        std::cerr << "Error: -rpc_client is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Load configuration from file
    PackageConfig config;
    if (!config.loadFromFile(config_file_path)) {
        std::cerr << "Failed to load configuration from: " << config_file_path << std::endl;
        return 1;
    }
    
    // Set global verbose mode
    verbose_mode = config.verbose;
    
    if (verbose_mode) {
        std::cout << "Loaded configuration from: " << config_file_path << std::endl;
        config.print();
    }
    
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Initialize RPC client and processor (RPC mode is now the only mode)
    std::cout << "RPC config: " << rpc_config_file_path << std::endl;
    
    // Initialize RPC client
    g_rpcClient = std::make_shared<RpcClient>(rpc_config_file_path, "ur-mavcollector-v2.0");
    
    // Initialize operation processor
    g_operationProcessor = std::make_unique<RpcOperationProcessor>(config, verbose_mode);
    
    // CRITICAL: Set message handler BEFORE starting the client
    std::cout << "Setting up RPC message handler..." << std::endl;
    g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
        if (verbose_mode) {
            std::cout << "RPC handler received message on topic: " << topic << std::endl;
        }
        
        // Handle discovery responses from ur-mavdiscovery
        if (topic.find("direct_messaging/ur-mavdiscovery/responses") != std::string::npos) {
            if (g_operationProcessor) {
                g_operationProcessor->handleDiscoveryResponse(topic, payload);
            }
            return;
        }
        
        // Handle collector responses (for other RPC operations)
        if (topic.find("direct_messaging/ur-mavcollector/responses") != std::string::npos) {
            if (verbose_mode) {
                std::cout << "Received collector response on topic: " << topic << std::endl;
            }
            return; // Collector responses are handled elsewhere
        }
        
        // Only process messages on the request topic
        if (topic.find("direct_messaging/ur-mavcollector/requests") == std::string::npos) {
            return;
        }   
        
        // Process the request using operation processor
        if (g_operationProcessor) {
            g_operationProcessor->processRequest(payload.c_str(), payload.size());
        }
    });
    std::cout << "RPC message handler configured successfully" << std::endl;

    
    // Start RPC client - handler must be set first
    std::cout << "Starting RPC client..." << std::endl;
    if (!g_rpcClient->start()) {
        std::cerr << "Failed to start RPC client" << std::endl;
        return 1;
    }
    
    // Wait for RPC client to initialize
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (!g_rpcClient->isRunning()) {
        std::cerr << "RPC client failed to start" << std::endl;
        return 1;
    }
    
    std::cout << "RPC client is running and ready to process requests" << std::endl;
    std::cout << "Listening on: direct_messaging/ur-mavcollector/requests" << std::endl;
    std::cout << "Responding on: direct_messaging/ur-mavcollector/responses" << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    
    // Main loop - keep running until signal received
    std::cout << "Entering main event loop..." << std::endl;
    int loop_count = 0;
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Print status every 10 seconds to show the application is responsive
        if (++loop_count % 100 == 0) {
            std::cout << "Main loop running... (" << (loop_count / 10) << " seconds)" << std::endl;
        }
    }
    
    std::cout << "Main loop exiting, performing graceful shutdown..." << std::endl;
    
    // Graceful shutdown
    std::cout << "Shutting down RPC client..." << std::endl;
    g_rpcClient->stop();
    
    std::cout << "Application stopped" << std::endl;
    return 0;
}
