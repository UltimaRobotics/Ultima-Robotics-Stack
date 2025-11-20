
#include "metrics_reporter.h"
#include "qmi_session_handler.h"
#include "interface_controller.h"
#include "connectivity_monitor.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <ctime>
#include <algorithm>

MetricsReporter::MetricsReporter(QMISessionHandler* session_handler,
                                InterfaceController* interface_controller,
                                ConnectivityMonitor* connectivity_monitor)
    : m_session_handler(session_handler)
    , m_interface_controller(interface_controller)
    , m_connectivity_monitor(connectivity_monitor)
    , m_connection_attempts(0)
    , m_successful_connections(0)
    , m_failed_connections(0)
    , m_session_errors(0)
    , m_ip_config_errors(0)
    , m_dns_errors(0)
    , m_connectivity_errors(0)
    , m_recovery_attempts(0)
    , m_successful_recoveries(0)
    , m_reporting(false)
    , m_reporting_interval_ms(60000)
    , m_console_output_enabled(false)
    , m_file_logging_enabled(false)
{
    m_start_time = std::chrono::system_clock::now();
    m_connection_start_time = m_start_time;
    
    // Initialize last metrics
    m_last_metrics = {};
    m_last_metrics.timestamp = m_start_time;
}

MetricsReporter::~MetricsReporter() {
    stopReporting();
    
    if (m_log_file && m_log_file->is_open()) {
        m_log_file->close();
    }
}

void MetricsReporter::startReporting(int interval_ms) {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_reporting) {
        return;
    }
    
    m_reporting_interval_ms = interval_ms;
    m_reporting = true;
    
    m_reporting_thread = std::make_unique<std::thread>(&MetricsReporter::reportingLoop, this);
    std::cout << "Metrics reporting started (interval: " << interval_ms << "ms)" << std::endl;
}

void MetricsReporter::stopReporting() {
    {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        m_reporting = false;
    }
    
    m_reporting_cv.notify_all();
    
    if (m_reporting_thread && m_reporting_thread->joinable()) {
        m_reporting_thread->join();
    }
    
    std::cout << "Metrics reporting stopped" << std::endl;
}

bool MetricsReporter::isReporting() const {
    return m_reporting;
}

void MetricsReporter::setReportingInterval(int interval_ms) {
    m_reporting_interval_ms = interval_ms;
}

void MetricsReporter::setMetricsCallback(DetailedMetricsCallback callback) {
    m_metrics_callback = callback;
}

void MetricsReporter::enableFileLogging(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_file_mutex);
    
    if (m_log_file && m_log_file->is_open()) {
        m_log_file->close();
    }
    
    m_log_filename = filename;
    m_log_file = std::make_unique<std::ofstream>(filename, std::ios::app);
    
    if (m_log_file->is_open()) {
        m_file_logging_enabled = true;
        std::cout << "File logging enabled: " << filename << std::endl;
    } else {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        m_file_logging_enabled = false;
    }
}

void MetricsReporter::disableFileLogging() {
    std::lock_guard<std::mutex> lock(m_file_mutex);
    
    if (m_log_file && m_log_file->is_open()) {
        m_log_file->close();
    }
    
    m_file_logging_enabled = false;
    std::cout << "File logging disabled" << std::endl;
}

void MetricsReporter::enableConsoleOutput(bool enable) {
    m_console_output_enabled = enable;
    std::cout << "Console output " << (enable ? "enabled" : "disabled") << std::endl;
}

