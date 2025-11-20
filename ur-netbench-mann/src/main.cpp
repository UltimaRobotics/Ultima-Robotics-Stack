#include "../include/ConfigManager.hpp"
#include "../include/OperationWorker.hpp"
#include "../include/CLIUtils.hpp"
#include "../include/RpcClient.hpp"
#include "../include/RpcOperationProcessor.hpp"
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "../thirdparty/nlohmann/json.hpp"
#include "../thirdparty/ur-rpc-template/extensions/direct_template.h"
#include <iostream>
#include <getopt.h>
#include <cstring>
#include <atomic>
#include <memory>
#include <thread>

using json = nlohmann::json;
using namespace ThreadMgr;

std::atomic<bool> g_interrupted(false);
std::unique_ptr<ThreadManager> g_thread_manager;
std::atomic<bool> g_running(true);

// Global RPC components for signal handling
std::shared_ptr<RpcClient> g_rpcClient;
std::unique_ptr<RpcOperationProcessor> g_operationProcessor;

int main(int argc, char* argv[]) {
    setup_signal_handlers();

    std::string package_config_file;
    std::string rpc_config_file;
    bool rpc_mode = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-package_config") {
            if (i + 1 < argc) {
                package_config_file = argv[++i];
            } else {
                std::cerr << "Error: -package_config requires a file path\n";
                print_usage(argv[0]);
                return 1;
            }
        } else if (arg == "-rpc_config") {
            if (i + 1 < argc) {
                rpc_config_file = argv[++i];
                rpc_mode = true;
            } else {
                std::cerr << "Error: -rpc_config requires a file path\n";
                print_usage(argv[0]);
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate arguments
    if (rpc_mode) {
        if (rpc_config_file.empty()) {
            std::cerr << "Error: -rpc_config is required for RPC mode\n\n";
            print_usage(argv[0]);
            return 1;
        }
    } else {
        if (package_config_file.empty()) {
            std::cerr << "Error: -package_config is required for legacy mode\n\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    try {
        if (rpc_mode) {
            // RPC Mode: Launch RPC client and wait for requests
            std::cout << "========================================" << std::endl;
            std::cout << "Network Benchmark CLI - RPC Mode" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "RPC Config: " << rpc_config_file << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << std::endl;

            // Initialize RPC client
            g_rpcClient = std::make_shared<RpcClient>(rpc_config_file, "ur-netbench-mann");
            
            // Initialize operation processor with a dummy config manager for RPC mode
            ConfigManager dummyConfig;
            g_operationProcessor = std::make_unique<RpcOperationProcessor>(dummyConfig, true);
            
            // Set up message handler for RPC requests
            g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
                // Topic filtering for selective processing
                if (topic.find("direct_messaging/ur-netbench-mann/requests") == std::string::npos) {
                    return;
                }
                
                std::cout << "[RPC] Received request on topic: " << topic << std::endl;
                
                // Set response topic based on request topic
                std::string responseTopic = topic;
                size_t pos = responseTopic.find("/requests");
                if (pos != std::string::npos) {
                    responseTopic.replace(pos, 9, "/responses");
                }
                g_operationProcessor->setResponseTopic(responseTopic);
                g_operationProcessor->setRpcClient(g_rpcClient);
                
                // Delegate to operation processor
                if (g_operationProcessor) {
                    g_operationProcessor->processRequest(payload.c_str(), payload.size());
                }
            });

            // Start RPC client
            if (!g_rpcClient->start()) {
                std::cerr << "Failed to start RPC client" << std::endl;
                return 1;
            }

            std::cout << "[RPC] Client started successfully, waiting for requests..." << std::endl;

            // Main loop with shutdown monitoring
            while (g_running.load() && !g_interrupted.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                // Print statistics every 60 seconds
                static int statsCounter = 0;
                if (++statsCounter >= 60) {
                    direct_client_statistics_t stats;
                    g_rpcClient->getStatistics(&stats);
                    std::cout << "[RPC] Statistics - Messages: " << stats.messages_received 
                              << ", Active threads: " << g_operationProcessor->getActiveThreadsCount() 
                              << ", Connected: " << (stats.is_connected ? "Yes" : "No") << std::endl;
                    statsCounter = 0;
                }
            }

            // Final cleanup
            std::cout << "[RPC] Shutting down..." << std::endl;
            if (g_operationProcessor) {
                g_operationProcessor->shutdown();
            }
            g_rpcClient->stop();
            
            return 0;

        } else {
            // Legacy Mode: Original functionality
            // Use ConfigManager for centralized configuration
            ConfigManager config_manager;
            if (!config_manager.loadPackageConfig(package_config_file)) {
                return 1;
            }
            if (!config_manager.validateConfig()) {
                return 1;
            }

            std::string operation = config_manager.getOperation();
            std::cout << "========================================" << std::endl;
            std::cout << "Network Benchmark CLI - Legacy Mode" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "Operation: " << operation << std::endl;
            std::cout << "Package Config: " << package_config_file << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << std::endl;

            // Create ThreadManager
            g_thread_manager = std::make_unique<ThreadManager>(10);

            // Create and register the operation worker thread
            std::string attachment_id = "operation_worker_thread";
            unsigned int thread_id = g_thread_manager->createThread(
                [](ThreadMgr::ThreadManager& tm, const std::string& config_file) {
                    // This lambda will be executed in the new thread
                    // It should load configuration and perform the actual operation
                    operation_worker(tm, config_file);
                },
                std::ref(*g_thread_manager),
                package_config_file
            );

            std::cout << "[Main] Created operation worker thread with ID: " << thread_id << std::endl;
            g_thread_manager->registerThread(thread_id, attachment_id);

            // Wait for the operation to complete
            std::cout << "[Main] Waiting for operation to complete..." << std::endl;
            // Using a generous timeout or 0 for no timeout if operation_worker is designed to handle interruption
            // The original code had a 300-second timeout for specific test types.
            // For a generic worker, a very large timeout or a mechanism to detect interruption is needed.
            // Let's assume operation_worker will signal completion or interruption.
            // If operation_worker itself handles interruption, joinThread with a large timeout or 0 might be appropriate.
            // For now, mirroring the general structure of waiting for completion.
            bool completed = g_thread_manager->joinThread(thread_id, std::chrono::seconds(300)); // Re-introducing a timeout similar to original for safety.

            std::cout << "\n========================================" << std::endl;
            if (completed) {
                std::cout << "[Main] Operation completed" << std::endl;
            } else {
                std::cout << "[Main] Operation timed out or was interrupted" << std::endl;
                // Attempt to stop the thread if it timed out.
                // The operation_worker should ideally handle external stop signals.
                g_thread_manager->stopThread(thread_id);
            }
            std::cout << "========================================" << std::endl;

            // Return 0 for success, 1 for failure/timeout
            return completed ? 0 : 1;
        }

    } catch (const ThreadManagerException& e) {
        std::cerr << "[Main] ThreadManager error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[Main] Error: " << e.what() << std::endl;
        return 1;
    }
}