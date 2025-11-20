#ifndef TIMEOUT_CONFIG_H
#define TIMEOUT_CONFIG_H

#include <string>
#include <unordered_map>

/**
 * @brief Structure to hold timeout configurations for different operations
 */
struct TimeoutConfig {
    // QMI Session timeouts (in milliseconds)
    int qmi_device_open_timeout = 10000;           // 10 seconds
    int qmi_start_network_timeout = 30000;         // 30 seconds
    int qmi_stop_network_timeout = 15000;          // 15 seconds
    int qmi_get_status_timeout = 5000;             // 5 seconds
    int qmi_get_device_info_timeout = 5000;        // 5 seconds
    int qmi_set_operating_mode_timeout = 20000;    // 20 seconds
    
    // Interface Controller timeouts
    int dhcp_timeout = 30000;                      // 30 seconds
    int interface_up_timeout = 10000;              // 10 seconds
    int interface_down_timeout = 10000;            // 10 seconds
    int ip_config_timeout = 15000;                 // 15 seconds
    int dns_config_timeout = 10000;                // 10 seconds
    int routing_timeout = 15000;                   // 15 seconds
    
    // Connectivity Monitor timeouts
    int ping_timeout = 5000;                       // 5 seconds
    int dns_resolution_timeout = 10000;            // 10 seconds
    int http_test_timeout = 30000;                 // 30 seconds
    int connectivity_check_interval = 30000;       // 30 seconds
    
    // Failure Detector timeouts
    int signal_strength_check_timeout = 5000;      // 5 seconds
    int registration_check_timeout = 10000;        // 10 seconds
    int data_bearer_check_timeout = 5000;          // 5 seconds
    int interface_status_check_timeout = 5000;     // 5 seconds
    
    // Recovery Engine timeouts
    int modem_reset_timeout = 60000;               // 60 seconds
    int network_rescan_timeout = 45000;            // 45 seconds
    int reconnect_attempt_timeout = 30000;         // 30 seconds
    int recovery_operation_timeout = 120000;       // 120 seconds
    
    // General operation timeouts
    int command_execution_timeout = 30000;         // 30 seconds
    int state_transition_timeout = 15000;          // 15 seconds
    int monitoring_interval = 10000;               // 10 seconds
    int retry_delay = 5000;                        // 5 seconds
    
    /**
     * @brief Load timeout configuration from JSON file
     * @param config_file Path to the JSON configuration file
     * @return true if configuration loaded successfully, false otherwise
     */
    bool loadFromFile(const std::string& config_file);
    
    /**
     * @brief Save current configuration to JSON file
     * @param config_file Path to the JSON configuration file
     * @return true if configuration saved successfully, false otherwise
     */
    bool saveToFile(const std::string& config_file) const;
    
    /**
     * @brief Get timeout value by name
     * @param timeout_name Name of the timeout parameter
     * @return Timeout value in milliseconds, or -1 if not found
     */
    int getTimeout(const std::string& timeout_name) const;
    
    /**
     * @brief Set timeout value by name
     * @param timeout_name Name of the timeout parameter
     * @param timeout_value Timeout value in milliseconds
     * @return true if timeout was set successfully, false otherwise
     */
    bool setTimeout(const std::string& timeout_name, int timeout_value);
    
    /**
     * @brief Print current timeout configuration
     */
    void printConfiguration() const;
    
    /**
     * @brief Validate timeout values (ensure they are positive and within reasonable limits)
     * @return true if all timeouts are valid, false otherwise
     */
    bool validateTimeouts() const;

private:
    /**
     * @brief Internal mapping of timeout names to their values
     */
    std::unordered_map<std::string, int*> timeout_map;
    
    /**
     * @brief Initialize the timeout mapping
     */
    void initializeTimeoutMap();
};

/**
 * @brief Global timeout configuration instance
 */
extern TimeoutConfig g_timeout_config;

#endif // TIMEOUT_CONFIG_H