#include "timeout_config.h"
#include <fstream>
#include <iostream>
#include <json/json.h>

// Global timeout configuration instance
TimeoutConfig g_timeout_config;

void TimeoutConfig::initializeTimeoutMap() {
    timeout_map.clear();
    
    // QMI Session timeouts
    timeout_map["qmi_device_open_timeout"] = &qmi_device_open_timeout;
    timeout_map["qmi_start_network_timeout"] = &qmi_start_network_timeout;
    timeout_map["qmi_stop_network_timeout"] = &qmi_stop_network_timeout;
    timeout_map["qmi_get_status_timeout"] = &qmi_get_status_timeout;
    timeout_map["qmi_get_device_info_timeout"] = &qmi_get_device_info_timeout;
    timeout_map["qmi_set_operating_mode_timeout"] = &qmi_set_operating_mode_timeout;
    
    // Interface Controller timeouts
    timeout_map["dhcp_timeout"] = &dhcp_timeout;
    timeout_map["interface_up_timeout"] = &interface_up_timeout;
    timeout_map["interface_down_timeout"] = &interface_down_timeout;
    timeout_map["ip_config_timeout"] = &ip_config_timeout;
    timeout_map["dns_config_timeout"] = &dns_config_timeout;
    timeout_map["routing_timeout"] = &routing_timeout;
    
    // Connectivity Monitor timeouts
    timeout_map["ping_timeout"] = &ping_timeout;
    timeout_map["dns_resolution_timeout"] = &dns_resolution_timeout;
    timeout_map["http_test_timeout"] = &http_test_timeout;
    timeout_map["connectivity_check_interval"] = &connectivity_check_interval;
    
    // Failure Detector timeouts
    timeout_map["signal_strength_check_timeout"] = &signal_strength_check_timeout;
    timeout_map["registration_check_timeout"] = &registration_check_timeout;
    timeout_map["data_bearer_check_timeout"] = &data_bearer_check_timeout;
    timeout_map["interface_status_check_timeout"] = &interface_status_check_timeout;
    
    // Recovery Engine timeouts
    timeout_map["modem_reset_timeout"] = &modem_reset_timeout;
    timeout_map["network_rescan_timeout"] = &network_rescan_timeout;
    timeout_map["reconnect_attempt_timeout"] = &reconnect_attempt_timeout;
    timeout_map["recovery_operation_timeout"] = &recovery_operation_timeout;
    
    // General operation timeouts
    timeout_map["command_execution_timeout"] = &command_execution_timeout;
    timeout_map["state_transition_timeout"] = &state_transition_timeout;
    timeout_map["monitoring_interval"] = &monitoring_interval;
    timeout_map["retry_delay"] = &retry_delay;
}

bool TimeoutConfig::loadFromFile(const std::string& config_file) {
    initializeTimeoutMap();
    
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open timeout config file: " << config_file 
                  << ". Using default timeouts." << std::endl;
        return false;
    }
    
    Json::Value root;
    Json::CharReaderBuilder reader_builder;
    std::string errs;
    
    if (!Json::parseFromStream(reader_builder, file, &root, &errs)) {
        std::cerr << "Error parsing timeout config file: " << errs << std::endl;
        return false;
    }
    
    // Load timeouts from JSON
    bool config_loaded = false;
    
    if (root.isMember("timeouts") && root["timeouts"].isObject()) {
        const Json::Value& timeouts = root["timeouts"];
        
        for (const auto& member : timeouts.getMemberNames()) {
            if (timeouts[member].isInt()) {
                int timeout_value = timeouts[member].asInt();
                if (setTimeout(member, timeout_value)) {
                    config_loaded = true;
                    std::cout << "Loaded timeout: " << member << " = " << timeout_value << "ms" << std::endl;
                } else {
                    std::cerr << "Warning: Unknown timeout parameter: " << member << std::endl;
                }
            } else {
                std::cerr << "Warning: Invalid timeout value for: " << member << std::endl;
            }
        }
    }
    
    if (config_loaded) {
        std::cout << "Timeout configuration loaded from: " << config_file << std::endl;
        if (!validateTimeouts()) {
            std::cerr << "Warning: Some timeout values may be invalid" << std::endl;
        }
    } else {
        std::cerr << "Warning: No valid timeout configuration found in file. Using defaults." << std::endl;
        return false;
    }
    
    return config_loaded;
}

bool TimeoutConfig::saveToFile(const std::string& config_file) const {
    Json::Value root;
    Json::Value timeouts;
    
    // Create timeout mapping for saving
    auto temp_config = const_cast<TimeoutConfig*>(this);
    temp_config->initializeTimeoutMap();
    
    // Save all timeout values
    for (const auto& pair : timeout_map) {
        timeouts[pair.first] = *(pair.second);
    }
    
    root["timeouts"] = timeouts;
    root["description"] = "QMI Connection Manager Timeout Configuration";
    root["version"] = "1.0";
    
    std::ofstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not create timeout config file: " << config_file << std::endl;
        return false;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &file);
    
    std::cout << "Timeout configuration saved to: " << config_file << std::endl;
    return true;
}

