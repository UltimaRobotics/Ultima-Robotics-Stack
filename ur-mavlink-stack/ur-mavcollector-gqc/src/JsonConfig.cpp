#include "JsonConfig.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

bool JsonConfig::loadFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open JSON config file: " << filePath << std::endl;
        return false;
    }

    try {
        // Parse JSON using nlohmann library
        file >> _jsonRoot;
        
        // Load all configuration sections
        loadConfigurationSections();
        
        // For backward compatibility, populate the legacy _values map
        _values.clear();
        for (auto& [key, value] : _jsonRoot.items()) {
            if (value.is_string()) {
                _values[key] = value.get<std::string>();
            } else if (value.is_number_integer()) {
                _values[key] = value.get<int>();
            } else if (value.is_number_float()) {
                _values[key] = value.get<double>();
            } else if (value.is_boolean()) {
                _values[key] = value.get<bool>();
            }
        }
        
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        // Fallback to legacy parser
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string jsonString = buffer.str();
        return parseJson(jsonString);
    } catch (const std::exception& e) {
        std::cerr << "Error loading JSON config: " << e.what() << std::endl;
        return false;
    }
}

void JsonConfig::loadConfigurationSections()
{
    loadPrintConfig();
    loadDataFields();
    loadJsonOutputConfig();
    
    // Load output format
    if (_jsonRoot.contains("output_format")) {
        _outputFormat = _jsonRoot["output_format"].get<std::string>();
    }
}

void JsonConfig::loadPrintConfig()
{
    _printConfig.clear();
    
    if (_jsonRoot.contains("print_config") && _jsonRoot["print_config"].is_object()) {
        for (auto& [key, value] : _jsonRoot["print_config"].items()) {
            if (value.is_boolean()) {
                _printConfig[key] = value.get<bool>();
            }
        }
    } else {
        // Default configuration - enable all data types
        _printConfig = {
            {"vehicle_info", true},
            {"system_status", true},
            {"gps_data", true},
            {"gps2_data", true},
            {"battery_data", true},
            {"rc_data", true},
            {"vibration_data", true},
            {"temperature_data", true},
            {"estimator_status", true},
            {"wind_data", true},
            {"parameters", true},
            {"connection_stats", true},
            {"heartbeat", true},
            {"autopilot_version", true}
        };
    }
}

void JsonConfig::loadDataFields()
{
    _dataFields.clear();
    
    if (_jsonRoot.contains("data_fields") && _jsonRoot["data_fields"].is_object()) {
        for (auto& [key, value] : _jsonRoot["data_fields"].items()) {
            if (value.is_array()) {
                std::vector<std::string> fields;
                for (const auto& field : value) {
                    if (field.is_string()) {
                        fields.push_back(field.get<std::string>());
                    }
                }
                _dataFields[key] = fields;
            }
        }
    }
}

void JsonConfig::loadJsonOutputConfig()
{
    if (_jsonRoot.contains("json_output") && _jsonRoot["json_output"].is_object()) {
        _jsonOutputConfig = _jsonRoot["json_output"];
    } else {
        // Default JSON output configuration
        _jsonOutputConfig = nlohmann::json{
            {"pretty_print", true},
            {"include_timestamp", true},
            {"include_metadata", true},
            {"compact_arrays", false},
            {"stringify_numbers", false}
        };
    }
}

const JsonConfig::PrintConfig& JsonConfig::getPrintConfig() const
{
    return _printConfig;
}

const JsonConfig::DataFields& JsonConfig::getDataFields() const
{
    return _dataFields;
}

bool JsonConfig::shouldPrint(const std::string& dataType) const
{
    auto it = _printConfig.find(dataType);
    if (it != _printConfig.end()) {
        return it->second;
    }
    
    // Default to true if not specified
    return true;
}

std::vector<std::string> JsonConfig::getFieldsForType(const std::string& dataType) const
{
    auto it = _dataFields.find(dataType);
    if (it != _dataFields.end()) {
        return it->second;
    }
    
    // Return empty vector if no fields configured
    return {};
}

nlohmann::json JsonConfig::getJsonOutputConfig() const
{
    return _jsonOutputConfig;
}

std::string JsonConfig::getOutputFormat() const
{
    return _outputFormat;
}

bool JsonConfig::parseJson(const std::string& jsonString)
{
    _values.clear();
    
    std::istringstream stream(jsonString);
    std::string line;
    bool inObject = false;
    
    while (std::getline(stream, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '/' || line[0] == '{' || line[0] == '}') {
            if (line[0] == '{') inObject = true;
            continue;
        }
        
        // Look for key-value pairs
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string key = extractKey(line);
        std::string valueToken = extractValue(line);
        
        if (!key.empty() && !valueToken.empty()) {
            Value value = parseValue(valueToken);
            _values[key] = value;
        }
    }
    
    return !_values.empty();
}

