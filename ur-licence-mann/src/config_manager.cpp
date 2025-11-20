#include "config_manager.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

ConfigManager::ConfigManager(bool verbose) 
    : verbose_(verbose), config_loaded_(false) {
}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::load_config(const std::string& config_path) {
    try {
        log("Loading configuration from: " + config_path);
        
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Configuration file does not exist: " << config_path << std::endl;
            return false;
        }
        
        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open configuration file: " << config_path << std::endl;
            return false;
        }
        
        file >> config_;
        config_path_ = config_path;
        config_loaded_ = true;
        
        log("Configuration loaded successfully");
        return true;
        
    } catch (const json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::save_config(const std::string& config_path) {
    try {
        log("Saving configuration to: " + config_path);
        
        std::filesystem::path path(config_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        
        std::ofstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open configuration file for writing: " << config_path << std::endl;
            return false;
        }
        
        file << config_.dump(4);
        config_path_ = config_path;
        
        log("Configuration saved successfully");
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving configuration: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigManager::get_value(const std::string& key, const std::string& default_value) const {
    try {
        if (config_.contains(key)) {
            return config_[key].get<std::string>();
        }
    } catch (const json::exception&) {
    }
    return default_value;
}

void ConfigManager::set_value(const std::string& key, const std::string& value) {
    config_[key] = value;
}

bool ConfigManager::has_key(const std::string& key) const {
    return config_.contains(key);
}

std::map<std::string, std::string> ConfigManager::get_all_values() const {
    std::map<std::string, std::string> values;
    
    for (auto it = config_.begin(); it != config_.end(); ++it) {
        try {
            values[it.key()] = it.value().dump();
        } catch (const json::exception&) {
        }
    }
    
    return values;
}

void ConfigManager::log(const std::string& message) const {
    if (verbose_) {
        std::cout << "[ConfigManager] " << message << std::endl;
    }
}
