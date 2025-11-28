#include "vpn_instance_manager.hpp"
#include "vpn_rpc_client.hpp"
#include "vpn_rpc_operation_processor.hpp"
#ifdef HTTP_ENABLED
#include "http_server.hpp"
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <csignal>
#include <vector>
#include <memory>
#include <getopt.h>

// Include logger API
extern "C" {
#include "../ur-rpc-template/deps/ur-logger-api/logger.h"
}

std::atomic<bool> g_running(true);

// Global pointers for signal handler access
vpn_manager::VPNInstanceManager* g_manager = nullptr;
#ifdef HTTP_ENABLED
http_server::HTTPServer* g_http_server = nullptr;
std::thread* g_http_thread = nullptr;
#endif
std::string g_cache_file_path;
// RPC client global pointers
std::unique_ptr<VpnRpcClient> g_rpcClient = nullptr;
std::unique_ptr<vpn_manager::VpnRpcOperationProcessor> g_rpcProcessor = nullptr;

// Force cleanup WireGuard interfaces without relying on threads
void forceCleanupWireGuardInterfaces() {
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "WIREGUARD_FORCE_CLEANUP_START"},
        {"message", "Scanning and cleaning WireGuard interfaces independently"}
    }).dump() << std::endl;


    // Read /proc/net/dev to find WireGuard interfaces
    std::vector<std::string> wg_interfaces;
    std::ifstream dev_file("/proc/net/dev");
    if (dev_file.is_open()) {
        std::string line;
        // Skip header lines
        std::getline(dev_file, line);
        std::getline(dev_file, line);

        while (std::getline(dev_file, line)) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string iface = line.substr(0, colon_pos);
                // Trim whitespace
                iface.erase(0, iface.find_first_not_of(" \t"));
                iface.erase(iface.find_last_not_of(" \t") + 1);

                // Check if it's a WireGuard interface (wg* or wiga*)
                if (iface.find("wg") == 0 || iface.find("wiga") == 0) {
                    wg_interfaces.push_back(iface);
                }
            }
        }
        dev_file.close();
    }

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "WIREGUARD_INTERFACES_FOUND"},
        {"count", wg_interfaces.size()},
        {"interfaces", wg_interfaces}
    }).dump() << std::endl;


    // Force cleanup each WireGuard interface
    for (const auto& iface : wg_interfaces) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "CLEANUP_WG_INTERFACE"},
            {"interface", iface},
            {"message", "Forcing cleanup of WireGuard interface"}
        }).dump() << std::endl;


        // Remove all routes
        std::string flush_routes_cmd = "ip route flush dev " + iface + " 2>/dev/null || true";
        system(flush_routes_cmd.c_str());

        // Remove DNS configuration
        std::string remove_dns_cmd = "resolvconf -d " + iface + " 2>/dev/null || true";
        system(remove_dns_cmd.c_str());

        // Bring interface down
        std::string down_cmd = "ip link set dev " + iface + " down 2>/dev/null || true";
        system(down_cmd.c_str());

        // Delete interface
        std::string delete_cmd = "ip link del dev " + iface + " 2>/dev/null || true";
        system(delete_cmd.c_str());

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "WG_INTERFACE_CLEANED"},
            {"interface", iface}
        }).dump() << std::endl;

    }

    // Kill any remaining WireGuard processes (wg-quick, wireguard, etc.)
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "KILL_WG_PROCESSES"},
        {"message", "Terminating WireGuard-related processes"}
    }).dump() << std::endl;


    system("killall -9 wg-quick 2>/dev/null || true");
    system("killall -9 wireguard 2>/dev/null || true");

    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "WIREGUARD_FORCE_CLEANUP_COMPLETE"},
        {"message", "WireGuard force cleanup completed"}
    }).dump() << std::endl;

}


