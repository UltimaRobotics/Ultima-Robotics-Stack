#pragma once

#include "DataStructures.h"
#include <string>
#include <vector>

// Forward declaration
namespace Json {
    class Value;
}

namespace FlightCollector {

/**
 * @brief Configuration parser for JSON config files
 */
class ConfigParser {
public:
    /**
     * @brief Parse configuration from JSON file
     * @param config_file Path to JSON configuration file
     * @return ConnectionConfig structure
     * @throws std::runtime_error on parsing failure
     */
    static ConnectionConfig parseConfig(const std::string& config_file);
    
    /**
     * @brief Validate configuration structure
     * @param config Configuration to validate
     * @return true if valid, false otherwise
     */
    static bool validateConfig(const ConnectionConfig& config);
    
    /**
     * @brief Generate default configuration
     * @return Default ConnectionConfig
     */
    static ConnectionConfig getDefaultConfig();
    
    /**
     * @brief Save configuration to JSON file
     * @param config Configuration to save
     * @param config_file Output file path
     * @throws std::runtime_error on save failure
     */
    static void saveConfig(const ConnectionConfig& config, const std::string& config_file);

private:
    /**
     * @brief Parse connection type from string
     * @param type_str Connection type string
     * @return Normalized connection type
     */
    static std::string parseConnectionType(const std::string& type_str);
    
    /**
     * @brief Check if required fields are present
     * @param config JSON configuration object
     * @return true if all required fields present
     */
    static bool hasRequiredFields(const Json::Value& config);
};

} // namespace FlightCollector
