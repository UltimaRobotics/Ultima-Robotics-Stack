
#include "qmi_watchdog.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <gateway.hpp>
#include <ThreadManager.hpp>
#include <user-level.h>

using namespace ThreadMgr;
using namespace DirectTemplate;

QMIWatchdog* g_watchdog = nullptr;
static std::atomic<int> message_counter(0);
ThreadManager manager(5);
std::string packageConfigPath;
std::string rpcConfigPath;
bool hasPackageConfig = false;
bool hasRpcConfig = false;

Json::Value monitoringConfig;
Json::Value failureDetectionConfig;

bool ValidatedStartupInstance_0 = false;

void signalHandler(int signal) {
    std::cout << "Shutting down system..." << std::endl;
    g_running = false;
    if (g_watchdog) {
        g_watchdog->stopMonitoring();
    }
    exit(0);
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

void watchdogThreadFunction(std::string* refconfig) {
    
    QMIWatchdog watchdog;
    g_watchdog = &watchdog;
    
    
    if (!watchdog.loadDeviceConfig(*refconfig)) {
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
    while (watchdog.isMonitoring()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " -package_config <file> -rpc_config <file>\n"
              << "Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -package_config <file>     Path to package config JSON file (required)\n"
              << "  -rpc_config <file>         Path to RPC client config JSON file (required)\n";
}

bool TargetedRPCResponder::processRequest(const std::string& method, const std::string& payload) {
    std::string data_payload = TargetedRequestParser::extractDataPayload(payload);
    TargetedRequestData request = TargetedRequestParser::parseTargetedRequest(payload);
    if (TargetedRequestParser::verifyQMIDeviceFormat(data_payload)) {
        QMIDeviceData device_data = TargetedRequestParser::parseQMIDeviceData(data_payload);
        
        Utils::logInfo("JSON parsed successfully");
        if (manager.getThreadCount()>1){
            try {
                auto foundId = manager.findThreadByAttachment(device_data.device_path);
                auto state = manager.getThreadState(foundId);
                std::cout << "Thread " << foundId << " is in state: " << static_cast<int>(state) << std::endl;
                if((thread_state_t)state != THREAD_RUNNING and device_data.action == "added"){
                    Json::Value root; Json::Value profiles; Json::Value basic; 
                    basic["path"] = device_data.device_path;
                    basic["imei"] = device_data.imei;
                    basic["model"] = device_data.model;
                    basic["manufacturer"] = device_data.manufacturer;
                    profiles["basic"] = basic;
                    root["profiles"] = profiles;
                    root["monitoring_config"] = monitoringConfig;
                    root["failure_detection"] = failureDetectionConfig;

                    Json::StreamWriterBuilder writer;
                    writer["indentation"] = "  "; // pretty-print with 2 spaces
                    std::string placeholder_ss = Json::writeString(writer, root);
                    auto placeholder = manager.createThread(watchdogThreadFunction,&placeholder_ss);
                    manager.registerThread(placeholder, device_data.device_path);
                }else if ((thread_state_t)state == THREAD_RUNNING and device_data.action == "removed"){
                    manager.stopThreadByAttachment(device_data.device_path);
                }
            }catch (const ThreadManagerException& e) {
                std::cout << "Error finding thread: " << e.what() << std::endl;
            }
        }
    }else {
        Utils::logInfo("JSON parsing failed");
        return false;
    }
    return true;
}

void PerformStartUpRequests(std::string RefTopic){
    if (RefTopic == "clients/ur-qmi-ident/heartbeat"){
        try {
            while (!GlobalClientThraedRef || !GlobalClientThraedRef->isConnected()) {
                std::cerr << "Target Thread process Warning: Client Thread not connected, cannot send device data" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (!g_running.load()) return ;
            }
            g_requester = std::make_unique<TargetedRPCRequester>(GlobalClientThraedRef);
            if(!g_requester) {
                Utils::logError("TargetedRPCRequester is not initialized, cannot send startup request");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            Utils::logInfo("Requester Ready for Startup process");
            Json::Value StartupRequest;
            StartupRequest["NullData"] = "NullData";
            Json::StreamWriterBuilder writer;
            std::string request_data = Json::writeString(writer, StartupRequest);
            g_requester->sendTargetedRequest("ur-qmi-ident", "qmi-stack-module-startup-ValidatedStartupInstance_0", request_data, 
                [](bool success, const std::string& result, const std::string& error_message, int error_code) {
                if (success) {
                    Utils::logInfo("live device resuested successfully: " + result);
                } else {
                    Utils::logError("Failed to request live devices: " + error_message);
                }
            });        
            Utils::logInfo("Startup request sent to ur-qmi-ident");
        } catch (const DirectTemplateException& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return ;
        }
    }
}

void handleIncomingMessage(const std::string& topic, const std::string& payload){
    if ((!ValidatedStartupInstance_0) && (topic.find("ValidatedStartupInstance")!= std::string::npos)){
        ValidatedStartupInstance_0 = true;
        GlobalClientThraedRef->unsubscribeTopic(topic);
        GlobalClientThraedRef->unsubscribeTopic("clients/ur-qmi-ident/heartbeat");
    }else if (!ValidatedStartupInstance_0 && topic.find("clients/ur-qmi-ident/heartbeat")!= std::string::npos){
        PerformStartUpRequests(topic);
    }else {
        handleTargetedMessage(topic, payload, g_requester.get(), g_responder.get());
    }
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
        } else if (!arg.empty() && arg[0] == '-') {
            continue;
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
    if (!hasRpcConfig) {
        std::cerr << "Error: -rpc_config is required\n";
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
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return 1;
    }

    try {
        ThreadManager::setLogLevel(LogLevel::Info);        
        std::cout << "\n1. Creating identification thread ..." << std::endl;
        auto qmi_watchdog_rpc = manager.createThread(RpcClientThread, rpcConfigPath);
        manager.registerThread(qmi_watchdog_rpc, "qmi_watchdog_rpc");
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            threads_monitor_lookfor();
        }
    }
    catch (const ThreadManagerException& e) {
        std::cerr << "ThreadManager error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