void signalHandler(int signal) {

    std::cout << "Performing Graceful Exit on Interrup" << std::endl;
    exit(0);
    std::cout << json({
        {"type", "signal"},
        {"signal", signal},
        {"message", "Signal received, initiating graceful shutdown"}
    }).dump() << std::endl;


    // Set running flag to false to exit main loop
    g_running = false;

    // ===== COMPREHENSIVE GRACEFUL SHUTDOWN SEQUENCE =====
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "SHUTDOWN_START"},
        {"message", "Signal handler: beginning comprehensive shutdown sequence"}
    }).dump() << std::endl;


    // Force cleanup WireGuard interfaces first (independent of thread management)
    forceCleanupWireGuardInterfaces();

    // Step 2: Stop RPC client first (prevents new operations)
    if (g_rpcClient) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "RPC_CLIENT_STOP_START"},
            {"message", "Stopping RPC client to prevent new operations"}
        }).dump() << std::endl;

        g_rpcClient->stop();

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "RPC_CLIENT_STOP_COMPLETE"},
            {"message", "RPC client stopped"}
        }).dump() << std::endl;
    }

    // Step 3: Stop HTTP server
#ifdef HTTP_ENABLED
    if (g_http_server) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "HTTP_SERVER_STOP_START"},
            {"message", "Stopping HTTP server to prevent new operations"}
        }).dump() << std::endl;


        g_http_server->stop();

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "HTTP_SERVER_STOP_COMPLETE"},
            {"message", "HTTP server stopped"}
        }).dump() << std::endl;

    }
#endif

    // Step 4: Track and stop all VPN instances
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "TRACK_INSTANCES_START"},
        {"message", "Tracking all active VPN instances for graceful shutdown"}
    }).dump() << std::endl;


    // Step 5: Execute comprehensive VPN instance shutdown
    if (g_manager) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "VPN_INSTANCES_STOP_START"},
            {"message", "Stopping all VPN instances with comprehensive cleanup (same as HTTP stop)"}
        }).dump() << std::endl;


        g_manager->stopAll();

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "VPN_INSTANCES_STOP_COMPLETE"},
            {"message", "All VPN instances stopped with comprehensive cleanup"}
        }).dump() << std::endl;

    }

    // Step 6: Join HTTP thread if it was started
#ifdef HTTP_ENABLED
    if (g_http_thread && g_http_thread->joinable()) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "HTTP_THREAD_JOIN_START"},
            {"message", "Joining HTTP server thread"}
        }).dump() << std::endl;


        g_http_thread->join();

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "HTTP_THREAD_JOIN_COMPLETE"},
            {"message", "HTTP server thread joined"}
        }).dump() << std::endl;

    }
#endif

    // Step 7: Save cached data
    if (g_manager && !g_cache_file_path.empty()) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "CACHE_SAVE_START"},
            {"message", "Saving cached data to disk"},
            {"cache_file", g_cache_file_path}
        }).dump() << std::endl;


        g_manager->saveCachedData(g_cache_file_path);

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "CACHE_SAVE_COMPLETE"},
            {"message", "Cached data saved successfully"}
        }).dump() << std::endl;

    }

    // Step 8: Delete HTTP server object
#ifdef HTTP_ENABLED
    if (g_http_server) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "HTTP_SERVER_DELETE_START"},
            {"message", "Deleting HTTP server object"}
        }).dump() << std::endl;


        delete g_http_server;
        g_http_server = nullptr;

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "HTTP_SERVER_DELETE_COMPLETE"},
            {"message", "HTTP server object deleted"}
        }).dump() << std::endl;

    }
