
#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include "json.hpp"

using json = nlohmann::json;

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    // Load package configuration from file
    bool loadPackageConfig(const std::string& config_file_path);
    
    // Get configuration values
    std::string getOperation() const;
    std::string getServersListPath() const;
    std::string getOutputDir() const;
    json getFilters() const;
    json getTestConfig() const;
    std::string getOutputFile() const;
    
    // Get the full package config
    const json& getPackageConfig() const;
    
    // Validate configuration
    bool validateConfig() const;
    
private:
    json package_config_;
    bool config_loaded_;
    
    // Helper methods
    std::string getConfigString(const std::string& key, const std::string& default_value = "") const;
    json getConfigJson(const std::string& key, const json& default_value = json::object()) const;
};

#endif // CONFIG_MANAGER_HPP
