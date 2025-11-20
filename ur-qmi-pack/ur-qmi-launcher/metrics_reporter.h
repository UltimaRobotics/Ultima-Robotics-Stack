
#ifndef METRICS_REPORTER_H
#define METRICS_REPORTER_H

#include "connection_manager.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <json/json.h>

struct DetailedMetrics {
    // Connection metrics
    std::chrono::system_clock::time_point timestamp;
    bool is_connected;
    std::chrono::milliseconds connection_duration;
    int connection_attempts;
    int successful_connections;
    int failed_connections;
    
    // Signal metrics
    int signal_strength;
    int signal_quality;
    std::string network_type;
    std::string band;
    std::string carrier;
    
    // Data metrics
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    double throughput_up_kbps;
    double throughput_down_kbps;
    
    // Network metrics
    std::string ip_address;
    std::string gateway;
    std::string dns_primary;
    std::string dns_secondary;
    int ping_latency_ms;
    double packet_loss_percent;
    
    // Error metrics
    int session_errors;
    int ip_config_errors;
    int dns_errors;
    int connectivity_errors;
    int recovery_attempts;
    int successful_recoveries;
};

using DetailedMetricsCallback = std::function<void(const DetailedMetrics&)>;

class QMISessionHandler;
class InterfaceController;
class ConnectivityMonitor;

class MetricsReporter {
public:
    MetricsReporter(QMISessionHandler* session_handler,
                   InterfaceController* interface_controller,
                   ConnectivityMonitor* connectivity_monitor);
    ~MetricsReporter();
    
    // Reporting control
    void startReporting(int interval_ms = 60000);
    void stopReporting();
    bool isReporting() const;
    
    // Configuration
    void setReportingInterval(int interval_ms);
    void setMetricsCallback(DetailedMetricsCallback callback);
    void enableFileLogging(const std::string& filename);
    void disableFileLogging();
    void enableConsoleOutput(bool enable);
    
    // Manual reporting
    DetailedMetrics collectCurrentMetrics();
    void reportMetrics(const DetailedMetrics& metrics);
    void resetCounters();
    
    // Statistics
    std::vector<DetailedMetrics> getMetricsHistory(int count = 100) const;
    DetailedMetrics getAverageMetrics(std::chrono::minutes window) const;
    Json::Value getMetricsAsJson(const DetailedMetrics& metrics) const;
    std::string getMetricsReport() const;
    
    // Event tracking
    void incrementConnectionAttempt();
    void incrementSuccessfulConnection();
    void incrementFailedConnection();
    void incrementSessionError();
    void incrementIPConfigError();
    void incrementDNSError();
    void incrementConnectivityError();
    void incrementRecoveryAttempt();
    void incrementSuccessfulRecovery();

private:
    void reportingLoop();
    void performPeriodicCollection();
    void writeToFile(const DetailedMetrics& metrics);
    void writeToConsole(const DetailedMetrics& metrics);
    double calculateThroughput(uint64_t bytes, std::chrono::milliseconds duration);
    
    QMISessionHandler* m_session_handler;
    InterfaceController* m_interface_controller;
    ConnectivityMonitor* m_connectivity_monitor;
    
    std::vector<DetailedMetrics> m_metrics_history;
    DetailedMetrics m_last_metrics;
    
    // Counters
    std::atomic<int> m_connection_attempts;
    std::atomic<int> m_successful_connections;
    std::atomic<int> m_failed_connections;
    std::atomic<int> m_session_errors;
    std::atomic<int> m_ip_config_errors;
    std::atomic<int> m_dns_errors;
    std::atomic<int> m_connectivity_errors;
    std::atomic<int> m_recovery_attempts;
    std::atomic<int> m_successful_recoveries;
    
    std::unique_ptr<std::thread> m_reporting_thread;
    std::atomic<bool> m_reporting;
    std::atomic<int> m_reporting_interval_ms;
    std::atomic<bool> m_console_output_enabled;
    
    DetailedMetricsCallback m_metrics_callback;
    
    std::unique_ptr<std::ofstream> m_log_file;
    std::string m_log_filename;
    bool m_file_logging_enabled;
    
    mutable std::mutex m_history_mutex;
    mutable std::mutex m_file_mutex;
    std::condition_variable m_reporting_cv;
    
    std::chrono::system_clock::time_point m_start_time;
    std::chrono::system_clock::time_point m_connection_start_time;
    
    static const size_t MAX_HISTORY_SIZE = 1000;
};

#endif // METRICS_REPORTER_H
