#pragma once

#include <string>
#include <map>
#include <variant>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>

// Include nlohmann JSON library
#include "nlohmann/json.hpp"

/// Enhanced JSON configuration parser for MAVLink Data Collector
/// Supports complex JSON structure with nested objects, arrays, and various data types
class JsonConfig
{
public:
    using Value = std::variant<std::string, int, bool, double>;
    using PrintConfig = std::map<std::string, bool>;
    using DataFields = std::map<std::string, std::vector<std::string>>;
    
    JsonConfig() = default;
    ~JsonConfig() = default;
    
    /// Load JSON configuration from file
    /// @param filePath Path to JSON configuration file
    /// @return true if loaded successfully
    bool loadFromFile(const std::string& filePath);
    
    /// Get string value
    /// @param key Configuration key
    /// @param defaultValue Default value if key not found
    /// @return String value
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    
    /// Get integer value
    /// @param key Configuration key
    /// @param defaultValue Default value if key not found
    /// @return Integer value
    int getInt(const std::string& key, int defaultValue = 0) const;
    
    /// Get boolean value
    /// @param key Configuration key
    /// @param defaultValue Default value if key not found
    /// @return Boolean value
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    /// Get double value
    /// @param key Configuration key
    /// @param defaultValue Default value if key not found
    /// @return Double value
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    
    /// Check if key exists
    /// @param key Configuration key
    /// @return true if key exists
    bool hasKey(const std::string& key) const;
    
    /// Get all keys
    /// @return Vector of all configuration keys
    std::vector<std::string> getKeys() const;
    
    /// Print configuration (for debugging)
    void printConfig() const;
    
    /// Get print configuration for data types
    /// @return Map of data type to print enable/disable
    const PrintConfig& getPrintConfig() const;
    
    /// Get data fields configuration for each data type
    /// @return Map of data type to list of fields
    const DataFields& getDataFields() const;
    
    /// Check if a specific data type should be printed
    /// @param dataType Name of the data type
    /// @return true if data type should be printed
    bool shouldPrint(const std::string& dataType) const;
    
    /// Get list of fields for a specific data type
    /// @param dataType Name of the data type
    /// @return Vector of field names
    std::vector<std::string> getFieldsForType(const std::string& dataType) const;
    
    /// Get JSON output configuration
    /// @return JSON object with output settings
    nlohmann::json getJsonOutputConfig() const;
    
    /// Get output format (json or text)
    /// @return Output format string
    std::string getOutputFormat() const;

private:
    std::map<std::string, Value> _values;
    nlohmann::json _jsonRoot;
    PrintConfig _printConfig;
    DataFields _dataFields;
    nlohmann::json _jsonOutputConfig;
    std::string _outputFormat = "json";
    
    /// Parse JSON string using nlohmann JSON
    /// @param jsonString JSON string to parse
    /// @return true if parsed successfully
    bool parseJson(const std::string& jsonString);
    
    /// Load configuration sections
    void loadConfigurationSections();
    
    /// Load print configuration from JSON
    void loadPrintConfig();
    
    /// Load data fields configuration from JSON
    void loadDataFields();
    
    /// Load JSON output configuration
    void loadJsonOutputConfig();
    
    /// Legacy methods for backward compatibility
    std::string trim(const std::string& str) const;
    std::string extractKey(const std::string& line) const;
    std::string extractValue(const std::string& line) const;
    Value parseValue(const std::string& token) const;
};
