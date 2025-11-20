#include "ConfigParser.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>

namespace FlightCollector {

ConnectionConfig ConfigParser::parseConfig(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file);
    }
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        throw std::runtime_error("Failed to parse JSON config: " + errors);
    }
    
    if (!hasRequiredFields(root)) {
        throw std::runtime_error("Missing required fields in config file");
    }
    
    ConnectionConfig config;
    
    // Parse connection type
    config.type = parseConnectionType(root["connection"]["type"].asString());
    
    // Parse connection details
    if (config.type == "udp" || config.type == "tcp") {
        config.address = root["connection"]["address"].asString();
        config.port = root["connection"]["port"].asInt();
    } else if (config.type == "serial") {
        config.address = root["connection"]["device"].asString();
        config.baudrate = root["connection"]["baudrate"].asInt();
    }
    
    // Parse system and component IDs
    if (root["connection"].isMember("system_id")) {
        config.system_id = root["connection"]["system_id"].asString();
    }
    if (root["connection"].isMember("component_id")) {
        config.component_id = root["connection"]["component_id"].asString();
    }
    
    // Parse timeout
    if (root["connection"].isMember("timeout_s")) {
        config.timeout_s = root["connection"]["timeout_s"].asInt();
    }
    
    return config;
}

bool ConfigParser::validateConfig(const ConnectionConfig& config) {
    // Check connection type
    if (config.type != "udp" && config.type != "tcp" && config.type != "serial") {
        std::cerr << "Invalid connection type: " << config.type << std::endl;
        return false;
    }
    
    // Check connection-specific parameters
    if (config.type == "udp" || config.type == "tcp") {
        if (config.address.empty()) {
            std::cerr << "Address is required for UDP/TCP connections" << std::endl;
            return false;
        }
        if (config.port <= 0 || config.port > 65535) {
            std::cerr << "Invalid port number: " << config.port << std::endl;
            return false;
        }
    } else if (config.type == "serial") {
        if (config.address.empty()) {
            std::cerr << "Device path is required for serial connections" << std::endl;
            return false;
        }
        if (config.baudrate <= 0) {
            std::cerr << "Invalid baudrate: " << config.baudrate << std::endl;
            return false;
        }
    }
    
    // Check timeout
    if (config.timeout_s <= 0) {
        std::cerr << "Invalid timeout: " << config.timeout_s << std::endl;
        return false;
    }
    
    return true;
}

ConnectionConfig ConfigParser::getDefaultConfig() {
    ConnectionConfig config;
    config.type = "udp";
    config.address = "127.0.0.1";
    config.port = 14550;
    config.baudrate = 57600;
    config.system_id = "1";
    config.component_id = "1";
    config.timeout_s = 10;
    return config;
}

void ConfigParser::saveConfig(const ConnectionConfig& config, const std::string& config_file) {
    Json::Value root;
    
    // Connection section
    root["connection"]["type"] = config.type;
    
    if (config.type == "udp" || config.type == "tcp") {
        root["connection"]["address"] = config.address;
        root["connection"]["port"] = config.port;
    } else if (config.type == "serial") {
        root["connection"]["device"] = config.address;
        root["connection"]["baudrate"] = config.baudrate;
    }
    
    root["connection"]["system_id"] = config.system_id;
    root["connection"]["component_id"] = config.component_id;
    root["connection"]["timeout_s"] = config.timeout_s;
    
    // Write to file
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::ofstream file(config_file);
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &file);
    
    if (!file.good()) {
        throw std::runtime_error("Failed to write config file: " + config_file);
    }
}

std::string ConfigParser::parseConnectionType(const std::string& type_str) {
    std::string lower_type = type_str;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(), ::tolower);
    
    if (lower_type == "udp") return "udp";
    if (lower_type == "tcp") return "tcp";
    if (lower_type == "serial") return "serial";
    
    throw std::runtime_error("Unsupported connection type: " + type_str);
}

bool ConfigParser::hasRequiredFields(const Json::Value& config) {
    if (!config.isMember("connection")) {
        std::cerr << "Missing 'connection' section" << std::endl;
        return false;
    }
    
    const Json::Value& connection = config["connection"];
    
    if (!connection.isMember("type")) {
        std::cerr << "Missing connection type" << std::endl;
        return false;
    }
    
    std::string type = parseConnectionType(connection["type"].asString());
    
    if (type == "udp" || type == "tcp") {
        if (!connection.isMember("address") || !connection.isMember("port")) {
            std::cerr << "Missing address or port for UDP/TCP connection" << std::endl;
            return false;
        }
    } else if (type == "serial") {
        if (!connection.isMember("device")) {
            std::cerr << "Missing device path for serial connection" << std::endl;
            return false;
        }
    }
    
    return true;
}

} // namespace FlightCollector
