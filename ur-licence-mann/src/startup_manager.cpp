#include "startup_manager.hpp"
#include <iostream>
#include <fstream>

StartupManager::StartupManager(bool verbose) 
    : verbose_(verbose), initialized_(false) {
}

StartupManager::~StartupManager() = default;

bool StartupManager::initialize(const StartupPaths& paths) {
    log("Initializing startup manager...");
    paths_ = paths;
    
    bool success = true;
    
    if (!check_and_create_paths()) {
        std::cerr << "Failed to create required paths" << std::endl;
        success = false;
    }
    
    if (!check_and_create_files()) {
        std::cerr << "Failed to create required files" << std::endl;
        success = false;
    }
    
    initialized_ = success;
    
    if (initialized_) {
        log("Startup initialization completed successfully");
    } else {
        log("Startup initialization completed with errors");
    }
    
    return initialized_;
}

bool StartupManager::check_and_create_paths() {
    log("Checking and creating required paths...");
    
    bool success = true;
    
    if (!verify_directory(paths_.keys_dir)) {
        std::cerr << "Failed to create keys directory: " << paths_.keys_dir << std::endl;
        success = false;
    }
    
    if (!verify_directory(paths_.config_dir)) {
        std::cerr << "Failed to create config directory: " << paths_.config_dir << std::endl;
        success = false;
    }
    
    if (!verify_directory(paths_.licenses_dir)) {
        std::cerr << "Failed to create licenses directory: " << paths_.licenses_dir << std::endl;
        success = false;
    }
    
    return success;
}

bool StartupManager::check_and_create_files() {
    log("Checking and creating required files...");
    
    bool success = true;
    
    if (!verify_file(paths_.definitions_file, true, get_default_license_definitions_json())) {
        std::cerr << "Failed to create definitions file: " << paths_.definitions_file << std::endl;
        success = false;
    }
    
    if (!verify_file(paths_.app_config_file, true, get_default_app_config_json())) {
        std::cerr << "Failed to create app config file: " << paths_.app_config_file << std::endl;
        success = false;
    }
    
    return success;
}

bool StartupManager::verify_directory(const std::string& dir_path, bool create_if_missing) {
    try {
        if (std::filesystem::exists(dir_path)) {
            if (!std::filesystem::is_directory(dir_path)) {
                std::cerr << "Path exists but is not a directory: " << dir_path << std::endl;
                return false;
            }
            log("Directory exists: " + dir_path);
            return true;
        }
        
        if (create_if_missing) {
            if (std::filesystem::create_directories(dir_path)) {
                log("Created directory: " + dir_path);
                return true;
            } else {
                std::cerr << "Failed to create directory: " << dir_path << std::endl;
                return false;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "Error verifying directory '" << dir_path << "': " << e.what() << std::endl;
        return false;
    }
}

bool StartupManager::verify_file(const std::string& file_path, bool create_if_missing,
                                  const std::string& default_content) {
    try {
        if (std::filesystem::exists(file_path)) {
            log("File exists: " + file_path);
            return true;
        }
        
        if (create_if_missing) {
            std::filesystem::path path(file_path);
            if (path.has_parent_path()) {
                std::filesystem::create_directories(path.parent_path());
            }
            
            std::ofstream file(file_path);
            if (!file.is_open()) {
                std::cerr << "Failed to create file: " << file_path << std::endl;
                return false;
            }
            
            file << default_content;
            log("Created file: " + file_path);
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "Error verifying file '" << file_path << "': " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> StartupManager::get_missing_paths() const {
    std::vector<std::string> missing;
    
    if (!std::filesystem::exists(paths_.keys_dir)) {
        missing.push_back(paths_.keys_dir);
    }
    if (!std::filesystem::exists(paths_.config_dir)) {
        missing.push_back(paths_.config_dir);
    }
    if (!std::filesystem::exists(paths_.licenses_dir)) {
        missing.push_back(paths_.licenses_dir);
    }
    
    return missing;
}

std::vector<std::string> StartupManager::get_missing_files() const {
    std::vector<std::string> missing;
    
    if (!std::filesystem::exists(paths_.definitions_file)) {
        missing.push_back(paths_.definitions_file);
    }
    if (!std::filesystem::exists(paths_.app_config_file)) {
        missing.push_back(paths_.app_config_file);
    }
    
    return missing;
}

void StartupManager::log(const std::string& message) const {
    if (verbose_) {
        std::cout << "[StartupManager] " << message << std::endl;
    }
}

std::string StartupManager::get_default_license_definitions_json() const {
    return R"([
  {
    "license_type": "UltimaOpenLicence",
    "features": [
      {"feature_name": "basic_features", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "advanced_analytics", "feature_status": "DISABLED"},
      {"feature_name": "cloud_sync", "feature_status": "DISABLED"}
    ]
  },
  {
    "license_type": "UltimaProfessionalLicence",
    "features": [
      {"feature_name": "basic_features", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "advanced_analytics", "feature_status": "LIMITED_ACCESS"},
      {"feature_name": "cloud_sync", "feature_status": "LIMITED_ACCESS"},
      {"feature_name": "priority_support", "feature_status": "UNLIMITED_ACCESS"}
    ]
  },
  {
    "license_type": "UltimaEnterpriseLicence",
    "features": [
      {"feature_name": "basic_features", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "advanced_analytics", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "cloud_sync", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "priority_support", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "custom_integrations", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "dedicated_account_manager", "feature_status": "UNLIMITED_ACCESS"}
    ]
  },
  {
    "license_type": "UltimaDeveloperLicence",
    "features": [
      {"feature_name": "basic_features", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "advanced_analytics", "feature_status": "LIMITED_ACCESS_V2"},
      {"feature_name": "cloud_sync", "feature_status": "LIMITED_ACCESS_V2"},
      {"feature_name": "development_tools", "feature_status": "UNLIMITED_ACCESS"},
      {"feature_name": "api_access", "feature_status": "UNLIMITED_ACCESS"}
    ]
  }
])";
}

std::string StartupManager::get_default_app_config_json() const {
    return R"({
  "application": {
    "name": "ur-licence-mann",
    "version": "1.0.0",
    "author": "License Management System"
  },
  "paths": {
    "keys_directory": "./keys",
    "config_directory": "./config",
    "licenses_directory": "./licenses"
  },
  "crypto": {
    "default_key_size": 2048,
    "encryption_algorithm": "AES-256-CBC",
    "signature_algorithm": "RSA-SHA256"
  },
  "license": {
    "default_expiry_days": 365,
    "allow_hardware_binding": true,
    "require_encryption": false
  },
  "watchdog": {
    "enabled": false,
    "watch_interval_seconds": 5,
    "auto_reload_on_change": true
  }
})";
}
