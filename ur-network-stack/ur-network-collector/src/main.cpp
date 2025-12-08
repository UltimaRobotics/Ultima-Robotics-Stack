#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <getopt.h>
#include <signal.h>
#include <thread>
#include <atomic>

#include "NetworkCollectorThread.h"
#include "RpcClient.h"
#include "RpcOperationProcessor.h"

// Note: vector and fstream are included in NetworkCollectorThread.h/implementation
// iomanip is used for timestamp formatting in NetworkCollectorThread

// Global flag for graceful shutdown
static std::atomic<bool> g_running(true);
static std::unique_ptr<NetworkCollectorThread> g_collectorThread;
static std::shared_ptr<RpcClient> g_rpcClient;
static std::unique_ptr<RpcOperationProcessor> g_operationProcessor;

void signalHandler(int signal) {
    std::cout << "\n[Main] Caught signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
    exit(0);
    // Stop RPC client first
    if (g_rpcClient) {
        g_rpcClient->stop();
    }
    
    // Stop network collector thread
    if (g_collectorThread) {
        g_collectorThread->stop();
    }
    
    // Shutdown operation processor
    if (g_operationProcessor) {
        g_operationProcessor->shutdown();
    }
    
    exit(0);
}

void printUsage(const char* programName) {
    std::cout << "Network Data Collector Utility (with RPC Support)\n";
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help          Show this help message\n";
    std::cout << "  -v, --verbose       Enable verbose output\n";
    std::cout << "  --rpc_config FILE   Enable RPC client with JSON configuration file\n";
    std::cout << "  --rpc_client_id ID  Set RPC client ID (default: ur-network-collector)\n\n";
    std::cout << "Runtime Behavior:\n";
    std::cout << "  • Standalone mode: Collect all network data once in JSON format\n";
    std::cout << "  • RPC mode: RPC client starts first, waits for connection,\n";
    std::cout << "    then launches continuous collection with 1-second intervals\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << "                          # Collect all data once (JSON)\n";
    std::cout << "  " << programName << " -v                        # Collect all data with verbose output\n";
    std::cout << "  " << programName << " --rpc_config rpc.json     # Start RPC client with continuous collection\n";
    std::cout << "  " << programName << " -v --rpc_config rpc.json  # RPC mode with verbose logging\n\n";
    std::cout << "Note: In standalone mode, always collects all network data in JSON format.\n";
    std::cout << "      In RPC mode, collection runs continuously with 1-second intervals.\n";
    std::cout << "      Use Ctrl+C to stop continuous collection or RPC client.\n";
}