#endif

    // Step 9: Cleanup RPC client and processor
    if (g_rpcProcessor) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "RPC_PROCESSOR_DELETE_START"},
            {"message", "Cleaning up RPC operation processor"}
        }).dump() << std::endl;

        g_rpcProcessor.reset();

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "RPC_PROCESSOR_DELETE_COMPLETE"},
            {"message", "RPC operation processor cleaned up"}
        }).dump() << std::endl;
    }

    if (g_rpcClient) {
        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "RPC_CLIENT_DELETE_START"},
            {"message", "Cleaning up RPC client"}
        }).dump() << std::endl;

        g_rpcClient.reset();

        std::cout << json({
            {"type", "shutdown_verbose"},
            {"step", "RPC_CLIENT_DELETE_COMPLETE"},
            {"message", "RPC client cleaned up"}
        }).dump() << std::endl;
    }

    // Step 10: Final shutdown message
    std::cout << json({
        {"type", "shutdown_verbose"},
        {"step", "SHUTDOWN_COMPLETE"},
        {"message", "All resources cleaned up, all threads stopped, all wrappers disconnected"}
    }).dump() << std::endl;


    std::cout << json({
        {"type", "shutdown"},
        {"message", "VPN Instance Manager stopped cleanly - exiting"}
    }).dump() << std::endl;


    // Now it's safe to exit - everything is cleaned up
    exit(0);
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "OPTIONS:" << std::endl;
    std::cout << "  -h, --help              Show this help message" << std::endl;
    std::cout << "  -pkg_config <file>      Path to master configuration JSON file" << std::endl;
    std::cout << "  -rpc_config <file>      Path to RPC configuration JSON file" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string master_config_file;
    std::string rpc_config_file;
    
    // Manual argument parsing to avoid getopt_long issue
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-pkg_config" || arg == "--pkg_config") {
            if (i + 1 < argc) {
                master_config_file = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a file path argument" << std::endl;
                return 1;
            }
        } else if (arg == "-rpc_config" || arg == "--rpc_config") {
            if (i + 1 < argc) {
                rpc_config_file = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a file path argument" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << ". Use -h for help." << std::endl;
            return 1;
        }
    }
    
    // Check for required arguments
    if (master_config_file.empty()) {
        std::cerr << "Missing master configuration file. Use -pkg_config <file>. Use -h for help." << std::endl;
        return 1;
    }

    std::ifstream master_file(master_config_file);
    if (!master_file.good()) {
        json error = {
            {"type", "error"},
            {"message", "Master config file not found"},
            {"file", master_config_file}
        };
        std::cout << error.dump() << std::endl;
        return 1;
    }

    // Parse master config
    json master_config;
    try {
        master_file >> master_config;
        master_file.close();
    } catch (const std::exception& e) {
        json error = {
            {"type", "error"},
            {"message", "Failed to parse master config file"},
            {"error", e.what()}
        };
        std::cout << error.dump() << std::endl;
        return 1;
    }

    // Extract paths from master config
    if (!master_config.contains("config_file_path")) {
        json error = {
            {"type", "error"},
            {"message", "Master config missing 'config_file_path' field"}
        };
        std::cout << error.dump() << std::endl;
        return 1;
    }

    std::string config_file = master_config["config_file_path"].get<std::string>();
    std::string cache_file = master_config.value("cached_data_path", "");

    // Validate and create config file if missing or corrupted
    {
        std::ifstream test_config(config_file);
        bool config_valid = false;

        if (test_config.good()) {
            try {
                json test_json;
                test_config >> test_json;
                test_config.close();

                // Validate structure
                if (test_json.contains("vpn_profiles") && test_json["vpn_profiles"].is_array()) {
                    config_valid = true;
                } else {
                    json warning = {
                        {"type", "warning"},
                        {"message", "Config file has invalid structure, recreating with empty data"},
                        {"file", config_file}
                    };
                    std::cout << warning.dump() << std::endl;
                }
            } catch (const std::exception& e) {
                json warning = {
                    {"type", "warning"},
                    {"message", "Config file is corrupted, recreating with empty data"},
                    {"file", config_file},
                    {"error", e.what()}
                };
                std::cout << warning.dump() << std::endl;
                test_config.close();
            }
        } else {
            json warning = {
                {"type", "warning"},
                {"message", "Config file not found, creating with empty data"},
                {"file", config_file}
            };
            std::cout << warning.dump() << std::endl;
        }

        if (!config_valid) {
            // Create empty config file
            json empty_config;
            empty_config["vpn_profiles"] = json::array();

            std::ofstream config_out(config_file);
            config_out << empty_config.dump(2);
            config_out.close();

            json info = {
                {"type", "info"},
                {"message", "Created empty config file"},
                {"file", config_file}
            };
            std::cout << info.dump() << std::endl;
        }
    }

    // Validate and create cache file if missing or corrupted
    if (!cache_file.empty()) {
        std::ifstream test_cache(cache_file);
        bool cache_valid = false;

        if (test_cache.good()) {
            try {
                json test_json;
                test_cache >> test_json;
                test_cache.close();

                // Validate structure
                if (test_json.contains("instances") && test_json["instances"].is_array()) {
                    cache_valid = true;
                } else {
                    json warning = {
                        {"type", "warning"},
                        {"message", "Cache file has invalid structure, recreating with empty data"},
                        {"file", cache_file}
                    };
                    std::cout << warning.dump() << std::endl;
                }
            } catch (const std::exception& e) {
                json warning = {
                    {"type", "warning"},
                    {"message", "Cache file is corrupted, recreating with empty data"},
                    {"file", cache_file},
                    {"error", e.what()}
                };
                std::cout << warning.dump() << std::endl;
                test_cache.close();
            }
        } else {
            json warning = {
                {"type", "warning"},
                {"message", "Cache file not found, creating with empty data"},
                {"file", cache_file}
            };
            std::cout << warning.dump() << std::endl;
        }

        if (!cache_valid) {
            // Create empty cache file
            json empty_cache;
            empty_cache["instances"] = json::array();
            empty_cache["last_saved"] = time(nullptr);

            std::ofstream cache_out(cache_file);
            cache_out << empty_cache.dump(2);
            cache_out.close();

            json info = {
                {"type", "info"},
                {"message", "Created empty cache file"},
                {"file", cache_file}
            };
            std::cout << info.dump() << std::endl;
        }
    }

    // Parse routing rules file path
    std::string routing_rules_file = master_config.value("custom_routing_rules", "");
    
    // Parse HTTP server configuration
    bool http_enabled = false;
    std::string http_host = "0.0.0.0";
    int http_port = 8080;

    if (master_config.contains("http_server")) {
        auto http_config = master_config["http_server"];
        http_enabled = http_config.value("enabled", false);
        http_host = http_config.value("host", "0.0.0.0");
        http_port = http_config.value("port", 8080);
    }

    // Parse verbose mode
    bool verbose_mode = master_config.value("verbose", false);

    // Parse stats logging configuration
    bool stats_logging_enabled = true;
    bool openvpn_stats_logging = true;
    bool wireguard_stats_logging = true;
    
    if (master_config.contains("stats_logging")) {
        auto stats_config = master_config["stats_logging"];
        stats_logging_enabled = stats_config.value("enabled", true);
        openvpn_stats_logging = stats_config.value("openvpn", true);
        wireguard_stats_logging = stats_config.value("wireguard", true);
    }

    // Parse logging configuration
    bool logging_enabled = true;
    bool source_logging[10] = {true, true, true, true, true, true, true, true, true, true};
    
    if (master_config.contains("logging")) {
        auto logging_config = master_config["logging"];
        logging_enabled = logging_config.value("enabled", true);
        
        if (logging_config.contains("sources")) {
            auto sources = logging_config["sources"];
            source_logging[0] = sources.value("unknown", true);                    // LOG_SOURCE_UNKNOWN
            source_logging[1] = sources.value("ur_rpc_template", true);            // LOG_SOURCE_UR_RPC_TEMPLATE
            source_logging[2] = sources.value("thread_manager", true);            // LOG_SOURCE_THREAD_MANAGER
            source_logging[3] = sources.value("vpn_manager", true);               // LOG_SOURCE_VPN_MANAGER
            source_logging[4] = sources.value("openvpn_library", true);           // LOG_SOURCE_OPENVPN_LIBRARY
            source_logging[5] = sources.value("wireguard_library", true);         // LOG_SOURCE_WIREGUARD_LIBRARY
            source_logging[6] = sources.value("http_server", true);               // LOG_SOURCE_HTTP_SERVER
            source_logging[7] = sources.value("rpc_client", true);                // LOG_SOURCE_RPC_CLIENT
            source_logging[8] = sources.value("rpc_processor", true);             // LOG_SOURCE_RPC_PROCESSOR
            source_logging[9] = sources.value("external_binary", true);           // LOG_SOURCE_EXTERNAL_BINARY
        }
    }

    // Parse RPC configuration
    std::string rpc_config_path = rpc_config_file;
    // No longer reading from master config, using command line argument directly
    
    bool rpc_enabled = !rpc_config_path.empty();
    
    // Validate RPC config file if provided
    if (rpc_enabled) {
        std::ifstream rpc_file(rpc_config_path);
        if (!rpc_file.good()) {
            json error = {
                {"type", "error"},
                {"message", "RPC config file not found"},
                {"file", rpc_config_path}
            };
            std::cout << error.dump() << std::endl;
            return 1;
        }
    }
    
    if (verbose_mode) {
        json rpc_info = {
            {"type", "rpc_config"},
            {"enabled", rpc_enabled},
            {"config_file", rpc_config_path}
        };
        std::cout << rpc_info.dump() << std::endl;
    }

    // Register signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Initialize VPN Instance Manager
    vpn_manager::VPNInstanceManager manager;
    g_manager = &manager;

    // Store cache file path for signal handler
    g_cache_file_path = cache_file;

    // Set verbose mode
    manager.setVerbose(verbose_mode);
    if (verbose_mode) {
        json verbose_info = {
            {"type", "verbose"},
            {"message", "Verbose mode enabled"}
        };
        std::cout << verbose_info.dump() << std::endl;
    }

    // Set stats logging configuration
    manager.setStatsLoggingEnabled(stats_logging_enabled);
    manager.setOpenVPNStatsLogging(openvpn_stats_logging);
    manager.setWireGuardStatsLogging(wireguard_stats_logging);
    
    // Configure logging sources
    logger_configure_sources(logging_enabled, source_logging);
    
    if (verbose_mode) {
        json stats_info = {
            {"type", "verbose"},
            {"message", "Stats logging configuration"},
            {"data", {
                {"stats_logging_enabled", stats_logging_enabled},
                {"openvpn_stats_logging", openvpn_stats_logging},
                {"wireguard_stats_logging", wireguard_stats_logging}
            }}
        };
        std::cout << stats_info.dump() << std::endl;
        
        json logging_info = {
            {"type", "verbose"},
            {"message", "Source logging configuration"},
            {"data", {
                {"logging_enabled", logging_enabled},
                {"vpn_manager", source_logging[3]},
                {"openvpn_library", source_logging[4]},
                {"wireguard_library", source_logging[5]}
            }}
        };
        std::cout << logging_info.dump() << std::endl;
    }

    // Set global event callback to print JSON events
    manager.setGlobalEventCallback([](const vpn_manager::AggregatedEvent& event) {
        json event_json = {
            {"instance", event.instance_name},
            {"type", event.event_type},
            {"message", event.message},
            {"timestamp", event.timestamp}
        };

        if (!event.data.empty()) {
            event_json["data"] = event.data;
        }

        std::cout << event_json.dump() << std::endl;
    });

    // Load configuration
    if (!manager.loadConfigurationFromFile(config_file, cache_file)) {
        json error = {
            {"type", "error"},
            {"message", "Failed to load configuration"}
        };
        std::cout << error.dump() << std::endl;
        return 1;
    }
    
    // Load routing rules if specified
    if (!routing_rules_file.empty()) {
        if (!manager.loadRoutingRules(routing_rules_file)) {
            json warning = {
                {"type", "warning"},
                {"message", "Failed to load routing rules, continuing without them"},
                {"file", routing_rules_file}
            };
            std::cout << warning.dump() << std::endl;
        }
    }

    // Start HTTP server if enabled
