
#include "DeviceManager.hpp"
#include "Logger.hpp"
#include "ConfigLoader.hpp"
#include <iostream>
#include <unistd.h>

void printHelp(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n\n";
    std::cout << "MAVLink Device Discovery Service\n";
    std::cout << "Discovers and verifies MAVLink devices, providing real-time RPC notifications.\n\n";
    std::cout << "Required Arguments:\n";
    std::cout << "  -rpc_config, --rpc-config FILE     Path to RPC configuration JSON file\n";
    std::cout << "  -package_config, --package-config FILE  Path to package configuration JSON file\n\n";
    std::cout << "Optional Arguments:\n";
    std::cout << "  -h, --help                          Display this help message and exit\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " -rpc_config rpc-config.json -package_config config.json\n";
    std::cout << "  " << programName << " --rpc-config /etc/mavdiscovery/rpc.json --package-config /etc/mavdiscovery/config.json\n\n";
    std::cout << "Configuration Files:\n";
    std::cout << "  RPC config file should contain broker settings, topics, and client configuration.\n";
    std::cout << "  Package config file should contain device discovery settings, baudrates, and logging configuration.\n";
}

bool validateConfigFile(const std::string& filePath, const std::string& configType) {
    // Check if file exists
    if (access(filePath.c_str(), F_OK) != 0) {
        std::cerr << "Error: " << configType << " config file not found: " << filePath << std::endl;
        return false;
    }
    
    // Check if file is readable
    if (access(filePath.c_str(), R_OK) != 0) {
        std::cerr << "Error: " << configType << " config file is not readable: " << filePath << std::endl;
        return false;
    }
    
    // Try to parse the JSON file
    try {
        ConfigLoader loader;
        if (!loader.loadFromFile(filePath)) {
            std::cerr << "Error: Failed to parse " << configType << " config file: " << filePath << std::endl;
            return false;
        }
        
        // Validate required fields based on config type
        auto config = loader.getConfig();
        if (configType == "RPC") {
            // RPC config specific validation
            if (config.brokerHost.empty() || config.brokerPort == 0) {
                std::cerr << "Error: RPC config missing required broker settings (host/port)" << std::endl;
                return false;
            }
            std::cout << "✓ RPC config file validated: " << filePath << std::endl;
        } else if (configType == "Package") {
            // Package config specific validation
            if (config.baudrates.empty()) {
                std::cerr << "Error: Package config missing required baudrates setting" << std::endl;
                return false;
            }
            if (config.devicePathFilters.empty()) {
                std::cerr << "Error: Package config missing required devicePathFilters setting" << std::endl;
                return false;
            }
            std::cout << "✓ Package config file validated: " << filePath << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: Exception while validating " << configType << " config file: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    std::string rpcConfigFile;
    std::string packageConfigFile;
    
    // Parse command line arguments manually for better control
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "-rpc_config" || arg == "--rpc-config") {
            if (i + 1 < argc) {
                rpcConfigFile = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a file path argument." << std::endl;
                return 1;
            }
        } else if (arg == "-package_config" || arg == "--package-config") {
            if (i + 1 < argc) {
                packageConfigFile = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a file path argument." << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            std::cerr << "Use -h or --help for usage information." << std::endl;
            return 1;
        }
    }
    
    // Validate required arguments
    if (rpcConfigFile.empty()) {
        std::cerr << "Error: RPC config file is required. Use -rpc_config or --rpc-config." << std::endl;
        std::cerr << "Use -h or --help for usage information." << std::endl;
        return 1;
    }
    
    if (packageConfigFile.empty()) {
        std::cerr << "Error: Package config file is required. Use -package_config or --package-config." << std::endl;
        std::cerr << "Use -h or --help for usage information." << std::endl;
        return 1;
    }
    
    // Validate configuration files
    std::cout << "Validating configuration files..." << std::endl;
    
    if (!validateConfigFile(rpcConfigFile, "RPC")) {
        return 1;
    }
    
    if (!validateConfigFile(packageConfigFile, "Package")) {
        return 1;
    }
    
    std::cout << "All configuration files validated successfully." << std::endl;
    std::cout << "Starting MAVLink Device Discovery..." << std::endl;
    
    LOG_INFO("MAVLink Device Discovery starting...");
    LOG_INFO("RPC Config: " + rpcConfigFile);
    LOG_INFO("Package Config: " + packageConfigFile);
    
    DeviceManager manager;
    
    // Initialize RPC FIRST before starting device discovery
    LOG_INFO("Initializing RPC system...");
    if (!manager.initializeRpc(rpcConfigFile)) {
        LOG_ERROR("Failed to initialize RPC client");
        std::cerr << "Error: Failed to initialize RPC client with config: " << rpcConfigFile << std::endl;
        return 1;
    }
    
    // Initialize device discovery AFTER RPC is ready
    LOG_INFO("Initializing device manager...");
    if (!manager.initialize(packageConfigFile)) {
        LOG_ERROR("Failed to initialize device manager");
        std::cerr << "Error: Failed to initialize device manager with package config: " << packageConfigFile << std::endl;
        return 1;
    }
    
    try {
        manager.run();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in device manager: " + std::string(e.what()));
        std::cerr << "Error: Exception in device manager: " << e.what() << std::endl;
        return 1;
    }
    
    manager.shutdown();
    
    LOG_INFO("MAVLink Device Discovery stopped");
    std::cout << "MAVLink Device Discovery stopped." << std::endl;
    return 0;
}