int main(int argc, char* argv[]) {
    bool verboseMode = false;
    std::string rpcConfigFile;
    std::string rpcClientId = "ur-network-collector";
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"rpc_config", required_argument, 0, 1000},
        {"rpc_client_id", required_argument, 0, 1001},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 'v':
                verboseMode = true;
                break;
            case 1000:  // --rpc_config
                rpcConfigFile = optarg;
                break;
            case 1001:  // --rpc_client_id
                rpcClientId = optarg;
                break;
            case '?':
                std::cerr << "Unknown option. Use -h for help." << std::endl;
                return 1;
            default:
                break;
        }
    }
    
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        bool rpcEnabled = !rpcConfigFile.empty();
        
        if (rpcEnabled) {
            if (verboseMode) {
                std::cout << "[Main] Initializing RPC client as primary service..." << std::endl;
            }
            
            // Initialize RPC client first
            g_rpcClient = std::make_shared<RpcClient>(rpcConfigFile, rpcClientId);
            
            // Initialize operation processor with default configuration (collect all)
            NetworkCollectorConfig rpcConfig;
            rpcConfig.collectVlan = true;
            rpcConfig.collectNat = true;
            rpcConfig.collectFirewall = true;
            rpcConfig.collectRoutes = true;
            rpcConfig.collectBridges = true;
            rpcConfig.collectAll = true;
            rpcConfig.outputText = false;
            rpcConfig.quietMode = !verboseMode;
            rpcConfig.outputFile = "";
            rpcConfig.collectionInterval = 1;  // Force 1-second interval
            rpcConfig.enableMqttPublishing = true;  // Enable MQTT publishing in RPC mode
            rpcConfig.mqttRuntimeTopic = "ur-shared-bus/ur-network-stack/ur-net-collector/runtime";
            
            g_operationProcessor = std::make_unique<RpcOperationProcessor>(rpcConfig, verboseMode);
            g_operationProcessor->setRpcClient(g_rpcClient);
            
            // Set message handler BEFORE starting the client
            g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
                // Topic filtering for selective processing
                if (topic.find("direct_messaging/" + rpcClientId + "/requests") == std::string::npos) {
                    return;
                }
                
                if (verboseMode) {
                    std::cout << "[Main] Received RPC request on topic: " << topic << std::endl;
                }
                
                // Delegate to operation processor
                if (g_operationProcessor) {
                    g_operationProcessor->processRequest(payload.c_str(), payload.size());
                }
            });
            
            // Set response topic for operation processor
            std::string responseTopic = "direct_messaging/" + rpcClientId + "/responses";
            g_operationProcessor->setResponseTopic(responseTopic);
            
            // Start RPC client - handler must be set first
            std::cout << "[Main] Starting RPC client..." << std::endl;
            if (!g_rpcClient->start()) {
                std::cerr << "[Main] Failed to start RPC client" << std::endl;
                return 1;
            }

            // Wait for RPC client to initialize (like ur-licence-mann)
            std::this_thread::sleep_for(std::chrono::seconds(2));

            if (!g_rpcClient->isRunning()) {
                std::cerr << "[Main] RPC client failed to start" << std::endl;
                return 1;
            }

            std::cout << "[Main] RPC client is running and ready to process requests" << std::endl;
            
            // Now launch the collection thread with automatic continuous mode and 1-second interval
            g_collectorThread = std::make_unique<NetworkCollectorThread>();
            
            CollectionConfig config;
            config.collectVlan = true;
            config.collectNat = true;
            config.collectFirewall = true;
            config.collectRoutes = true;
            config.collectBridges = true;
            config.collectAll = true;
            config.outputText = false;
            config.quietMode = !verboseMode;
            config.outputFile = "";
            config.collectionInterval = 1;  // Force 1-second interval for collection
            config.publishingInterval = 1;  // Publish every 1 second for maximum freshness
            config.continuousMode = true;   // Force continuous mode
            config.enableMqttPublishing = true;  // Enable MQTT publishing
            config.mqttRuntimeTopic = "ur-shared-bus/ur-network-stack/ur-net-collector/runtime";
            
            if (verboseMode) {
                std::cout << "[Main] Starting network collector with automatic continuous mode (1-second interval)" << std::endl;
                std::cout << "[Main] Collection will run continuously until RPC shutdown or Ctrl+C" << std::endl;
                std::cout << "[Main] WARNING: Publishing every 1 second may overload MQTT broker" << std::endl;
            }
            
            // Start the collector thread
            if (!g_collectorThread->start(config)) {
                std::cerr << "[Main] Failed to start network collector thread" << std::endl;
                g_rpcClient->stop();
                return 1;
            }
            
            if (verboseMode) {
                std::cout << "[Main] Network collector started successfully" << std::endl;
                std::cout << "[Main] System ready - RPC client and collection thread running" << std::endl;
            }
            
            // Main loop - keep running until signal received
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
        } else {
            // No RPC mode - run single collection with verbose output
            if (verboseMode) {
                std::cout << "[Main] Running in standalone mode - collecting all network data once" << std::endl;
            }
            
            // Create and configure the collector thread
            g_collectorThread = std::make_unique<NetworkCollectorThread>();
            
            CollectionConfig config;
            config.collectVlan = true;
            config.collectNat = true;
            config.collectFirewall = true;
            config.collectRoutes = true;
            config.collectBridges = true;
            config.collectAll = true;
            config.outputText = false;  // Always JSON output
            config.quietMode = !verboseMode;
            config.outputFile = "";
            config.collectionInterval = 1;
            config.publishingInterval = 30; // Default publishing interval
            config.continuousMode = false;  // Single collection in standalone mode
            config.enableMqttPublishing = false;  // Disable MQTT in standalone mode
            config.mqttRuntimeTopic = "ur-shared-bus/ur-network-stack/ur-net-collector/runtime";
            
            // Start the collector thread
            if (!g_collectorThread->start(config)) {
                std::cerr << "[Main] Failed to start network collector thread" << std::endl;
                return 1;
            }
            
            // Wait for single collection to complete
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            // Get and display the collected data
            std::string data = g_collectorThread->getLastCollectedData();
            if (!data.empty()) {
                if (verboseMode) {
                    std::cout << "[Main] Collected network data:" << std::endl;
                }
                std::cout << data << std::endl;
            } else {
                std::cerr << "[Main] No data collected" << std::endl;
            }
            
            // Stop the thread
            g_collectorThread->stop();
        }
        
        if (verboseMode) {
            std::cout << "[Main] Shutting down..." << std::endl;
        }
        
        // Cleanup
        if (g_collectorThread) {
            g_collectorThread->stop();
        }
        
        if (rpcEnabled) {
            if (g_operationProcessor) {
                g_operationProcessor->shutdown();
            }
            if (g_rpcClient) {
                g_rpcClient->stop();
            }
            if (verboseMode) {
                std::cout << "[Main] RPC client stopped" << std::endl;
            }
        }
        
        if (verboseMode) {
            std::cout << "[Main] Network collector stopped" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[Main] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