int TimeoutConfig::getTimeout(const std::string& timeout_name) const {
    auto temp_config = const_cast<TimeoutConfig*>(this);
    temp_config->initializeTimeoutMap();
    
    auto it = timeout_map.find(timeout_name);
    if (it != timeout_map.end()) {
        return *(it->second);
    }
    return -1; // Timeout not found
}

bool TimeoutConfig::setTimeout(const std::string& timeout_name, int timeout_value) {
    if (timeout_map.empty()) {
        initializeTimeoutMap();
    }
    
    auto it = timeout_map.find(timeout_name);
    if (it != timeout_map.end()) {
        *(it->second) = timeout_value;
        return true;
    }
    return false; // Timeout name not found
}

void TimeoutConfig::printConfiguration() const {
    std::cout << "\n=== Current Timeout Configuration ===" << std::endl;
    
    std::cout << "\nQMI Session Timeouts:" << std::endl;
    std::cout << "  qmi_device_open_timeout: " << qmi_device_open_timeout << "ms" << std::endl;
    std::cout << "  qmi_start_network_timeout: " << qmi_start_network_timeout << "ms" << std::endl;
    std::cout << "  qmi_stop_network_timeout: " << qmi_stop_network_timeout << "ms" << std::endl;
    std::cout << "  qmi_get_status_timeout: " << qmi_get_status_timeout << "ms" << std::endl;
    std::cout << "  qmi_get_device_info_timeout: " << qmi_get_device_info_timeout << "ms" << std::endl;
    std::cout << "  qmi_set_operating_mode_timeout: " << qmi_set_operating_mode_timeout << "ms" << std::endl;
    
    std::cout << "\nInterface Controller Timeouts:" << std::endl;
    std::cout << "  dhcp_timeout: " << dhcp_timeout << "ms" << std::endl;
    std::cout << "  interface_up_timeout: " << interface_up_timeout << "ms" << std::endl;
    std::cout << "  interface_down_timeout: " << interface_down_timeout << "ms" << std::endl;
    std::cout << "  ip_config_timeout: " << ip_config_timeout << "ms" << std::endl;
    std::cout << "  dns_config_timeout: " << dns_config_timeout << "ms" << std::endl;
    std::cout << "  routing_timeout: " << routing_timeout << "ms" << std::endl;
    
    std::cout << "\nConnectivity Monitor Timeouts:" << std::endl;
    std::cout << "  ping_timeout: " << ping_timeout << "ms" << std::endl;
    std::cout << "  dns_resolution_timeout: " << dns_resolution_timeout << "ms" << std::endl;
    std::cout << "  http_test_timeout: " << http_test_timeout << "ms" << std::endl;
    std::cout << "  connectivity_check_interval: " << connectivity_check_interval << "ms" << std::endl;
    
    std::cout << "\nFailure Detector Timeouts:" << std::endl;
    std::cout << "  signal_strength_check_timeout: " << signal_strength_check_timeout << "ms" << std::endl;
    std::cout << "  registration_check_timeout: " << registration_check_timeout << "ms" << std::endl;
    std::cout << "  data_bearer_check_timeout: " << data_bearer_check_timeout << "ms" << std::endl;
    std::cout << "  interface_status_check_timeout: " << interface_status_check_timeout << "ms" << std::endl;
    
    std::cout << "\nRecovery Engine Timeouts:" << std::endl;
    std::cout << "  modem_reset_timeout: " << modem_reset_timeout << "ms" << std::endl;
    std::cout << "  network_rescan_timeout: " << network_rescan_timeout << "ms" << std::endl;
    std::cout << "  reconnect_attempt_timeout: " << reconnect_attempt_timeout << "ms" << std::endl;
    std::cout << "  recovery_operation_timeout: " << recovery_operation_timeout << "ms" << std::endl;
    
    std::cout << "\nGeneral Operation Timeouts:" << std::endl;
    std::cout << "  command_execution_timeout: " << command_execution_timeout << "ms" << std::endl;
    std::cout << "  state_transition_timeout: " << state_transition_timeout << "ms" << std::endl;
    std::cout << "  monitoring_interval: " << monitoring_interval << "ms" << std::endl;
    std::cout << "  retry_delay: " << retry_delay << "ms" << std::endl;
    
    std::cout << "======================================\n" << std::endl;
}

bool TimeoutConfig::validateTimeouts() const {
    bool valid = true;
    
    // Check all timeout values are positive and within reasonable limits
    const int min_timeout = 100;        // 100ms minimum
    const int max_timeout = 300000;     // 5 minutes maximum
    
    auto temp_config = const_cast<TimeoutConfig*>(this);
    temp_config->initializeTimeoutMap();
    
    for (const auto& pair : timeout_map) {
        int value = *(pair.second);
        if (value < min_timeout || value > max_timeout) {
            std::cerr << "Warning: Timeout " << pair.first << " (" << value 
                      << "ms) is outside reasonable range [" << min_timeout 
                      << "ms, " << max_timeout << "ms]" << std::endl;
            valid = false;
        }
    }
    
    return valid;
}