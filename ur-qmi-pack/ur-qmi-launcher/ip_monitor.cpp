
#include "ip_monitor.h"
#include "qmi_session_handler.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <cstdio>
#include <unistd.h>

std::string PingResult::toJson() const {
    Json::Value json;
    json["target_ip"] = target_ip;
    json["interface_name"] = interface_name;
    json["success"] = success;
    json["response_time_ms"] = response_time_ms;
    json["error_message"] = error_message;
    
    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json["timestamp"] = ss.str();
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, json);
}

std::string ModemReferenceData::toJson() const {
    Json::Value json;
    json["device_path"] = device_path;
    json["imei"] = imei;
    json["signal_strength"] = signal_strength;
    json["network_type"] = network_type;
    json["ip_address"] = ip_address;
    json["gateway"] = gateway;
    json["dns_primary"] = dns_primary;
    json["dns_secondary"] = dns_secondary;
    json["interface_name"] = interface_name;
    json["is_connected"] = is_connected;
    
    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(data_timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json["data_timestamp"] = ss.str();
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, json);
}

std::string IPMonitorReport::toJson() const {
    Json::Value json;
    
    // Report metadata
    auto time_t = std::chrono::system_clock::to_time_t(report_timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json["report_timestamp"] = ss.str();
    json["successful_pings"] = successful_pings;
    json["total_pings"] = total_pings;
    json["average_response_time_ms"] = average_response_time;
    
    // Ping results
    Json::Value ping_array(Json::arrayValue);
    for (const auto& ping : ping_results) {
        Json::Value ping_json;
        Json::Reader reader;
        reader.parse(ping.toJson(), ping_json);
        ping_array.append(ping_json);
    }
    json["ping_results"] = ping_array;
    
    // Modem data
    Json::Value modem_json;
    Json::Reader reader;
    reader.parse(modem_data.toJson(), modem_json);
    json["modem_reference_data"] = modem_json;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    return Json::writeString(builder, json);
}

std::string IPMonitor::MonitoringStats::toJson() const {
    Json::Value json;
    json["total_reports"] = static_cast<Json::UInt64>(total_reports);
    json["total_pings"] = static_cast<Json::UInt64>(total_pings);
    json["successful_pings"] = static_cast<Json::UInt64>(successful_pings);
    
    auto start_time_t = std::chrono::system_clock::to_time_t(start_time);
    auto last_time_t = std::chrono::system_clock::to_time_t(last_report_time);
    
    std::stringstream ss1, ss2;
    ss1 << std::put_time(std::gmtime(&start_time_t), "%Y-%m-%dT%H:%M:%SZ");
    ss2 << std::put_time(std::gmtime(&last_time_t), "%Y-%m-%dT%H:%M:%SZ");
    
    json["start_time"] = ss1.str();
    json["last_report_time"] = ss2.str();
    
    double success_rate = total_pings > 0 ? (static_cast<double>(successful_pings) / total_pings * 100.0) : 0.0;
    json["success_rate_percent"] = success_rate;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, json);
}

IPMonitor::IPMonitor() 
    : m_monitoring(false), m_session_handler(nullptr) {
}

IPMonitor::~IPMonitor() {
    stopMonitoring();
}

bool IPMonitor::loadConfigFromFile(const std::string& config_file_path) {
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open IP monitor config file: " << config_file_path << std::endl;
        return false;
    }
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(file, root)) {
        std::cerr << "Failed to parse IP monitor config JSON: " << reader.getFormattedErrorMessages() << std::endl;
        return false;
    }
    
    return loadConfigFromJson(root);
}

bool IPMonitor::loadConfigFromJson(const Json::Value& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    
    try {
        if (config.isMember("ping_targets") && config["ping_targets"].isArray()) {
            m_config.ping_targets.clear();
            for (const auto& target : config["ping_targets"]) {
                if (target.isString()) {
                    m_config.ping_targets.push_back(target.asString());
                }
            }
        }
        
        if (config.isMember("ping_interval_ms")) {
            m_config.ping_interval_ms = config["ping_interval_ms"].asInt();
        }
        
        if (config.isMember("ping_timeout_ms")) {
            m_config.ping_timeout_ms = config["ping_timeout_ms"].asInt();
        }
        
        if (config.isMember("enable_monitoring")) {
            m_config.enable_monitoring = config["enable_monitoring"].asBool();
        }
        
        if (config.isMember("log_format")) {
            m_config.log_format = config["log_format"].asString();
        }
        
        if (config.isMember("include_modem_data")) {
            m_config.include_modem_data = config["include_modem_data"].asBool();
        }
        
        std::cout << "IP Monitor configuration loaded successfully:" << std::endl;
        std::cout << "  Targets: " << m_config.ping_targets.size() << std::endl;
        std::cout << "  Interval: " << m_config.ping_interval_ms << "ms" << std::endl;
        std::cout << "  Timeout: " << m_config.ping_timeout_ms << "ms" << std::endl;
        std::cout << "  Monitoring enabled: " << (m_config.enable_monitoring ? "true" : "false") << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading IP monitor config: " << e.what() << std::endl;
        return false;
    }
}

