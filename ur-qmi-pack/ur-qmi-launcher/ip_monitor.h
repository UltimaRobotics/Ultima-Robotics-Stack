
#ifndef IP_MONITOR_H
#define IP_MONITOR_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <condition_variable>
#include <json/json.h>

// Forward declarations
class QMISessionHandler;

struct PingResult {
    std::string target_ip;
    std::string interface_name;
    bool success;
    double response_time_ms;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;
    
    std::string toJson() const;
};

struct ModemReferenceData {
    std::string device_path;
    std::string imei;
    std::string signal_strength;
    std::string network_type;
    std::string ip_address;
    std::string gateway;
    std::string dns_primary;
    std::string dns_secondary;
    std::string interface_name;
    bool is_connected;
    std::chrono::system_clock::time_point data_timestamp;
    
    std::string toJson() const;
};

struct IPMonitorConfig {
    std::vector<std::string> ping_targets;
    int ping_interval_ms;
    int ping_timeout_ms;
    bool enable_monitoring;
    std::string log_format;
    bool include_modem_data;
    
    IPMonitorConfig() : 
        ping_interval_ms(5000),
        ping_timeout_ms(3000),
        enable_monitoring(true),
        log_format("json"),
        include_modem_data(true) {}
};

struct IPMonitorReport {
    std::vector<PingResult> ping_results;
    ModemReferenceData modem_data;
    std::chrono::system_clock::time_point report_timestamp;
    int successful_pings;
    int total_pings;
    double average_response_time;
    
    std::string toJson() const;
};

using IPMonitorCallback = std::function<void(const IPMonitorReport&)>;

class IPMonitor {
public:
    IPMonitor();
    ~IPMonitor();
    
    // Configuration
    bool loadConfigFromFile(const std::string& config_file_path);
    bool loadConfigFromJson(const Json::Value& config);
    void setConfig(const IPMonitorConfig& config);
    IPMonitorConfig getConfig() const;
    
    // Monitoring control
    bool startMonitoring(const std::string& interface_name, QMISessionHandler* session_handler);
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Manual ping operations
    PingResult performPing(const std::string& target_ip, const std::string& interface_name);
    std::vector<PingResult> performAllPings(const std::string& interface_name);
    
    // Modem data collection
    ModemReferenceData collectModemData(QMISessionHandler* session_handler, const std::string& interface_name);
    
    // Report generation
    IPMonitorReport generateReport(const std::string& interface_name, QMISessionHandler* session_handler);
    
    // Callbacks
    void setMonitorCallback(IPMonitorCallback callback);
    
    // Statistics
    struct MonitoringStats {
        uint64_t total_reports = 0;
        uint64_t total_pings = 0;
        uint64_t successful_pings = 0;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_report_time;
        
        std::string toJson() const;
    };
    
    MonitoringStats getStats() const;
    void resetStats();

private:
    void monitoringLoop();
    void logReportToTerminal(const IPMonitorReport& report);
    std::string executeCommandWithOutput(const std::string& command);
    double parseResponseTime(const std::string& ping_output);
    std::string getCurrentTimestamp() const;
    
    // Configuration
    IPMonitorConfig m_config;
    mutable std::mutex m_config_mutex;
    
    // Monitoring state
    std::atomic<bool> m_monitoring;
    std::unique_ptr<std::thread> m_monitor_thread;
    std::string m_interface_name;
    QMISessionHandler* m_session_handler;
    
    // Statistics
    MonitoringStats m_stats;
    mutable std::mutex m_stats_mutex;
    
    // Callbacks
    IPMonitorCallback m_monitor_callback;
    
    // Threading
    mutable std::mutex m_monitoring_mutex;
    std::condition_variable m_monitor_cv;
};

#endif // IP_MONITOR_H
