#include "qmi_scanner.h"
#include <iostream>
#include <signal.h>
#include <chrono>
#include <thread>
#include <string>
#include <gateway.hpp>
#include <user-level.h>

#include <vector>
#include <nlohmann/json.hpp>
#include <qmi_device_registry.h>


static QMIScanner* g_scanner = nullptr;
static bool running = true;

using namespace DirectTemplate;

static std::atomic<int> message_counter(0);

using namespace ThreadMgr;
ThreadManager manager(5);

void signalHandler(int signal) {
    std::cout << "Shutting down system..." << std::endl;
    g_running = false;
    if (g_scanner) {
        g_scanner->stopMonitoring();
    }
    exit(-1);
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

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS] <file_path>\n"
              << "Options:\n"
              << "  -basic          Run in basic mode (default)\n"
              << "  -advanced       Run in advanced mode\n"
              << "  -manager        Run in manager mode\n"
              << "  -rpc_config <file>  Specify RPC configuration file (required)\n"
              << "  -h, --help      Show this help message\n"
              << "\n"
              << "Example:\n"
              << "  " << programName << " -manager -rpc_config config.json data_file.txt\n"
              << "  " << programName << " -advanced -rpc_config /path/to/config.json /path/to/data_file\n";
}

void ScannerThread(ProfileMode* mode) {
    std::cout << "Starting QMI Device Scanner in " 
              << (*mode == ProfileMode::BASIC ? "BASIC" : 
                  *mode == ProfileMode::ADVANCED ? "ADVANCED" : "MANAGER") 
              << " mode..." << std::endl;

    QMIScanner scanner;
    g_scanner = &scanner;

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

void TargetedRPCResponder::handleRequestMessage(const std::string& topic, const std::string& payload) {
    Utils::logInfo("Responder Handling Process...");
    TargetedRequestData request = TargetedRequestParser::parseTargetedRequest(payload);
    if (request.method.find("qmi-stack-module-startup")!=s td::string::npos){
        if (g_responder) {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            Utils::logInfo("Response Process ...");            
            g_responder->sendResponse(request.response_topic,request.transaction_id,request.method,true,g_scanner->getRegistryJson() ,now);
        }
    }
    return ;
}

void handleIncomingMessage(const std::string& topic, const std::string& payload){
    Utils::logInfo("Handling Cureent message... ");
    handleTargetedMessage(topic, payload, g_requester.get(), g_responder.get());
}

int main(int argc, char* argv[]) {
    std::string rpcConfigPath;
    ProfileMode mode = ProfileMode::BASIC; 
    bool hasRpcConfig = false;
    bool modeSpecified = false;

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
        else if (arg == "-rpc_config") {
            if (hasRpcConfig) {
                std::cerr << "Error: Multiple -rpc_config options specified" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            if (i + 1 < argc) {
                rpcConfigPath = argv[++i];
                hasRpcConfig = true;
            } else {
                std::cerr << "Error: -rpc_config requires a file path argument" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
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
    if (!hasRpcConfig) {
        std::cerr << "Error: -rpc_config is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    if (!modeSpecified) {
        std::cout << "Warning: No mode specified, using default: BASIC" << std::endl;
    }
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    try {
        ThreadManager::setLogLevel(LogLevel::Info);        
        std::cout << "\n1. Creating identification thread ..." << std::endl;
        auto qmi_lookfor_rpc = manager.createThread(RpcClientThread, rpcConfigPath);
        auto qmi_lookfor_id = manager.createThread<void(*)(ProfileMode*), ProfileMode*>(ScannerThread, &mode);
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