std::string JsonConfig::trim(const std::string& str) const
{
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string JsonConfig::extractKey(const std::string& line) const
{
    size_t start = line.find('"');
    if (start == std::string::npos) return "";
    
    size_t end = line.find('"', start + 1);
    if (end == std::string::npos) return "";
    
    return line.substr(start + 1, end - start - 1);
}

std::string JsonConfig::extractValue(const std::string& line) const
{
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) return "";
    
    std::string value = line.substr(colonPos + 1);
    value = trim(value);
    
    // Remove trailing comma if present
    if (!value.empty() && value.back() == ',') {
        value.pop_back();
    }
    
    return trim(value);
}

JsonConfig::Value JsonConfig::parseValue(const std::string& token) const
{
    std::string trimmed = trim(token);
    
    // Boolean values
    if (trimmed == "true") return true;
    if (trimmed == "false") return false;
    
    // String values (quoted)
    if (trimmed.front() == '"' && trimmed.back() == '"') {
        return trimmed.substr(1, trimmed.length() - 2);
    }
    
    // Numeric values
    try {
        // Check if it's a double (contains decimal point)
        if (trimmed.find('.') != std::string::npos) {
            return std::stod(trimmed);
        }
        // Try integer first
        return std::stoi(trimmed);
    } catch (const std::exception&) {
        // Fallback to string if parsing fails
        return trimmed;
    }
}

std::string JsonConfig::getString(const std::string& key, const std::string& defaultValue) const
{
    auto it = _values.find(key);
    if (it == _values.end()) return defaultValue;
    
    try {
        return std::get<std::string>(it->second);
    } catch (const std::bad_variant_access&) {
        try {
            // Convert other types to string
            if (std::holds_alternative<int>(it->second)) {
                return std::to_string(std::get<int>(it->second));
            } else if (std::holds_alternative<double>(it->second)) {
                return std::to_string(std::get<double>(it->second));
            } else if (std::holds_alternative<bool>(it->second)) {
                return std::get<bool>(it->second) ? "true" : "false";
            }
        } catch (...) {
            // Fall through to default
        }
    }
    
    return defaultValue;
}

int JsonConfig::getInt(const std::string& key, int defaultValue) const
{
    auto it = _values.find(key);
    if (it == _values.end()) return defaultValue;
    
    try {
        return std::get<int>(it->second);
    } catch (const std::bad_variant_access&) {
        try {
            if (std::holds_alternative<double>(it->second)) {
                return static_cast<int>(std::get<double>(it->second));
            }
        } catch (...) {
            // Fall through to default
        }
    }
    
    return defaultValue;
}

bool JsonConfig::getBool(const std::string& key, bool defaultValue) const
{
    auto it = _values.find(key);
    if (it == _values.end()) return defaultValue;
    
    try {
        return std::get<bool>(it->second);
    } catch (const std::bad_variant_access&) {
        // Fall through to default
    }
    
    return defaultValue;
}

double JsonConfig::getDouble(const std::string& key, double defaultValue) const
{
    auto it = _values.find(key);
    if (it == _values.end()) return defaultValue;
    
    try {
        return std::get<double>(it->second);
    } catch (const std::bad_variant_access&) {
        try {
            if (std::holds_alternative<int>(it->second)) {
                return static_cast<double>(std::get<int>(it->second));
            }
        } catch (...) {
            // Fall through to default
        }
    }
    
    return defaultValue;
}

bool JsonConfig::hasKey(const std::string& key) const
{
    return _values.find(key) != _values.end();
}

std::vector<std::string> JsonConfig::getKeys() const
{
    std::vector<std::string> keys;
    for (const auto& pair : _values) {
        keys.push_back(pair.first);
    }
    return keys;
}

void JsonConfig::printConfig() const
{
    std::cout << "=== JSON Configuration ===" << std::endl;
    
    if (_outputFormat == "json") {
        std::cout << "Output Format: JSON" << std::endl;
        std::cout << "Print Configuration:" << std::endl;
        for (const auto& [dataType, enabled] : _printConfig) {
            std::cout << "  " << dataType << ": " << (enabled ? "enabled" : "disabled") << std::endl;
        }
        
        std::cout << "Data Fields Configuration:" << std::endl;
        for (const auto& [dataType, fields] : _dataFields) {
            std::cout << "  " << dataType << ": [";
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << fields[i];
            }
            std::cout << "]" << std::endl;
        }
        
        std::cout << "JSON Output Configuration:" << std::endl;
        std::cout << _jsonOutputConfig.dump(2) << std::endl;
    } else {
        // Legacy format
        for (const auto& pair : _values) {
            std::cout << pair.first << ": ";
            if (std::holds_alternative<std::string>(pair.second)) {
                std::cout << std::get<std::string>(pair.second);
            } else if (std::holds_alternative<int>(pair.second)) {
                std::cout << std::get<int>(pair.second);
            } else if (std::holds_alternative<double>(pair.second)) {
                std::cout << std::get<double>(pair.second);
            } else if (std::holds_alternative<bool>(pair.second)) {
                std::cout << (std::get<bool>(pair.second) ? "true" : "false");
            } else {
                std::cout << "<unknown>";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "=========================" << std::endl;
}
