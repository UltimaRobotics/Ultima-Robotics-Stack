
#include "../include/ConfigManager.hpp"
#include <fstream>
#include <iostream>

ConfigManager::ConfigManager() : config_loaded_(false) {
}

ConfigManager::~ConfigManager() {
}

bool ConfigManager::loadPackageConfig(const std::string& config_file_path) {
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        std::cerr << "[ConfigManager] Error: Could not open config file: " << config_file_path << std::endl;
        return false;
    }
    
    try {
        file >> package_config_;
        file.close();
        config_loaded_ = true;
        
        std::cout << "[ConfigManager] Successfully loaded config from: " << config_file_path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ConfigManager] Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigManager::getOperation() const {
    return getConfigString("operation");
}

std::string ConfigManager::getServersListPath() const {
    return getConfigString("servers_list_path");
}

std::string ConfigManager::getOutputDir() const {
    return getConfigString("output_dir", "runtime-data/server-status");
}

json ConfigManager::getFilters() const {
    return getConfigJson("filters");
}

json ConfigManager::getTestConfig() const {
    std::string operation = getOperation();
    
    // Return operation-specific config if it exists
    if (package_config_.contains(operation)) {
        return package_config_[operation];
    }
    
    // Fallback to test_config for backward compatibility
    return getConfigJson("test_config");
}

std::string ConfigManager::getOutputFile() const {
    return getConfigString("output_file");
}

const json& ConfigManager::getPackageConfig() const {
    return package_config_;
}

bool ConfigManager::validateConfig() const {
    if (!config_loaded_) {
        std::cerr << "[ConfigManager] Error: No configuration loaded" << std::endl;
        return false;
    }
    
    if (!package_config_.contains("operation")) {
        std::cerr << "[ConfigManager] Error: 'operation' field is required" << std::endl;
        return false;
    }
    
    std::string operation = getOperation();
    
    if (operation == "servers-status") {
        if (!package_config_.contains("servers_list_path")) {
            std::cerr << "[ConfigManager] Error: 'servers_list_path' is required for servers-status operation" << std::endl;
            return false;
        }
    }
    
    return true;
}

std::string ConfigManager::getConfigString(const std::string& key, const std::string& default_value) const {
    if (!config_loaded_) {
        return default_value;
    }
    
    if (package_config_.contains(key)) {
        return package_config_[key].get<std::string>();
    }
    
    return default_value;
}

json ConfigManager::getConfigJson(const std::string& key, const json& default_value) const {
    if (!config_loaded_) {
        return default_value;
    }
    
    if (package_config_.contains(key)) {
        return package_config_[key];
    }
    
    return default_value;
}
