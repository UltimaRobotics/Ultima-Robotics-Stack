#ifndef PACKAGE_CONFIG_H
#define PACKAGE_CONFIG_H

#include <string>
#include <cstdint>
#include <iostream>
#include <fstream>
#include "../thirdparty/nlohmann/json.hpp"

using json = nlohmann::json;

struct PackageConfig {
    std::string address;
    uint16_t port;
    uint8_t system_id;
    uint8_t component_id;
    bool verbose;
    
    PackageConfig() : 
        address("127.0.0.1"),
        port(44003),
        system_id(250),
        component_id(1),
        verbose(false) {}
    
    bool loadFromFile(const std::string& config_file_path) {
        try {
            std::ifstream config_file(config_file_path);
            if (!config_file.is_open()) {
                std::cerr << "Failed to open config file: " << config_file_path << std::endl;
                return false;
            }
            
            json config_json;
            config_file >> config_json;
            config_file.close();
            
            // Parse configuration values
            if (config_json.contains("address")) {
                address = config_json["address"].get<std::string>();
            }
            
            if (config_json.contains("port")) {
                port = config_json["port"].get<uint16_t>();
            }
            
            if (config_json.contains("system_id")) {
                system_id = config_json["system_id"].get<uint8_t>();
            }
            
            if (config_json.contains("component_id")) {
                component_id = config_json["component_id"].get<uint8_t>();
            }
            
            if (config_json.contains("verbose")) {
                verbose = config_json["verbose"].get<bool>();
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config file: " << e.what() << std::endl;
            return false;
        }
    }
    
    void print() const {
        std::cout << "Package Configuration:" << std::endl;
        std::cout << "  Address: " << address << std::endl;
        std::cout << "  Port: " << port << std::endl;
        std::cout << "  System ID: " << static_cast<int>(system_id) << std::endl;
        std::cout << "  Component ID: " << static_cast<int>(component_id) << std::endl;
        std::cout << "  Verbose: " << (verbose ? "true" : "false") << std::endl;
    }
};

#endif // PACKAGE_CONFIG_H