DetailedMetrics MetricsReporter::collectCurrentMetrics() {
    DetailedMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    
    // Initialize with defaults
    metrics.is_connected = false;
    metrics.connection_duration = std::chrono::milliseconds(0);
    metrics.connection_attempts = m_connection_attempts;
    metrics.successful_connections = m_successful_connections;
    metrics.failed_connections = m_failed_connections;
    metrics.signal_strength = -999;
    metrics.signal_quality = 0;
    metrics.network_type = "Unknown";
    metrics.band = "Unknown";
    metrics.carrier = "Unknown";
    metrics.bytes_sent = 0;
    metrics.bytes_received = 0;
    metrics.total_bytes_sent = 0;
    metrics.total_bytes_received = 0;
    metrics.throughput_up_kbps = 0.0;
    metrics.throughput_down_kbps = 0.0;
    metrics.ip_address = "";
    metrics.gateway = "";
    metrics.dns_primary = "";
    metrics.dns_secondary = "";
    metrics.ping_latency_ms = -1;
    metrics.packet_loss_percent = 0.0;
    metrics.session_errors = m_session_errors;
    metrics.ip_config_errors = m_ip_config_errors;
    metrics.dns_errors = m_dns_errors;
    metrics.connectivity_errors = m_connectivity_errors;
    metrics.recovery_attempts = m_recovery_attempts;
    metrics.successful_recoveries = m_successful_recoveries;
    
    // Collect session-related metrics
    if (m_session_handler) {
        try {
            metrics.is_connected = m_session_handler->isSessionActive();
            
            if (metrics.is_connected) {
                auto now = std::chrono::system_clock::now();
                metrics.connection_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - m_connection_start_time);
                
                // Use cached session info instead of calling QMI services
                auto session_info = m_session_handler->getSessionInfo();
                metrics.ip_address = session_info.ip_address;
                metrics.gateway = session_info.gateway;
                metrics.dns_primary = session_info.dns_primary;
                metrics.dns_secondary = session_info.dns_secondary;
                
                // Set default values for signal info to avoid QMI calls
                metrics.signal_strength = -999;
                metrics.signal_quality = 0;
                metrics.network_type = "Unknown";
                metrics.band = "Unknown";
                metrics.carrier = "Unknown";
                
                // Get interface statistics if available - use a default interface name or skip if not available
                if (m_interface_controller) {
                    // Try to get interface status from the first available WWAN interface
                    auto wwan_interfaces = m_interface_controller->findWWANInterfaces();
                    if (!wwan_interfaces.empty()) {
                        auto interface_status = m_interface_controller->getInterfaceStatus(wwan_interfaces[0]);
                    metrics.bytes_sent = interface_status.bytes_sent;
                    metrics.bytes_received = interface_status.bytes_received;
                    
                    // Calculate throughput since last measurement
                    if (!m_last_metrics.timestamp.time_since_epoch().count()) {
                        // First measurement
                        metrics.total_bytes_sent = metrics.bytes_sent;
                        metrics.total_bytes_received = metrics.bytes_received;
                    } else {
                        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                            metrics.timestamp - m_last_metrics.timestamp);
                        
                        if (time_diff.count() > 0) {
                            uint64_t bytes_sent_diff = metrics.bytes_sent - m_last_metrics.bytes_sent;
                            uint64_t bytes_recv_diff = metrics.bytes_received - m_last_metrics.bytes_received;
                            
                            metrics.throughput_up_kbps = calculateThroughput(bytes_sent_diff, time_diff);
                            metrics.throughput_down_kbps = calculateThroughput(bytes_recv_diff, time_diff);
                        }
                        
                        metrics.total_bytes_sent = m_last_metrics.total_bytes_sent + 
                                                  (metrics.bytes_sent - m_last_metrics.bytes_sent);
                        metrics.total_bytes_received = m_last_metrics.total_bytes_received + 
                                                      (metrics.bytes_received - m_last_metrics.bytes_received);
                    }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error collecting session metrics: " << e.what() << std::endl;
        }
    }
    
    // Collect connectivity metrics
    if (m_connectivity_monitor) {
        try {
            if (metrics.is_connected) {
                // Test ping latency
                auto ping_test = m_connectivity_monitor->pingTest("8.8.8.8", 5000);
                if (ping_test.success) {
                    metrics.ping_latency_ms = ping_test.response_time_ms;
                }
                
                // Calculate packet loss from recent tests
                auto recent_tests = m_connectivity_monitor->getRecentTests(10);
                if (!recent_tests.empty()) {
                    int failed_tests = std::count_if(recent_tests.begin(), recent_tests.end(),
                                                    [](const ConnectivityTest& test) {
                                                        return !test.success;
                                                    });
                    metrics.packet_loss_percent = static_cast<double>(failed_tests) / recent_tests.size() * 100.0;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error collecting connectivity metrics: " << e.what() << std::endl;
        }
    }
    
    return metrics;
}

void MetricsReporter::reportMetrics(const DetailedMetrics& metrics) {
    // Store in history
    {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        m_metrics_history.push_back(metrics);
        
        if (m_metrics_history.size() > MAX_HISTORY_SIZE) {
            m_metrics_history.erase(m_metrics_history.begin());
        }
    }
    
    // Update last metrics
    m_last_metrics = metrics;
    
    // Write to file if enabled
    if (m_file_logging_enabled) {
        writeToFile(metrics);
    }
    
    // Write to console if enabled
    if (m_console_output_enabled) {
        writeToConsole(metrics);
    }
    
    // Call callback if set
    if (m_metrics_callback) {
        m_metrics_callback(metrics);
    }
}

void MetricsReporter::resetCounters() {
    m_connection_attempts = 0;
    m_successful_connections = 0;
    m_failed_connections = 0;
    m_session_errors = 0;
    m_ip_config_errors = 0;
    m_dns_errors = 0;
    m_connectivity_errors = 0;
    m_recovery_attempts = 0;
    m_successful_recoveries = 0;
    
    m_start_time = std::chrono::system_clock::now();
    m_connection_start_time = m_start_time;
    
    std::cout << "Metrics counters reset" << std::endl;
}

std::vector<DetailedMetrics> MetricsReporter::getMetricsHistory(int count) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_metrics_history.size() <= static_cast<size_t>(count)) {
        return m_metrics_history;
    }
    
    return std::vector<DetailedMetrics>(
        m_metrics_history.end() - count, m_metrics_history.end());
}

DetailedMetrics MetricsReporter::getAverageMetrics(std::chrono::minutes window) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto window_start = now - window;
    
    std::vector<DetailedMetrics> window_metrics;
    for (const auto& metrics : m_metrics_history) {
        if (metrics.timestamp >= window_start) {
            window_metrics.push_back(metrics);
        }
    }
    
    if (window_metrics.empty()) {
        return DetailedMetrics{}; // Return empty metrics
    }
    
    DetailedMetrics avg_metrics = {};
    avg_metrics.timestamp = now;
    
    // Calculate averages
    int connected_count = 0;
    int total_signal_strength = 0;
    int signal_count = 0;
    double total_throughput_up = 0.0;
    double total_throughput_down = 0.0;
    int total_ping = 0;
    int ping_count = 0;
    
    for (const auto& metrics : window_metrics) {
        if (metrics.is_connected) {
            connected_count++;
        }
        
        if (metrics.signal_strength > -999) {
            total_signal_strength += metrics.signal_strength;
            signal_count++;
        }
        
        total_throughput_up += metrics.throughput_up_kbps;
        total_throughput_down += metrics.throughput_down_kbps;
        
        if (metrics.ping_latency_ms > 0) {
            total_ping += metrics.ping_latency_ms;
            ping_count++;
        }
    }
    
    avg_metrics.is_connected = (static_cast<size_t>(connected_count) > window_metrics.size() / 2);
    avg_metrics.signal_strength = signal_count > 0 ? total_signal_strength / signal_count : -999;
    avg_metrics.throughput_up_kbps = total_throughput_up / window_metrics.size();
    avg_metrics.throughput_down_kbps = total_throughput_down / window_metrics.size();
    avg_metrics.ping_latency_ms = ping_count > 0 ? total_ping / ping_count : -1;
    
    return avg_metrics;
}