void IPMonitor::setConfig(const IPMonitorConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
}

IPMonitorConfig IPMonitor::getConfig() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

bool IPMonitor::startMonitoring(const std::string& interface_name, QMISessionHandler* session_handler) {
    std::lock_guard<std::mutex> lock(m_monitoring_mutex);
    
    if (m_monitoring) {
        std::cout << "IP monitoring already running" << std::endl;
        return false;
    }
    
    if (!session_handler) {
        std::cerr << "Invalid session handler for IP monitoring" << std::endl;
        return false;
    }
    
    if (interface_name.empty()) {
        std::cerr << "Invalid interface name for IP monitoring" << std::endl;
        return false;
    }
    
    // Check if monitoring is enabled in config
    {
        std::lock_guard<std::mutex> config_lock(m_config_mutex);
        if (!m_config.enable_monitoring) {
            std::cout << "IP monitoring is disabled in configuration" << std::endl;
            return false;
        }
        
        if (m_config.ping_targets.empty()) {
            std::cout << "No ping targets configured, IP monitoring disabled" << std::endl;
            return false;
        }
    }
    
    m_interface_name = interface_name;
    m_session_handler = session_handler;
    m_monitoring = true;
    
    // Reset statistics
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_stats = MonitoringStats();
        m_stats.start_time = std::chrono::system_clock::now();
    }
    
    m_monitor_thread = std::make_unique<std::thread>(&IPMonitor::monitoringLoop, this);
    
    std::cout << "IP monitoring started for interface: " << interface_name << std::endl;
    return true;
}

void IPMonitor::stopMonitoring() {
    {
        std::lock_guard<std::mutex> lock(m_monitoring_mutex);
        m_monitoring = false;
    }
    
    m_monitor_cv.notify_all();
    
    if (m_monitor_thread && m_monitor_thread->joinable()) {
        m_monitor_thread->join();
    }
    
    std::cout << "IP monitoring stopped" << std::endl;
}

bool IPMonitor::isMonitoring() const {
    return m_monitoring;
}

PingResult IPMonitor::performPing(const std::string& target_ip, const std::string& interface_name) {
    PingResult result;
    result.target_ip = target_ip;
    result.interface_name = interface_name;
    result.timestamp = std::chrono::system_clock::now();
    result.success = false;
    result.response_time_ms = -1.0;
    
    int timeout_seconds;
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        timeout_seconds = m_config.ping_timeout_ms / 1000;
        if (timeout_seconds < 1) timeout_seconds = 1;
    }
    
    // Construct ping command
    std::stringstream cmd;
    cmd << "ping -c 1 -W " << timeout_seconds << " -I " << interface_name << " " << target_ip << " 2>&1";
    
    std::string output = executeCommandWithOutput(cmd.str());
    
    if (output.find("1 packets transmitted, 1 received") != std::string::npos ||
        output.find("1 packets transmitted, 1 packets received") != std::string::npos) {
        result.success = true;
        result.response_time_ms = parseResponseTime(output);
    } else {
        result.error_message = output.substr(0, 200); // Limit error message length
    }
    
    return result;
}

std::vector<PingResult> IPMonitor::performAllPings(const std::string& interface_name) {
    std::vector<PingResult> results;
    
    std::vector<std::string> targets;
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        targets = m_config.ping_targets;
    }
    
    for (const auto& target : targets) {
        results.push_back(performPing(target, interface_name));
    }
    
    return results;
}

