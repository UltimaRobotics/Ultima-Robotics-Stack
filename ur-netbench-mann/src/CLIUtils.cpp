
#include "../include/CLIUtils.hpp"
#include "../include/RpcClient.hpp"
#include "../include/RpcOperationProcessor.hpp"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <memory>
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"

extern std::atomic<bool> g_interrupted;
extern std::unique_ptr<ThreadMgr::ThreadManager> g_thread_manager;
extern std::atomic<bool> g_running;

// Global RPC components for signal handling
extern std::shared_ptr<RpcClient> g_rpcClient;
extern std::unique_ptr<RpcOperationProcessor> g_operationProcessor;

void signal_handler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\n[Signal] Caught Ctrl+C (SIGINT), exiting gracefully..." << std::endl;
        g_interrupted.store(true);
        g_running.store(false);
        exit(0);
        
        // Stop RPC client if running
        if (g_rpcClient) {
            std::cout << "[Signal] Stopping RPC client..." << std::endl;
            g_rpcClient->stop();
        }
        
        // Shutdown operation processor if running
        if (g_operationProcessor) {
            std::cout << "[Signal] Shutting down operation processor..." << std::endl;
            g_operationProcessor->shutdown();
        }
        
        // Don't exit immediately, let main loop handle cleanup
    }
}

void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        std::cerr << "Warning: Failed to setup SIGINT handler" << std::endl;
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Network Benchmark CLI supports two modes:\n\n"
              << "LEGACY MODE:\n"
              << "  -package_config FILE    Main configuration file for legacy mode\n\n"
              << "RPC MODE:\n"
              << "  -rpc_config FILE        RPC configuration file for RPC server mode\n\n"
              << "Common Options:\n"
              << "  -h, --help              Show this help message\n\n"
              << "Examples:\n"
              << "  " << program_name << " -package_config config.json          # Run in legacy mode\n"
              << "  " << program_name << " -rpc_config rpc-config.json          # Run as RPC server\n\n"
              << "Legacy Mode Configuration file format:\n"
              << "{\n"
              << "  \"operation\": \"servers-status\",\n"
              << "  \"servers_list_path\": \"path/to/servers.json\",\n"
              << "  \"filters\": {\n"
              << "    \"keyword\": \"optional keyword to search in all fields\",\n"
              << "    \"continent\": \"optional continent filter\",\n"
              << "    \"country\": \"optional country filter\",\n"
              << "    \"site\": \"optional site filter\",\n"
              << "    \"provider\": \"optional provider filter\",\n"
              << "    \"host\": \"optional host/IP filter\",\n"
              << "    \"port\": 5201,\n"
              << "    \"min_speed\": \"10\",\n"
              << "    \"options\": \"-R\"\n"
              << "  },\n"
              << "  \"output_dir\": \"runtime-data/server-status\"\n"
              << "}\n\n"
              << "RPC Mode Configuration file format:\n"
              << "{\n"
              << "  \"client_id\": \"ur-netbench-mann\",\n"
              << "  \"broker_host\": \"127.0.0.1\",\n"
              << "  \"broker_port\": 1899,\n"
              << "  \"keepalive\": 60,\n"
              << "  \"qos\": 0,\n"
              << "  \"auto_reconnect\": true,\n"
              << "  \"reconnect_delay_min\": 1,\n"
              << "  \"reconnect_delay_max\": 60,\n"
              << "  \"use_tls\": false,\n"
              << "  \"heartbeat\": {\n"
              << "    \"enabled\": true,\n"
              << "    \"interval_seconds\": 5,\n"
              << "    \"topic\": \"clients/ur-netbench-mann/heartbeat\",\n"
              << "    \"payload\": \"{\\\"client\\\":\\\"ur-netbench-mann\\\",\\\"status\\\":\\\"alive\\\"}\"\n"
              << "  },\n"
              << "  \"json_added_pubs\": {\n"
              << "    \"topics\": [\n"
              << "      \"direct_messaging/ur-netbench-mann/responses\"\n"
              << "    ]\n"
              << "  },\n"
              << "  \"json_added_subs\": {\n"
              << "    \"topics\": [\n"
              << "      \"direct_messaging/ur-netbench-mann/requests\"\n"
              << "    ]\n"
              << "  }\n"
              << "}\n\n"
              << "RPC Operations:\n"
              << "  - servers-status: Check status of servers from list\n"
              << "  - ping-test: Perform ping test to target\n"
              << "  - traceroute-test: Perform traceroute to target\n"
              << "  - iperf-test: Perform iperf bandwidth test\n"
              << "  - dns-lookup: Perform DNS lookup\n"
              << std::endl;
}