Json::Value MetricsReporter::getMetricsAsJson(const DetailedMetrics& metrics) const {
    Json::Value json;
    
    // Timestamp
    auto time_t = std::chrono::system_clock::to_time_t(metrics.timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json["timestamp"] = ss.str();
    
    // Connection metrics
    json["connection"]["is_connected"] = metrics.is_connected;
    json["connection"]["duration_ms"] = static_cast<Json::Int64>(metrics.connection_duration.count());
    json["connection"]["attempts"] = static_cast<int>(m_connection_attempts);
    json["connection"]["successful"] = static_cast<int>(m_successful_connections);
    json["connection"]["failed"] = static_cast<int>(m_failed_connections);
    
    // Signal metrics
    json["signal"]["strength_dbm"] = metrics.signal_strength;
    json["signal"]["quality"] = metrics.signal_quality;
    json["signal"]["network_type"] = metrics.network_type;
    json["signal"]["band"] = metrics.band;
    json["signal"]["carrier"] = metrics.carrier;
    
    // Data metrics
    json["data"]["bytes_sent"] = static_cast<Json::UInt64>(metrics.bytes_sent);
    json["data"]["bytes_received"] = static_cast<Json::UInt64>(metrics.bytes_received);
    json["data"]["total_bytes_sent"] = static_cast<Json::UInt64>(metrics.total_bytes_sent);
    json["data"]["total_bytes_received"] = static_cast<Json::UInt64>(metrics.total_bytes_received);
    json["data"]["throughput_up_kbps"] = metrics.throughput_up_kbps;
    json["data"]["throughput_down_kbps"] = metrics.throughput_down_kbps;
    
    // Network metrics
    json["network"]["ip_address"] = metrics.ip_address;
    json["network"]["gateway"] = metrics.gateway;
    json["network"]["dns_primary"] = metrics.dns_primary;
    json["network"]["dns_secondary"] = metrics.dns_secondary;
    json["network"]["ping_latency_ms"] = metrics.ping_latency_ms;
    json["network"]["packet_loss_percent"] = metrics.packet_loss_percent;
    
    // Error metrics
    json["errors"]["session_errors"] = static_cast<int>(m_session_errors);
    json["errors"]["ip_config_errors"] = static_cast<int>(m_ip_config_errors);
    json["errors"]["dns_errors"] = static_cast<int>(m_dns_errors);
    json["errors"]["connectivity_errors"] = static_cast<int>(m_connectivity_errors);
    json["errors"]["recovery_attempts"] = static_cast<int>(m_recovery_attempts);
    json["errors"]["successful_recoveries"] = static_cast<int>(m_successful_recoveries);
    
    return json;
}

std::string MetricsReporter::getMetricsReport() const {
    auto metrics = m_last_metrics;
    std::stringstream report;
    
    report << "=== CONNECTION METRICS REPORT ===" << std::endl;
    report << "Status: " << (metrics.is_connected ? "CONNECTED" : "DISCONNECTED") << std::endl;
    
    if (metrics.is_connected) {
        report << "Connection Duration: " << metrics.connection_duration.count() / 1000.0 << " seconds" << std::endl;
        report << "Signal Strength: " << metrics.signal_strength << " dBm" << std::endl;
        report << "Network Type: " << metrics.network_type << std::endl;
        report << "IP Address: " << metrics.ip_address << std::endl;
        report << "Gateway: " << metrics.gateway << std::endl;
        
        if (metrics.ping_latency_ms > 0) {
            report << "Ping Latency: " << metrics.ping_latency_ms << " ms" << std::endl;
        }
        
        report << "Data Sent: " << metrics.total_bytes_sent / 1024.0 << " KB" << std::endl;
        report << "Data Received: " << metrics.total_bytes_received / 1024.0 << " KB" << std::endl;
        report << "Upload Speed: " << metrics.throughput_up_kbps << " Kbps" << std::endl;
        report << "Download Speed: " << metrics.throughput_down_kbps << " Kbps" << std::endl;
    }
    
    report << "Connection Attempts: " << m_connection_attempts << std::endl;
    report << "Successful Connections: " << m_successful_connections << std::endl;
    report << "Failed Connections: " << m_failed_connections << std::endl;
    report << "Total Errors: " << (m_session_errors + m_ip_config_errors + 
                                   m_dns_errors + m_connectivity_errors) << std::endl;
    report << "Recovery Attempts: " << m_recovery_attempts << std::endl;
    report << "Successful Recoveries: " << m_successful_recoveries << std::endl;
    
    return report.str();
}

void MetricsReporter::incrementConnectionAttempt() { m_connection_attempts++; }
void MetricsReporter::incrementSuccessfulConnection() { 
    m_successful_connections++; 
    m_connection_start_time = std::chrono::system_clock::now();
}
void MetricsReporter::incrementFailedConnection() { m_failed_connections++; }
void MetricsReporter::incrementSessionError() { m_session_errors++; }
void MetricsReporter::incrementIPConfigError() { m_ip_config_errors++; }
void MetricsReporter::incrementDNSError() { m_dns_errors++; }
void MetricsReporter::incrementConnectivityError() { m_connectivity_errors++; }
void MetricsReporter::incrementRecoveryAttempt() { m_recovery_attempts++; }
void MetricsReporter::incrementSuccessfulRecovery() { m_successful_recoveries++; }

void MetricsReporter::reportingLoop() {
    while (m_reporting) {
        performPeriodicCollection();
        
        std::unique_lock<std::mutex> lock(m_history_mutex);
        m_reporting_cv.wait_for(lock, std::chrono::milliseconds(m_reporting_interval_ms),
                               [this] { return !m_reporting; });
    }
}

void MetricsReporter::performPeriodicCollection() {
    auto metrics = collectCurrentMetrics();
    reportMetrics(metrics);
}

void MetricsReporter::writeToFile(const DetailedMetrics& metrics) {
    std::lock_guard<std::mutex> lock(m_file_mutex);
    
    if (!m_log_file || !m_log_file->is_open()) {
        return;
    }
    
    Json::Value json = getMetricsAsJson(metrics);
    *m_log_file << json << std::endl;
    m_log_file->flush();
}

void MetricsReporter::writeToConsole(const DetailedMetrics& metrics) {
    std::cout << "=== METRICS ===" << std::endl;
    std::cout << "Connected: " << (metrics.is_connected ? "YES" : "NO") << std::endl;
    
    if (metrics.is_connected) {
        std::cout << "Signal: " << metrics.signal_strength << " dBm" << std::endl;
        std::cout << "Network: " << metrics.network_type << std::endl;
        std::cout << "IP: " << metrics.ip_address << std::endl;
        std::cout << "Up: " << std::fixed << std::setprecision(2) 
                  << metrics.throughput_up_kbps << " Kbps" << std::endl;
        std::cout << "Down: " << std::fixed << std::setprecision(2) 
                  << metrics.throughput_down_kbps << " Kbps" << std::endl;
    }
    
    std::cout << "Errors: " << (m_session_errors + m_ip_config_errors + 
                                m_dns_errors + m_connectivity_errors) << std::endl;
    std::cout << "===============" << std::endl;
}

double MetricsReporter::calculateThroughput(uint64_t bytes, std::chrono::milliseconds duration) {
    if (duration.count() == 0) {
        return 0.0;
    }
    
    double seconds = duration.count() / 1000.0;
    double kbps = (bytes * 8.0) / (seconds * 1000.0); // Convert to Kbps
    return kbps;
}