ModemReferenceData IPMonitor::collectModemData(QMISessionHandler* session_handler, const std::string& interface_name) {
    ModemReferenceData data;
    data.interface_name = interface_name;
    data.data_timestamp = std::chrono::system_clock::now();
    data.is_connected = false;
    
    if (!session_handler) {
        return data;
    }
    
    try {
        // Get device path
        data.device_path = session_handler->getDevicePath();
        
        // Get IMEI
        data.imei = session_handler->getIMEI();
        
        // Get signal information
        auto signal_info = session_handler->getSignalInfo();
        data.signal_strength = std::to_string(signal_info.rssi) + " dBm";
        data.network_type = signal_info.network_type;
        
        // Get current network settings
        auto settings = session_handler->getCurrentSettings();
        data.ip_address = settings.ip_address;
        data.gateway = settings.gateway;
        data.dns_primary = settings.dns_primary;
        data.dns_secondary = settings.dns_secondary;
        
        // Check if session is active
        data.is_connected = session_handler->isSessionActive();
        
    } catch (const std::exception& e) {
        std::cerr << "Error collecting modem data: " << e.what() << std::endl;
    }
    
    return data;
}

IPMonitorReport IPMonitor::generateReport(const std::string& interface_name, QMISessionHandler* session_handler) {
    IPMonitorReport report;
    report.report_timestamp = std::chrono::system_clock::now();
    
    // Perform ping tests
    report.ping_results = performAllPings(interface_name);
    
    // Calculate statistics
    report.total_pings = report.ping_results.size();
    report.successful_pings = 0;
    double total_response_time = 0.0;
    int valid_response_times = 0;
    
    for (const auto& ping : report.ping_results) {
        if (ping.success) {
            report.successful_pings++;
            if (ping.response_time_ms > 0) {
                total_response_time += ping.response_time_ms;
                valid_response_times++;
            }
        }
    }
    
    report.average_response_time = valid_response_times > 0 ? total_response_time / valid_response_times : -1.0;
    
    // Collect modem data if enabled
    bool include_modem_data;
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        include_modem_data = m_config.include_modem_data;
    }
    
    if (include_modem_data) {
        report.modem_data = collectModemData(session_handler, interface_name);
    }
    
    return report;
}

void IPMonitor::setMonitorCallback(IPMonitorCallback callback) {
    m_monitor_callback = callback;
}

IPMonitor::MonitoringStats IPMonitor::getStats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void IPMonitor::resetStats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats = MonitoringStats();
    m_stats.start_time = std::chrono::system_clock::now();
}

void IPMonitor::monitoringLoop() {
    std::cout << "IP monitoring loop started" << std::endl;
    
    while (m_monitoring) {
        auto report = generateReport(m_interface_name, m_session_handler);
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            m_stats.total_reports++;
            m_stats.total_pings += report.total_pings;
            m_stats.successful_pings += report.successful_pings;
            m_stats.last_report_time = report.report_timestamp;
        }
        
        // Log to terminal
        logReportToTerminal(report);
        
        // Call callback if set
        if (m_monitor_callback) {
            m_monitor_callback(report);
        }
        
        // Wait for next interval
        int interval_ms;
        {
            std::lock_guard<std::mutex> lock(m_config_mutex);
            interval_ms = m_config.ping_interval_ms;
        }
        
        std::unique_lock<std::mutex> lock(m_monitoring_mutex);
        m_monitor_cv.wait_for(lock, std::chrono::milliseconds(interval_ms),
                             [this] { return !m_monitoring; });
    }
    
    std::cout << "IP monitoring loop ended" << std::endl;
}

void IPMonitor::logReportToTerminal(const IPMonitorReport& report) {
    std::string log_format;
    {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        log_format = m_config.log_format;
    }
    
    if (log_format == "json") {
        std::cout << "=== IP MONITOR REPORT ===" << std::endl;
        std::cout << report.toJson() << std::endl;
        std::cout << "=========================" << std::endl;
    } else {
        // Simple text format
        std::cout << "IP Monitor Report - " << getCurrentTimestamp() << std::endl;
        std::cout << "Interface: " << m_interface_name << std::endl;
        std::cout << "Successful pings: " << report.successful_pings << "/" << report.total_pings << std::endl;
        if (report.average_response_time > 0) {
            std::cout << "Average response time: " << std::fixed << std::setprecision(2) 
                      << report.average_response_time << "ms" << std::endl;
        }
        std::cout << "---" << std::endl;
    }
}

std::string IPMonitor::executeCommandWithOutput(const std::string& command) {
    std::string result;
    char buffer[256];
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "Error: Failed to execute command";
    }
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    return result;
}

double IPMonitor::parseResponseTime(const std::string& ping_output) {
    std::regex time_regex("time=([0-9.]+)");
    std::smatch match;
    
    if (std::regex_search(ping_output, match, time_regex)) {
        try {
            return std::stod(match[1].str());
        } catch (const std::exception& e) {
            return -1.0;
        }
    }
    
    return -1.0;
}

std::string IPMonitor::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}
