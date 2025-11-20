
#include "qmi_watchdog.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <iomanip>
#include <algorithm>

Json::Value loadMonitoringConfig(const std::string& filePath) {
    std::ifstream configFile(filePath);
    if (!configFile.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filePath);
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    
    if (!Json::parseFromStream(reader, configFile, &root, &errs)) {
        throw std::runtime_error("Failed to parse JSON: " + errs);
    }

    if (!root.isMember("monitoring_config")) {
        throw std::runtime_error("monitoring_config section not found in config file");
    }

    return root["monitoring_config"];
}

Json::Value loadFailureDetectionConfig(const std::string& filePath) {
    std::ifstream configFile(filePath);
    if (!configFile.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filePath);
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    
    if (!Json::parseFromStream(reader, configFile, &root, &errs)) {
        throw std::runtime_error("Failed to parse JSON: " + errs);
    }

    if (!root.isMember("failure_detection")) {
        throw std::runtime_error("failure_detection section not found in config file");
    }

    return root["failure_detection"];
}

// SignalMetrics JSON serialization
std::string SignalMetrics::toJson() const {
    Json::Value root;
    root["type"] = "signal_metrics";
    root["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    root["status"] = static_cast<int>(status);
    root["status_text"] = (status == CollectionStatus::SUCCESS) ? "SUCCESS" : "FAILED";
    
    if (status == CollectionStatus::SUCCESS) {
        root["rssi_dbm"] = rssi;
        root["rsrq_db"] = rsrq;
        root["rsrp_dbm"] = rsrp;
        root["snr_db"] = snr;
        root["radio_interface"] = radio_interface;
    } else {
        root["error_message"] = error_message;
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}



// NetworkInfo JSON serialization
std::string NetworkInfo::toJson() const {
    Json::Value root;
    root["type"] = "network_info";
    root["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    root["status"] = static_cast<int>(status);
    root["status_text"] = (status == CollectionStatus::SUCCESS) ? "SUCCESS" : "FAILED";
    
    if (status == CollectionStatus::SUCCESS) {
        root["registration_state"] = registration_state;
        root["cs_state"] = cs_state;
        root["ps_state"] = ps_state;
        root["selected_network"] = selected_network;
        root["roaming_status"] = roaming_status;
        root["mcc"] = mcc;
        root["mnc"] = mnc;
        root["operator_description"] = operator_description;
        root["location_area_code"] = location_area_code;
        root["cell_id"] = cell_id;
        root["tracking_area_code"] = tracking_area_code;
        root["detailed_status"] = detailed_status;
        root["capability"] = capability;
        root["forbidden"] = forbidden;
        
        Json::Value interfaces(Json::arrayValue);
        for (const auto& iface : radio_interfaces) {
            interfaces.append(iface);
        }
        root["radio_interfaces"] = interfaces;
        
        Json::Value capabilities(Json::arrayValue);
        for (const auto& cap : data_service_capabilities) {
            capabilities.append(cap);
        }
        root["data_service_capabilities"] = capabilities;
    } else {
        root["error_message"] = error_message;
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

// RFBandInfo JSON serialization
std::string RFBandInfo::toJson() const {
    Json::Value root;
    root["type"] = "rf_band_info";
    root["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    root["status"] = static_cast<int>(status);
    root["status_text"] = (status == CollectionStatus::SUCCESS) ? "SUCCESS" : "FAILED";
    
    if (status == CollectionStatus::SUCCESS) {
        root["radio_interface"] = radio_interface;
        root["active_band_class"] = active_band_class;
        root["active_channel"] = active_channel;
        root["bandwidth"] = bandwidth;
    } else {
        root["error_message"] = error_message;
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

// HealthScore JSON serialization
std::string HealthScore::toJson() const {
    Json::Value root;
    root["type"] = "health_score";
    root["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    root["overall_score"] = overall_score;
    root["signal_score"] = signal_score;
    root["network_score"] = network_score;
    root["rf_score"] = rf_score;
    root["health_status"] = health_status;
    
    Json::Value warn_array(Json::arrayValue);
    for (const auto& warning : warnings) {
        warn_array.append(warning);
    }
    root["warnings"] = warn_array;
    
    Json::Value critical_array(Json::arrayValue);
    for (const auto& issue : critical_issues) {
        critical_array.append(issue);
    }
    root["critical_issues"] = critical_array;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

// MonitoringSnapshot JSON serialization
std::string MonitoringSnapshot::toJson() const {
    Json::Value root;
    root["type"] = "monitoring_snapshot";
    root["device_path"] = device_path;
    root["collection_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        collection_time.time_since_epoch()).count();
    
    // Parse and embed individual metric JSON objects
    Json::Reader reader;
    Json::Value signal_json, network_json, rf_json, health_json;
    
    reader.parse(signal.toJson(), signal_json);
    reader.parse(network.toJson(), network_json);
    reader.parse(rf_band.toJson(), rf_json);
    reader.parse(health.toJson(), health_json);
    
    root["signal_metrics"] = signal_json;
    root["network_info"] = network_json;
    root["rf_band_info"] = rf_json;
    root["health_score"] = health_json;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

// WatchdogStats JSON serialization
std::string QMIWatchdog::WatchdogStats::toJson() const {
    Json::Value root;
    root["type"] = "watchdog_statistics";
    root["total_collections"] = static_cast<Json::UInt64>(total_collections);
    root["successful_collections"] = static_cast<Json::UInt64>(successful_collections);
    root["failed_collections"] = static_cast<Json::UInt64>(failed_collections);
    root["detected_failures"] = static_cast<Json::UInt64>(detected_failures);
    root["start_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        start_time.time_since_epoch()).count();
    root["last_collection_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        last_collection_time.time_since_epoch()).count();
    
    if (total_collections > 0) {
        root["success_rate"] = static_cast<double>(successful_collections) / 
                              static_cast<double>(total_collections);
    } else {
        root["success_rate"] = 0.0;
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

// QMIWatchdog implementation
QMIWatchdog::QMIWatchdog() 
    : m_monitoring(false) {
    m_stats.start_time = std::chrono::system_clock::now();
    
    // Set default failure detection configuration
    m_failure_config = FailureDetectionConfig{};
}

QMIWatchdog::~QMIWatchdog() {
    stopMonitoring();
}

bool QMIWatchdog::loadDeviceConfig(const std::string& config_json) {
    return parseDeviceConfigJson(config_json);
}

bool QMIWatchdog::loadDeviceConfigFromFile(const std::string& config_file_path) {
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_file_path << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return parseDeviceConfigJson(buffer.str());
}

bool QMIWatchdog::parseDeviceConfigJson(const std::string& json) {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json, root)) {
        std::cerr << "Failed to parse JSON configuration" << std::endl;
        return false;
    }
    
    // Handle both basic and advanced profile formats
    if (root.isMember("devices") && root["devices"].isArray() && !root["devices"].empty()) {
        // Basic format: {"devices": [...]}
        const Json::Value& device = root["devices"][0];
        m_device_config.device_path = device["device_path"].asString();
        m_device_config.imei = device.get("imei", "").asString();
        m_device_config.model = device.get("model", "").asString();
        m_device_config.manufacturer = device.get("manufacturer", "").asString();
        m_device_config.is_available = device.get("is_available", false).asBool();
    } else if (root.isMember("profiles") && root["profiles"].isArray() && !root["profiles"].empty()) {
        // Advanced format: {"profiles": [...]}
        const Json::Value& profile = root["profiles"][0];
        if (profile.isMember("basic")) {
            const Json::Value& basic = profile["basic"];
            m_device_config.device_path = basic["path"].asString();
            m_device_config.imei = basic.get("imei", "").asString();
            m_device_config.model = basic.get("model", "").asString();
            m_device_config.manufacturer = basic.get("manufacturer", "").asString();
            m_device_config.is_available = true;
        }
    } else if (root.isMember("device_path")) {
        // Simple format: direct device configuration
        m_device_config.device_path = root["device_path"].asString();
        m_device_config.imei = root.get("imei", "").asString();
        m_device_config.model = root.get("model", "").asString();
        m_device_config.manufacturer = root.get("manufacturer", "").asString();
        m_device_config.is_available = true;
    } else {
        std::cerr << "No valid device configuration found in JSON" << std::endl;
        return false;
    }
    
    // Load monitoring configuration if present
    if (root.isMember("monitoring_config")) {
        const Json::Value& monitoring = root["monitoring_config"];
        m_device_config.collection_interval_ms = monitoring.get("collection_interval_ms", 5000).asInt();
        m_device_config.timeout_ms = monitoring.get("timeout_ms", 10000).asInt();
        m_device_config.enable_health_scoring = monitoring.get("enable_health_scoring", true).asBool();
        
        if (monitoring.isMember("health_weights")) {
            const Json::Value& weights = monitoring["health_weights"];
            m_device_config.weights.signal_weight = weights.get("signal_weight", 0.5).asDouble();
            m_device_config.weights.network_weight = weights.get("network_weight", 0.35).asDouble();
            m_device_config.weights.rf_weight = weights.get("rf_weight", 0.15).asDouble();
        }
    }
    
    std::cout << "Loaded device configuration: " << m_device_config.device_path << std::endl;
    return true;
}

void QMIWatchdog::setFailureDetectionConfig(const FailureDetectionConfig& config) {
    m_failure_config = config;
}

void QMIWatchdog::setHealthWeights(const HealthWeights& weights) {
    m_device_config.weights = weights;
}

bool QMIWatchdog::startMonitoring() {
    if (m_monitoring.load()) {
        std::cout << "Monitoring already active" << std::endl;
        return true;
    }
    
    if (m_device_config.device_path.empty()) {
        std::cerr << "No device configuration loaded" << std::endl;
        return false;
    }
    
    std::cout << "Starting QMI watchdog monitoring for device: " << m_device_config.device_path << std::endl;
    
    m_monitoring.store(true);
    m_monitor_thread = std::make_unique<std::thread>(&QMIWatchdog::monitoringLoop, this);
    
    return true;
}

void QMIWatchdog::stopMonitoring() {
    if (!m_monitoring.load()) {
        return;
    }
    
    std::cout << "Stopping QMI watchdog monitoring..." << std::endl;
    
    m_monitoring.store(false);
    
    if (m_monitor_thread && m_monitor_thread->joinable()) {
        m_monitor_thread->join();
    }
    
    std::cout << "QMI watchdog monitoring stopped" << std::endl;
}

bool QMIWatchdog::isMonitoring() const {
    return m_monitoring.load();
}

void QMIWatchdog::monitoringLoop() {
    std::cout << "QMI watchdog monitoring loop started" << std::endl;
    
    while (m_monitoring.load()) {
        auto start_time = std::chrono::steady_clock::now();
        
        // Collect full monitoring snapshot
        MonitoringSnapshot snapshot = collectFullSnapshot();
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            m_stats.total_collections++;
            m_stats.last_collection_time = std::chrono::system_clock::now();
            
            bool all_successful = (snapshot.signal.status == CollectionStatus::SUCCESS &&
                                 snapshot.network.status == CollectionStatus::SUCCESS &&
                                 snapshot.rf_band.status == CollectionStatus::SUCCESS);
            
            if (all_successful) {
                m_stats.successful_collections++;
                updateCollectionStatus(CollectionStatus::SUCCESS);
            } else {
                m_stats.failed_collections++;
                updateCollectionStatus(CollectionStatus::FAILED);
            }
        }
        
        // Print JSON data to terminal
        printJsonToTerminal(snapshot.toJson(), "MONITORING_SNAPSHOT");
        
        // Check for failures
        std::vector<std::string> failures = detectFailures(snapshot);
        if (!failures.empty()) {
            {
                std::lock_guard<std::mutex> lock(m_stats_mutex);
                m_stats.detected_failures++;
            }
            
            if (m_failure_callback) {
                m_failure_callback("FAILURE_DETECTED", failures);
            }
            
            // Print failure information
            Json::Value failure_json;
            failure_json["type"] = "failure_detection";
            failure_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            failure_json["device_path"] = m_device_config.device_path;
            
            Json::Value failures_array(Json::arrayValue);
            for (const auto& failure : failures) {
                failures_array.append(failure);
            }
            failure_json["detected_failures"] = failures_array;
            
            Json::StreamWriterBuilder builder;
            printJsonToTerminal(Json::writeString(builder, failure_json), "FAILURE_DETECTION");
        }
        
        // Execute data collection callback if set
        if (m_data_callback) {
            m_data_callback(snapshot);
        }
        
        // Calculate sleep time to maintain interval
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto sleep_time = std::chrono::milliseconds(m_device_config.collection_interval_ms) - elapsed;
        
        if (sleep_time > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
    
    std::cout << "QMI watchdog monitoring loop ended" << std::endl;
}

MonitoringSnapshot QMIWatchdog::collectFullSnapshot() {
    MonitoringSnapshot snapshot;
    snapshot.device_path = m_device_config.device_path;
    snapshot.collection_time = std::chrono::system_clock::now();
    
    // Collect all metrics
    snapshot.signal = collectSignalMetrics();
    snapshot.network = collectNetworkInfo();
    snapshot.rf_band = collectRFBandInfo();
    
    // Calculate health score if enabled
    if (m_device_config.enable_health_scoring) {
        snapshot.health = calculateHealthScore(snapshot);
    }
    
    return snapshot;
}

SignalMetrics QMIWatchdog::collectSignalMetrics() {
    SignalMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    
    std::string command = "qmicli -d " + m_device_config.device_path + " --nas-get-signal-info";
    std::string output = executeQMICommand(command, m_device_config.timeout_ms);
    
    if (output.empty() || output.find("error") != std::string::npos) {
        metrics.status = CollectionStatus::DEVICE_ERROR;
        metrics.error_message = "Failed to execute QMI command or device error";
        return metrics;
    }
    
    metrics = parseSignalInfo(output);
    return metrics;
}



NetworkInfo QMIWatchdog::collectNetworkInfo() {
    NetworkInfo info;
    info.timestamp = std::chrono::system_clock::now();
    
    std::string command = "qmicli -d " + m_device_config.device_path + " --nas-get-serving-system";
    std::string output = executeQMICommand(command, m_device_config.timeout_ms);
    
    if (output.empty() || output.find("error") != std::string::npos) {
        info.status = CollectionStatus::DEVICE_ERROR;
        info.error_message = "Failed to execute QMI command or device error";
        return info;
    }
    
    info = parseNetworkInfo(output);
    return info;
}

RFBandInfo QMIWatchdog::collectRFBandInfo() {
    RFBandInfo info;
    info.timestamp = std::chrono::system_clock::now();
    
    std::string command = "qmicli -d " + m_device_config.device_path + " --nas-get-rf-band-info";
    std::string output = executeQMICommand(command, m_device_config.timeout_ms);
    
    if (output.empty() || output.find("error") != std::string::npos) {
        info.status = CollectionStatus::DEVICE_ERROR;
        info.error_message = "Failed to execute QMI command or device error";
        return info;
    }
    
    info = parseRFBandInfo(output);
    return info;
}

std::string QMIWatchdog::executeQMICommand(const std::string& command, int timeout_ms) {
    // Add timeout command wrapper
    std::string timeout_cmd = "timeout " + std::to_string(timeout_ms / 1000) + "s " + command;
    
    FILE* pipe = popen(timeout_cmd.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int exit_code = pclose(pipe);
    if (WEXITSTATUS(exit_code) != 0) {
        // Command failed or timed out
        return "";
    }
    
    return result;
}

SignalMetrics QMIWatchdog::parseSignalInfo(const std::string& qmi_output) {
    SignalMetrics metrics;
    
    try {
        // Extract LTE signal information
        metrics.rssi = extractNumericValue(qmi_output, "RSSI:");
        metrics.rsrq = extractNumericValue(qmi_output, "RSRQ:");
        metrics.rsrp = extractNumericValue(qmi_output, "RSRP:");
        metrics.snr = extractNumericValue(qmi_output, "SNR:");
        
        // Extract radio interface
        if (qmi_output.find("LTE:") != std::string::npos) {
            metrics.radio_interface = "lte";
        } else if (qmi_output.find("5G:") != std::string::npos || qmi_output.find("NR:") != std::string::npos) {
            metrics.radio_interface = "5g";
        } else {
            metrics.radio_interface = "unknown";
        }
        
        metrics.status = CollectionStatus::SUCCESS;
    } catch (const std::exception& e) {
        metrics.status = CollectionStatus::PARSE_ERROR;
        metrics.error_message = "Failed to parse signal information: " + std::string(e.what());
    }
    
    return metrics;
}



NetworkInfo QMIWatchdog::parseNetworkInfo(const std::string& qmi_output) {
    NetworkInfo info;
    
    try {
        info.registration_state = extractStringValue(qmi_output, "Registration state:");
        info.cs_state = extractStringValue(qmi_output, "CS:");
        info.ps_state = extractStringValue(qmi_output, "PS:");
        info.selected_network = extractStringValue(qmi_output, "Selected network:");
        info.roaming_status = extractStringValue(qmi_output, "Roaming status:");
        info.mcc = extractStringValue(qmi_output, "MCC:");
        info.mnc = extractStringValue(qmi_output, "MNC:");
        info.operator_description = extractStringValue(qmi_output, "Description:");
        info.location_area_code = extractStringValue(qmi_output, "3GPP location area code:");
        info.cell_id = extractStringValue(qmi_output, "3GPP cell ID:");
        info.tracking_area_code = extractStringValue(qmi_output, "LTE tracking area code:");
        info.detailed_status = extractStringValue(qmi_output, "Status:");
        info.capability = extractStringValue(qmi_output, "Capability:");
        
        // Parse radio interfaces
        std::regex radio_regex("\\[\\d+\\]:\\s*'([^']+)'");
        std::sregex_iterator iter(qmi_output.begin(), qmi_output.end(), radio_regex);
        std::sregex_iterator end;
        
        bool in_radio_section = false;
        std::istringstream iss(qmi_output);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("Radio interfaces:") != std::string::npos) {
                in_radio_section = true;
                continue;
            }
            if (in_radio_section && line.find("[0]:") != std::string::npos) {
                std::regex interface_regex("\\[0\\]:\\s*'([^']+)'");
                std::smatch match;
                if (std::regex_search(line, match, interface_regex)) {
                    info.radio_interfaces.push_back(match[1].str());
                }
                break;
            }
        }
        
        info.forbidden = (qmi_output.find("Forbidden: 'yes'") != std::string::npos);
        
        info.status = CollectionStatus::SUCCESS;
    } catch (const std::exception& e) {
        info.status = CollectionStatus::PARSE_ERROR;
        info.error_message = "Failed to parse network information: " + std::string(e.what());
    }
    
    return info;
}

RFBandInfo QMIWatchdog::parseRFBandInfo(const std::string& qmi_output) {
    RFBandInfo info;
    
    try {
        info.radio_interface = extractStringValue(qmi_output, "Radio Interface:");
        info.active_band_class = extractStringValue(qmi_output, "Active Band Class:");
        info.active_channel = extractStringValue(qmi_output, "Active Channel:");
        info.bandwidth = extractStringValue(qmi_output, "Bandwidth:");
        
        info.status = CollectionStatus::SUCCESS;
    } catch (const std::exception& e) {
        info.status = CollectionStatus::PARSE_ERROR;
        info.error_message = "Failed to parse RF band information: " + std::string(e.what());
    }
    
    return info;
}

double QMIWatchdog::extractNumericValue(const std::string& text, const std::string& pattern) {
    std::regex regex(pattern + "\\s*'?([+-]?\\d*\\.?\\d+)");
    std::smatch match;
    
    if (std::regex_search(text, match, regex)) {
        return std::stod(match[1].str());
    }
    
    return 0.0;
}

std::string QMIWatchdog::extractStringValue(const std::string& text, const std::string& field) {
    std::regex regex(field + "\\s*'([^']*)'");
    std::smatch match;
    
    if (std::regex_search(text, match, regex)) {
        return match[1].str();
    }
    
    return "";
}

HealthScore QMIWatchdog::calculateHealthScore(const MonitoringSnapshot& snapshot) {
    HealthScore health;
    health.timestamp = std::chrono::system_clock::now();
    
    // Calculate individual component scores
    health.signal_score = calculateSignalScore(snapshot.signal);
    health.network_score = calculateNetworkScore(snapshot.network);
    health.rf_score = calculateRFScore(snapshot.rf_band);
    
    // Calculate weighted overall score
    health.overall_score = (health.signal_score * m_device_config.weights.signal_weight) +
                          (health.network_score * m_device_config.weights.network_weight) +
                          (health.rf_score * m_device_config.weights.rf_weight);
    
    // Determine health status
    if (health.overall_score >= 90) {
        health.health_status = "EXCELLENT";
    } else if (health.overall_score >= 75) {
        health.health_status = "GOOD";
    } else if (health.overall_score >= 60) {
        health.health_status = "FAIR";
    } else if (health.overall_score >= 40) {
        health.health_status = "POOR";
    } else {
        health.health_status = "CRITICAL";
    }
    
    // Generate warnings and critical issues
    if (snapshot.signal.status == CollectionStatus::SUCCESS) {
        if (snapshot.signal.rssi < m_failure_config.critical_rssi_threshold) {
            health.critical_issues.push_back("Critical RSSI level: " + std::to_string(snapshot.signal.rssi) + " dBm");
        } else if (snapshot.signal.rssi < m_failure_config.warning_rssi_threshold) {
            health.warnings.push_back("Low RSSI level: " + std::to_string(snapshot.signal.rssi) + " dBm");
        }
        
        if (snapshot.signal.rsrp < m_failure_config.critical_rsrp_threshold) {
            health.critical_issues.push_back("Critical RSRP level: " + std::to_string(snapshot.signal.rsrp) + " dBm");
        } else if (snapshot.signal.rsrp < m_failure_config.warning_rsrp_threshold) {
            health.warnings.push_back("Low RSRP level: " + std::to_string(snapshot.signal.rsrp) + " dBm");
        }
    }
    
    
    
    return health;
}

double QMIWatchdog::calculateSignalScore(const SignalMetrics& signal) {
    if (signal.status != CollectionStatus::SUCCESS) {
        return 0.0;
    }
    
    // RSSI score (0-100)
    double rssi_score = 0.0;
    if (signal.rssi >= -60) rssi_score = 100.0;
    else if (signal.rssi >= -70) rssi_score = 80.0;
    else if (signal.rssi >= -80) rssi_score = 60.0;
    else if (signal.rssi >= -90) rssi_score = 40.0;
    else if (signal.rssi >= -100) rssi_score = 20.0;
    else rssi_score = 0.0;
    
    // RSRP score (0-100)
    double rsrp_score = 0.0;
    if (signal.rsrp >= -80) rsrp_score = 100.0;
    else if (signal.rsrp >= -90) rsrp_score = 80.0;
    else if (signal.rsrp >= -100) rsrp_score = 60.0;
    else if (signal.rsrp >= -110) rsrp_score = 40.0;
    else if (signal.rsrp >= -120) rsrp_score = 20.0;
    else rsrp_score = 0.0;
    
    // RSRQ score (0-100)
    double rsrq_score = 0.0;
    if (signal.rsrq >= -5) rsrq_score = 100.0;
    else if (signal.rsrq >= -8) rsrq_score = 80.0;
    else if (signal.rsrq >= -12) rsrq_score = 60.0;
    else if (signal.rsrq >= -15) rsrq_score = 40.0;
    else if (signal.rsrq >= -20) rsrq_score = 20.0;
    else rsrq_score = 0.0;
    
    // SNR score (0-100)
    double snr_score = 0.0;
    if (signal.snr >= 20) snr_score = 100.0;
    else if (signal.snr >= 10) snr_score = 80.0;
    else if (signal.snr >= 5) snr_score = 60.0;
    else if (signal.snr >= 0) snr_score = 40.0;
    else if (signal.snr >= -5) snr_score = 20.0;
    else snr_score = 0.0;
    
    // Weighted average
    return (rssi_score * 0.3) + (rsrp_score * 0.3) + (rsrq_score * 0.25) + (snr_score * 0.15);
}

double QMIWatchdog::calculateNetworkScore(const NetworkInfo& network) {
    if (network.status != CollectionStatus::SUCCESS) {
        return 0.0;
    }
    
    double score = 0.0;
    
    // Registration state (40% of network score)
    if (network.registration_state == "registered") {
        score += 40.0;
    }
    
    // CS/PS state (20% of network score)
    if (network.cs_state == "attached" && network.ps_state == "attached") {
        score += 20.0;
    } else if (network.cs_state == "attached" || network.ps_state == "attached") {
        score += 10.0;
    }
    
    // Roaming status (20% of network score)
    if (network.roaming_status == "off") {
        score += 20.0;
    } else {
        score += 10.0; // Roaming but connected
    }
    
    // Radio interface presence (20% of network score)
    if (!network.radio_interfaces.empty()) {
        score += 20.0;
    }
    
    return score;
}



double QMIWatchdog::calculateRFScore(const RFBandInfo& rf) {
    if (rf.status != CollectionStatus::SUCCESS) {
        return 0.0;
    }
    
    double score = 50.0; // Base score for successful collection
    
    // Bonus for LTE
    if (rf.radio_interface == "lte") {
        score += 30.0;
    }
    
    // Bonus for bandwidth
    if (!rf.bandwidth.empty()) {
        try {
            int bw = std::stoi(rf.bandwidth);
            if (bw >= 20) score += 20.0;      // 20MHz or higher
            else if (bw >= 10) score += 15.0; // 10MHz
            else if (bw >= 5) score += 10.0;  // 5MHz
            else score += 5.0;                // Lower bandwidth
        } catch (...) {
            // Bandwidth parsing failed, use base score
        }
    }
    
    return score;
}

std::vector<std::string> QMIWatchdog::detectFailures(const MonitoringSnapshot& snapshot) {
    std::vector<std::string> failures;
    
    // Check signal failures
    auto signal_failures = checkSignalFailures(snapshot.signal);
    failures.insert(failures.end(), signal_failures.begin(), signal_failures.end());
    
    // Check network failures
    auto network_failures = checkNetworkFailures(snapshot.network);
    failures.insert(failures.end(), network_failures.begin(), network_failures.end());
    
    // Check collection failures
    auto collection_failures = checkCollectionFailures();
    failures.insert(failures.end(), collection_failures.begin(), collection_failures.end());
    
    return failures;
}

std::vector<std::string> QMIWatchdog::checkSignalFailures(const SignalMetrics& signal) {
    std::vector<std::string> failures;
    
    if (signal.status != CollectionStatus::SUCCESS) {
        failures.push_back("Signal collection failed: " + signal.error_message);
        return failures;
    }
    
    if (signal.rssi < m_failure_config.critical_rssi_threshold) {
        failures.push_back("Critical RSSI level: " + std::to_string(signal.rssi) + " dBm");
    }
    
    if (signal.rsrp < m_failure_config.critical_rsrp_threshold) {
        failures.push_back("Critical RSRP level: " + std::to_string(signal.rsrp) + " dBm");
    }
    
    if (signal.rsrq < m_failure_config.critical_rsrq_threshold) {
        failures.push_back("Critical RSRQ level: " + std::to_string(signal.rsrq) + " dB");
    }
    
    return failures;
}



std::vector<std::string> QMIWatchdog::checkNetworkFailures(const NetworkInfo& network) {
    std::vector<std::string> failures;
    
    if (network.status != CollectionStatus::SUCCESS) {
        failures.push_back("Network information collection failed: " + network.error_message);
        return failures;
    }
    
    if (network.registration_state != "registered") {
        failures.push_back("Network not registered: " + network.registration_state);
    }
    
    if (network.cs_state != "attached" && network.ps_state != "attached") {
        failures.push_back("Network not attached (CS: " + network.cs_state + ", PS: " + network.ps_state + ")");
    }
    
    return failures;
}

std::vector<std::string> QMIWatchdog::checkCollectionFailures() {
    std::vector<std::string> failures;
    
    std::lock_guard<std::mutex> lock(m_failure_tracking_mutex);
    
    if (m_recent_collection_status.size() < m_failure_config.max_consecutive_failures) {
        return failures; // Not enough data yet
    }
    
    // Check for consecutive failures
    int consecutive_failures = 0;
    for (auto it = m_recent_collection_status.rbegin(); 
         it != m_recent_collection_status.rend() && consecutive_failures < m_failure_config.max_consecutive_failures; 
         ++it) {
        if (*it == CollectionStatus::FAILED) {
            consecutive_failures++;
        } else {
            break;
        }
    }
    
    if (consecutive_failures >= m_failure_config.max_consecutive_failures) {
        failures.push_back("Consecutive collection failures detected: " + std::to_string(consecutive_failures));
    }
    
    return failures;
}

void QMIWatchdog::updateCollectionStatus(CollectionStatus status) {
    std::lock_guard<std::mutex> lock(m_failure_tracking_mutex);
    
    m_recent_collection_status.push_back(status);
    
    // Keep only the recent status history
    if (m_recent_collection_status.size() > static_cast<size_t>(m_failure_config.failure_detection_window)) {
        m_recent_collection_status.erase(m_recent_collection_status.begin());
    }
}

void QMIWatchdog::setDataCollectionCallback(DataCollectionCallback callback) {
    m_data_callback = callback;
}

void QMIWatchdog::setFailureDetectionCallback(FailureDetectionCallback callback) {
    m_failure_callback = callback;
}

QMIWatchdog::WatchdogStats QMIWatchdog::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

std::string QMIWatchdog::getStatus() const {
    Json::Value status;
    status["monitoring_active"] = m_monitoring.load();
    status["device_path"] = m_device_config.device_path;
    status["collection_interval_ms"] = m_device_config.collection_interval_ms;
    status["health_scoring_enabled"] = m_device_config.enable_health_scoring;
    
    // Add statistics
    WatchdogStats stats = getStatistics();
    Json::Reader reader;
    Json::Value stats_json;
    reader.parse(stats.toJson(), stats_json);
    status["statistics"] = stats_json;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, status);
}

void QMIWatchdog::printJsonToTerminal(const std::string& json_data, const std::string& data_type) {
    // Print with timestamp and data type header
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::cout << "\n========== " << data_type << " [" 
              << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count()
              << "] ==========\n";
    std::cout << json_data << std::endl;
    std::cout << "================================" << std::string(data_type.length(), '=') << "===========================\n" << std::endl;
}

std::string QMIWatchdog::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
