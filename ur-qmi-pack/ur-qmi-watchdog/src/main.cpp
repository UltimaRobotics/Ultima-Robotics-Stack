
#include "qmi_watchdog.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <string>
#include <atomic>
#include <json/json.h>

QMIWatchdog* g_watchdog = nullptr;
static std::atomic<bool> g_running(true);
std::string packageConfigPath;
bool hasPackageConfig = false;

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
    std::cout << "Usage: " << programName << " -package_config <file>\n"
              << "Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -package_config <file>     Path to package config JSON file (required)\n";
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
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
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
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        monitoringConfig = loadMonitoringConfig(packageConfigPath);
        failureDetectionConfig = loadFailureDetectionConfig(packageConfigPath);
        std::cout << "Monitoring config loaded: " << monitoringConfig.toStyledString() << std::endl;
        std::cout << "Failure detection config loaded: " << failureDetectionConfig.toStyledString() << std::endl;
        
        // Create watchdog thread
        std::thread watchdog_thread(watchdogThreadFunction, &packageConfigPath);
        
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        watchdog_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