#ifdef HTTP_ENABLED
    if (http_enabled) {
        g_http_server = new http_server::HTTPServer(http_host, http_port);
        g_http_server->setVPNManager(&manager);

        static std::thread http_thread_local([](http_server::HTTPServer* server) {
            if (!server->start()) {
                json error = {
                    {"type", "error"},
                    {"message", "Failed to start HTTP server"}
                };
                std::cout << error.dump() << std::endl;
            }
        }, g_http_server);

        g_http_thread = &http_thread_local;

        json http_startup = {
            {"type", "http_server"},
            {"message", "HTTP server thread started"},
            {"host", http_host},
            {"port", http_port}
        };
        std::cout << http_startup.dump() << std::endl;
    } else {
        json http_disabled = {
            {"type", "http_server"},
            {"message", "HTTP server is disabled in configuration"}
        };
        std::cout << http_disabled.dump() << std::endl;
    }
#else
    if (http_enabled) {
        json http_warning = {
            {"type", "warning"},
            {"message", "HTTP server requested but not compiled. Rebuild with -DHTTP_ENABLED=ON"}
        };
        std::cout << http_warning.dump() << std::endl;
    }
#endif

    // Initialize RPC client if enabled
    if (rpc_enabled) {
        try {
            // Initialize RPC client
            g_rpcClient = std::make_unique<VpnRpcClient>(rpc_config_path, "ur-vpn-manager");

            // Initialize operation processor
            g_rpcProcessor = std::make_unique<vpn_manager::VpnRpcOperationProcessor>(manager, *g_rpcClient, verbose_mode);
            
            // Set response topic for the processor
            g_rpcProcessor->setResponseTopic("direct_messaging/ur-vpn-manager/responses");

            // CRITICAL: Set message handler BEFORE starting the client
            std::cout << "[Main] Setting up message handler..." << std::endl;
            g_rpcClient->setMessageHandler([&](const std::string &topic, const std::string &payload) {
                if (verbose_mode) {
                    std::cout << "[Main] Custom handler received message on topic: " << topic << std::endl;
                }
                // Only process messages on the request topic
                if (topic.find("direct_messaging/ur-vpn-manager/requests") == std::string::npos) {
                    return;
                }

                // Process the request using operation processor
                if (g_rpcProcessor) {
                    g_rpcProcessor->processRequest(payload.c_str(), payload.size());
                }
            });
            std::cout << "[Main] Message handler configured successfully" << std::endl;

            // Start RPC client - handler must be set first
            std::cout << "[Main] Starting RPC client..." << std::endl;
            if (!g_rpcClient->start()) {
                std::cerr << "[Main] Failed to start RPC client" << std::endl;
                return 1;
            }

            // Wait for RPC client to initialize
            std::this_thread::sleep_for(std::chrono::seconds(2));

            if (!g_rpcClient->isRunning()) {
                std::cerr << "[Main] RPC client failed to start" << std::endl;
                return 1;
            }

            std::cout << "[Main] RPC client is running and ready to process requests" << std::endl;
            std::cout << "[Main] Listening on: direct_messaging/ur-vpn-manager/requests" << std::endl;
            std::cout << "[Main] Responding on: direct_messaging/ur-vpn-manager/responses" << std::endl;
            std::cout << "[Main] Press Ctrl+C to stop..." << std::endl;

        } catch (const std::exception& e) {
            json error = {
                {"type", "error"},
                {"message", "Failed to initialize RPC client"},
                {"error", e.what()},
                {"config_file", rpc_config_path}
            };
            std::cout << error.dump() << std::endl;
            g_rpcClient.reset();
            g_rpcProcessor.reset();
            rpc_enabled = false;
        }
    } else {
        json rpc_disabled = {
            {"type", "rpc_client"},
            {"message", "RPC client is disabled - no configuration provided"}
        };
        std::cout << rpc_disabled.dump() << std::endl;
    }

    // Start all enabled instances
    json startup = {
        {"type", "startup"},
        {"message", "Starting all enabled VPN instances"}
    };
    std::cout << startup.dump() << std::endl;

    if (!manager.startAllEnabled()) {
        json error = {
            {"type", "error"},
            {"message", "Failed to start instances"}
        };
        std::cout << error.dump() << std::endl;
        return 1;
    }

    // Main monitoring loop
    json loop_start = {
        {"type", "info"},
        {"message", "Entering main monitoring loop - printing status every 10 seconds"}
    };
    if (logger_is_source_enabled(LOG_SOURCE_VPN_MANAGER)) {
        std::cout << loop_start.dump() << std::endl;
    }
    
    while (g_running) {
        // Get aggregated status
        json status = manager.getAllInstancesStatus();

        json status_msg = {
            {"type", "status"},
            {"instances", status},
            {"instance_count", status.size()},
            {"timestamp", time(nullptr)}
        };
        if (logger_is_source_enabled(LOG_SOURCE_VPN_MANAGER)) {
            std::cout << status_msg.dump() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    // If we exit the loop normally (not via signal), cleanup will be done by signalHandler
    // This path should rarely be reached unless g_running is set false programmatically
    std::cout << json({
        {"type", "info"},
        {"message", "Main loop exited normally"}
    }).dump() << std::endl;

    return 0;
}