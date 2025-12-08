
#include "../include/OperationWorker.hpp"
#include "../include/ConfigManager.hpp"
#include "../include/ServersStatusMonitor.hpp"
#include "../include/FilterUtils.hpp"
#include "../include/TestWorkers.hpp"
#include "../thirdparty/ur-threadder-api/cpp/include/ThreadManager.hpp"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <memory>

using json = nlohmann::json;
using namespace ThreadMgr;

void operation_worker(ThreadMgr::ThreadManager& tm, const std::string& package_config_file) {
    try {
        // Use ConfigManager for centralized configuration
        ConfigManager config_manager;

        if (!config_manager.loadPackageConfig(package_config_file)) {
            std::cerr << "[OperationWorker] Failed to load package config" << std::endl;
            return;
        }

        if (!config_manager.validateConfig()) {
            std::cerr << "[OperationWorker] Config validation failed" << std::endl;
            return;
        }

        std::string operation = config_manager.getOperation();

        std::cout << "========================================" << std::endl;
        std::cout << "Network Benchmark CLI" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Operation: " << operation << std::endl;
        std::cout << "Package Config: " << package_config_file << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;

        if (operation == "servers-status") {
            std::string servers_list_path = config_manager.getServersListPath();
            std::string output_dir = config_manager.getOutputDir();

            std::ifstream servers_file(servers_list_path);
            if (!servers_file.is_open()) {
                std::cerr << "[OperationWorker] Error: Could not open servers list file: " << servers_list_path << std::endl;
                return;
            }

            json servers_list;
            try {
                servers_file >> servers_list;
                servers_file.close();
            } catch (const std::exception& e) {
                std::cerr << "[OperationWorker] Error: Invalid JSON in servers list file: " << e.what() << std::endl;
                return;
            }

            json filters = config_manager.getFilters();
            if (!filters.empty()) {
                std::cout << "[OperationWorker] Applying filters..." << std::endl;
                servers_list = filter_servers(servers_list, filters);
                std::cout << "[OperationWorker] Filtered to " << servers_list.size() << " servers" << std::endl;
            }

            if (servers_list.empty()) {
                std::cerr << "[OperationWorker] Error: No servers match the filters" << std::endl;
                return;
            }

            std::string temp_servers_file = "/tmp/filtered_servers.json";
            std::ofstream temp_file(temp_servers_file);
            if (!temp_file.is_open()) {
                std::cerr << "[OperationWorker] Error: Could not create temporary filtered servers file" << std::endl;
                return;
            }
            temp_file << servers_list.dump(2);
            temp_file.close();

            std::cout << "[OperationWorker] Starting Servers Status Monitoring..." << std::endl;

            ServersStatusMonitor monitor(output_dir);

            if (!monitor.loadServersConfig(temp_servers_file)) {
                std::cerr << "[OperationWorker] Error: Failed to load servers configuration" << std::endl;
                return;
            }

            if (!monitor.startMonitoring()) {
                std::cerr << "[OperationWorker] Error: Failed to start monitoring" << std::endl;
                return;
            }

            std::cout << "\n[OperationWorker] Monitoring started...\n" << std::endl;

            monitor.displayContinuousStatus(1);

            monitor.stopMonitoring();

            std::cout << "\n[OperationWorker] Monitoring stopped." << std::endl;
            
            // Export aggregated results
            std::string output_file = config_manager.getOutputFile();
            if (output_file.empty()) {
                output_file = output_dir + "/aggregated_server_status.json";
            }
            monitor.exportAggregatedResults(output_file);

            std::remove(temp_servers_file.c_str());

        } else if (operation == "dns" || operation == "traceroute" || operation == "ping" || operation == "iperf") {
            unsigned int thread_id = 0;
            std::string attachment_id;
            std::string output_file = config_manager.getOutputFile();
            json test_config = config_manager.getTestConfig();

            std::cout << "[OperationWorker] Test config extracted from ConfigManager:" << std::endl;
            std::cout << test_config.dump(2) << std::endl;

            // For iperf, add servers_list_path to config if needed for auto-collection
            if (operation == "iperf") {
                bool need_port = !test_config.contains("port") || test_config["port"].get<int>() == 0;
                bool need_options = !test_config.contains("options") || test_config["options"].get<std::string>().empty();
                bool has_servers_list = test_config.contains("use_servers_list") && test_config["use_servers_list"].get<bool>();
                bool has_hostname = test_config.contains("server_hostname") && !test_config["server_hostname"].get<std::string>().empty();

                // Add servers_list_path if it will be needed for auto-collection
                if ((need_port || need_options) && (has_servers_list || (has_hostname && need_port))) {
                    std::string servers_list_path = config_manager.getServersListPath();
                    if (!servers_list_path.empty()) {
                        test_config["servers_list_path"] = servers_list_path;
                        std::cout << "[OperationWorker] Added servers_list_path to config: " << servers_list_path << std::endl;
                    } else {
                        std::cerr << "[OperationWorker] WARNING: servers_list_path not found in package config, auto-collection may fail" << std::endl;
                    }
                }
            }

            std::cout << "[OperationWorker] Final test config being passed to thread:" << std::endl;
            std::cout << test_config.dump(2) << std::endl;

            if (operation == "dns") {
                attachment_id = "dns_test_thread";
                thread_id = tm.createThread([](ThreadMgr::ThreadManager& tm_inner, const json& cfg, const std::string& out) {
                    dns_test_worker(tm_inner, cfg, out);
                }, std::ref(tm), test_config, output_file);
                std::cout << "[OperationWorker] Created DNS test thread with ID: " << thread_id << std::endl;
                tm.registerThread(thread_id, attachment_id);
            } else if (operation == "traceroute") {
                attachment_id = "traceroute_test_thread";
                thread_id = tm.createThread([](ThreadMgr::ThreadManager& tm_inner, const json& cfg, const std::string& out) {
                    traceroute_test_worker(tm_inner, cfg, out);
                }, std::ref(tm), test_config, output_file);
                std::cout << "[OperationWorker] Created Traceroute test thread with ID: " << thread_id << std::endl;
                tm.registerThread(thread_id, attachment_id);
            } else if (operation == "ping") {
                attachment_id = "ping_test_thread";
                thread_id = tm.createThread([](ThreadMgr::ThreadManager& tm_inner, const json& cfg, const std::string& out) {
                    ping_test_worker(tm_inner, cfg, out);
                }, std::ref(tm), test_config, output_file);
                std::cout << "[OperationWorker] Created Ping test thread with ID: " << thread_id << std::endl;
                tm.registerThread(thread_id, attachment_id);
            } else if (operation == "iperf") {
                attachment_id = "iperf_test_thread";
                thread_id = tm.createThread([](ThreadMgr::ThreadManager& tm_inner, const json& cfg, const std::string& out) {
                    iperf_test_worker(tm_inner, cfg, out);
                }, std::ref(tm), test_config, output_file);
                std::cout << "[OperationWorker] Created Iperf test thread with ID: " << thread_id << std::endl;
                tm.registerThread(thread_id, attachment_id);
            }

            std::cout << "[OperationWorker] Waiting for test to complete..." << std::endl;

            bool completed = tm.joinThread(thread_id, std::chrono::seconds(300));

            std::cout << "\n========================================" << std::endl;
            if (completed) {
                std::cout << "[OperationWorker] Test completed successfully" << std::endl;
            } else {
                std::cout << "[OperationWorker] Test timed out" << std::endl;
                tm.stopThread(thread_id);
            }
            std::cout << "========================================" << std::endl;

        } else {
            std::cerr << "[OperationWorker] Error: Unknown operation '" << operation << "'" << std::endl;
            std::cerr << "[OperationWorker] Supported operations: servers-status, dns, traceroute, ping, iperf" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[OperationWorker] Error: " << e.what() << std::endl;
    }
}
