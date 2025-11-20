
#include "../include/MAVLinkCollector.hpp"
#include <iostream>
#include <fstream>
#include <csignal>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include "../nholmann/json.hpp"

#ifdef HTTP_ENABLED
#include "HttpClient.hpp"
#endif

// Global variables for signal handling
std::unique_ptr<MAVLinkCollector> g_collector;
std::atomic<bool> g_running{true};
std::atomic<bool> g_collector_started{false};

void signalHandler(int signum) {
    std::cout << "\n[Main] Interrupt signal (" << signum << ") received." << std::endl;
    g_running = false;

    if (g_collector) {
        g_collector->printMessageStats();
        g_collector->stop();
    }

    exit(signum);
}

#ifdef HTTP_ENABLED
void httpStatusMonitorLoop(MAVLinkCollector* collector, const std::string& serverAddress, 
                            int serverPort, int timeoutMs, int fetchIntervalMs) {
    HttpClient::HttpConfig httpConfig;
    httpConfig.serverAddress = serverAddress;
    httpConfig.serverPort = serverPort;
    httpConfig.timeoutMs = timeoutMs;
    
    HttpClient::Client client(httpConfig);
    
    std::cout << "[Main] HTTP status monitor started" << std::endl;
    std::cout << "[Main] Polling /api/threads/mainloop every " << fetchIntervalMs << "ms" << std::endl;
    
    while (g_running) {
        try {
            // GET request to check mainloop thread status
            auto response = client.get("/api/threads/mainloop");
            
            if (response.success && response.statusCode == 200) {
                // Parse JSON response
                try {
                    auto json_response = nlohmann::json::parse(response.body);
                    
                    if (json_response.contains("threads") && 
                        json_response["threads"].contains("mainloop")) {
                        
                        bool isAlive = json_response["threads"]["mainloop"].value("isAlive", false);
                        
                        if (isAlive && !g_collector_started) {
                            std::cout << "[Main] Mainloop is alive - Starting Collector" << std::endl;
                            if (collector->start()) {
                                g_collector_started = true;
                                std::cout << "[Main] Collector started successfully" << std::endl;
                            } else {
                                std::cerr << "[Main] Failed to start Collector" << std::endl;
                            }
                        } else if (!isAlive && g_collector_started) {
                            std::cout << "[Main] Mainloop is not alive - Stopping Collector" << std::endl;
                            collector->stop();
                            g_collector_started = false;
                            std::cout << "[Main] Collector stopped successfully" << std::endl;
                        }
                    }
                } catch (const nlohmann::json::exception& e) {
                    std::cerr << "[Main] JSON parse error: " << e.what() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[Main] HTTP request error: " << e.what() << std::endl;
        }
        
        // Sleep for the configured interval
        std::this_thread::sleep_for(std::chrono::milliseconds(fetchIntervalMs));
    }
    
    std::cout << "[Main] HTTP status monitor stopped" << std::endl;
}
#endif

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string config_file = "collector_config.json";
    if (argc > 1) {
        config_file = argv[1];
    }

    std::cout << "[Main] MAVLink Collector starting..." << std::endl;
    std::cout << "[Main] Using config file: " << config_file << std::endl;

    try {
        g_collector = std::make_unique<MAVLinkCollector>(config_file);

#ifdef HTTP_ENABLED
        // Check if HTTP control is enabled
        std::ifstream config_stream(config_file);
        nlohmann::json config;
        config_stream >> config;
        
        bool httpEnabled = config.value("enableHTTP", false);
        
        if (httpEnabled) {
            std::cout << "[Main] HTTP status monitoring enabled" << std::endl;
            
            auto httpCfg = config["httpConfig"];
            std::string serverAddress = httpCfg.value("serverAddress", "0.0.0.0");
            int serverPort = httpCfg.value("serverPort", 5000);
            int timeoutMs = httpCfg.value("timeoutMs", 5000);
            int fetchIntervalMs = httpCfg.value("fetch_status_interval_ms", 1000);
            
            // Start HTTP status monitor thread
            std::thread httpThread(httpStatusMonitorLoop, g_collector.get(), 
                                  serverAddress, serverPort, timeoutMs, fetchIntervalMs);
            
            // Wait for signals
            while (g_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            httpThread.join();
        } else {
#endif
            // Normal operation without HTTP control
            std::cout << "[Main] Starting collector directly (HTTP control disabled)" << std::endl;
            
            if (!g_collector->start()) {
                std::cerr << "[Main] Failed to start collector" << std::endl;
                return 1;
            }

            // Keep running until interrupted
            while (g_running && g_collector->isRunning()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            g_collector->stop();
#ifdef HTTP_ENABLED
        }
#endif

    } catch (const std::exception& e) {
        std::cerr << "[Main] Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[Main] MAVLink Collector stopped" << std::endl;
    return 0;
}

