
#include "failure_detector.h"
#include "command_logger.h"
#include "qmi_session_handler.h"
#include "interface_controller.h"
#include "connectivity_monitor.h"
#include <iostream>
#include <algorithm>

FailureDetector::FailureDetector(QMISessionHandler* session_handler,
                                InterfaceController* interface_controller,
                                ConnectivityMonitor* connectivity_monitor)
    : m_session_handler(session_handler)
    , m_interface_controller(interface_controller)
    , m_connectivity_monitor(connectivity_monitor)
    , m_detecting(false)
    , m_detection_interval_ms(10000)
{
    // Initialize enabled checks
    m_enabled_checks[FailureType::SESSION_LOST] = true;
    m_enabled_checks[FailureType::IP_CONFIGURATION_LOST] = true;
    m_enabled_checks[FailureType::DNS_FAILURE] = true;
    m_enabled_checks[FailureType::CONNECTIVITY_LOST] = true;
    m_enabled_checks[FailureType::SIGNAL_WEAK] = true;
    m_enabled_checks[FailureType::MODEM_UNRESPONSIVE] = true;
    m_enabled_checks[FailureType::INTERFACE_DOWN] = true;
    m_enabled_checks[FailureType::ROUTING_FAILURE] = true;
    
    // Initialize failure thresholds
    m_failure_thresholds[FailureType::SESSION_LOST] = 1;
    m_failure_thresholds[FailureType::IP_CONFIGURATION_LOST] = 1;
    m_failure_thresholds[FailureType::DNS_FAILURE] = 2;
    m_failure_thresholds[FailureType::CONNECTIVITY_LOST] = 3;
    m_failure_thresholds[FailureType::SIGNAL_WEAK] = 5;
    m_failure_thresholds[FailureType::MODEM_UNRESPONSIVE] = 2;
    m_failure_thresholds[FailureType::INTERFACE_DOWN] = 1;
    m_failure_thresholds[FailureType::ROUTING_FAILURE] = 2;
    
    // Initialize failure counts
    for (auto& pair : m_failure_thresholds) {
        m_failure_counts[pair.first] = 0;
    }
}

FailureDetector::~FailureDetector() {
    stopDetection();
}

void FailureDetector::startDetection() {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_detecting) {
        return;
    }
    
    m_detecting = true;
    m_detection_thread = std::make_unique<std::thread>(&FailureDetector::detectionLoop, this);
    
    std::cout << "Failure detection started" << std::endl;
}

void FailureDetector::stopDetection() {
    {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        m_detecting = false;
    }
    
    m_detection_cv.notify_all();
    
    if (m_detection_thread && m_detection_thread->joinable()) {
        m_detection_thread->join();
    }
    
    std::cout << "Failure detection stopped" << std::endl;
}

bool FailureDetector::isDetecting() const {
    return m_detecting;
}

void FailureDetector::setFailureCallback(FailureCallback callback) {
    m_failure_callback = callback;
}

void FailureDetector::setDetectionInterval(int interval_ms) {
    m_detection_interval_ms = interval_ms;
}

void FailureDetector::enableFailureType(FailureType type, bool enable) {
    m_enabled_checks[type] = enable;
    std::cout << "Failure detection for type " << static_cast<int>(type) 
              << (enable ? " enabled" : " disabled") << std::endl;
}

void FailureDetector::setFailureThreshold(FailureType type, int threshold) {
    m_failure_thresholds[type] = threshold;
    std::cout << "Failure threshold for type " << static_cast<int>(type) 
              << " set to " << threshold << std::endl;
}

std::vector<FailureEvent> FailureDetector::performFullDiagnostic() {
    std::vector<FailureEvent> failures;
    
    // Check all failure types
    for (const auto& enabled : m_enabled_checks) {
        if (!enabled.second) continue;
        
        FailureEvent failure;
        switch (enabled.first) {
            case FailureType::SESSION_LOST:
                failure = checkSessionStatus();
                break;
            case FailureType::IP_CONFIGURATION_LOST:
                failure = checkIPConfiguration();
                break;
            case FailureType::DNS_FAILURE:
                failure = checkDNSResolution();
                break;
            case FailureType::CONNECTIVITY_LOST:
                failure = checkConnectivity();
                break;
            case FailureType::SIGNAL_WEAK:
                failure = checkSignalStrength();
                break;
            case FailureType::MODEM_UNRESPONSIVE:
                failure = checkModemResponsiveness();
                break;
            case FailureType::INTERFACE_DOWN:
                failure = checkInterfaceStatus();
                break;
            case FailureType::ROUTING_FAILURE:
                failure = checkRouting();
                break;
            default:
                continue;
        }
        
        if (failure.type != FailureType::UNKNOWN) {
            failures.push_back(failure);
        }
    }
    
    return failures;
}

FailureEvent FailureDetector::checkSessionStatus() {
    if (!m_session_handler) {
        return createFailureEvent(FailureType::SESSION_LOST, 
                                "Session handler not available", 10, false);
    }
    
    if (!m_session_handler->isSessionActive()) {
        return createFailureEvent(FailureType::SESSION_LOST,
                                "QMI data session is not active", 8, true);
    }
    
    return FailureEvent{FailureType::UNKNOWN, "", std::chrono::system_clock::now(), "", "", 0, false};
}

FailureEvent FailureDetector::checkIPConfiguration() {
    if (!m_interface_controller || !m_session_handler) {
        return createFailureEvent(FailureType::IP_CONFIGURATION_LOST,
                                "Required components not available", 10, false);
    }
    
    auto settings = m_session_handler->getCurrentSettings();
    if (settings.interface_name.empty()) {
        return createFailureEvent(FailureType::IP_CONFIGURATION_LOST,
                                "No interface configured", 7, true);
    }
    
    auto status = m_interface_controller->getInterfaceStatus(settings.interface_name);
    if (!status.has_ip) {
        return createFailureEvent(FailureType::IP_CONFIGURATION_LOST,
                                "Interface has no IP address", 7, true);
    }
    
    return FailureEvent{FailureType::UNKNOWN, "", std::chrono::system_clock::now(), "", "", 0, false};
}

FailureEvent FailureDetector::checkDNSResolution() {
    if (!m_interface_controller) {
        return createFailureEvent(FailureType::DNS_FAILURE,
                                "Interface controller not available", 10, false);
    }
    
    // Simple DNS test using ping to a known hostname
    std::string cmd = "ping -c 1 -W 5 google.com >/dev/null 2>&1";
    CommandLogger::logCommand(cmd);
    int result = system(cmd.c_str());
    CommandLogger::logCommandResult(cmd, (WEXITSTATUS(result) == 0) ? "SUCCESS" : "FAILED", WEXITSTATUS(result));
    
    if (WEXITSTATUS(result) != 0) {
        return createFailureEvent(FailureType::DNS_FAILURE,
                                "DNS resolution failed", 5, true);
    }
    
    return FailureEvent{FailureType::UNKNOWN, "", std::chrono::system_clock::now(), "", "", 0, false};
}

FailureEvent FailureDetector::checkConnectivity() {
    if (m_connectivity_monitor) {
        if (!m_connectivity_monitor->isConnected()) {
            return createFailureEvent(FailureType::CONNECTIVITY_LOST,
                                    "Internet connectivity lost", 6, true);
        }
    } else {
        // Fallback connectivity test
        if (!m_interface_controller || !m_interface_controller->testConnectivity()) {
            return createFailureEvent(FailureType::CONNECTIVITY_LOST,
                                    "Basic connectivity test failed", 6, true);
        }
    }
    
    return FailureEvent{FailureType::UNKNOWN, "", std::chrono::system_clock::now(), "", "", 0, false};
}

FailureEvent FailureDetector::checkSignalStrength() {
    if (!m_session_handler) {
        return createFailureEvent(FailureType::SIGNAL_WEAK,
                                "Session handler not available", 10, false);
    }
    
    auto signal_info = m_session_handler->getSignalInfo();
    if (signal_info.rssi < -100) { // Very weak signal
        return createFailureEvent(FailureType::SIGNAL_WEAK,
                                "Signal strength very weak: " + std::to_string(signal_info.rssi) + " dBm",
                                4, false);
    }
    
    return FailureEvent{FailureType::UNKNOWN, "", std::chrono::system_clock::now(), "", "", 0, false};
}

FailureEvent FailureDetector::checkModemResponsiveness() {
    if (!m_session_handler) {
        return createFailureEvent(FailureType::MODEM_UNRESPONSIVE,
                                "Session handler not available", 10, false);
    }
    
    if (!m_session_handler->isModemReady()) {
        return createFailureEvent(FailureType::MODEM_UNRESPONSIVE,
                                "Modem is not responding", 9, true);
    }
    
    return FailureEvent{FailureType::UNKNOWN, "", std::chrono::system_clock::now(), "", "", 0, false};
}

FailureEvent FailureDetector::checkInterfaceStatus() {
    if (!m_interface_controller || !m_session_handler) {
        return createFailureEvent(FailureType::INTERFACE_DOWN,
                                "Required components not available", 10, false);
    }
    
    auto settings = m_session_handler->getCurrentSettings();
    if (settings.interface_name.empty()) {
        return createFailureEvent(FailureType::INTERFACE_DOWN,
                                "No interface configured", 7, true);
    }
    
    auto status = m_interface_controller->getInterfaceStatus(settings.interface_name);
    if (!status.is_up) {
        return createFailureEvent(FailureType::INTERFACE_DOWN,
                                "Network interface is down", 7, true);
    }
    
    return FailureEvent{FailureType::UNKNOWN, "", std::chrono::system_clock::now(), "", "", 0, false};
}

FailureEvent FailureDetector::checkRouting() {
    if (!m_interface_controller) {
        return createFailureEvent(FailureType::ROUTING_FAILURE,
                                "Interface controller not available", 10, false);
    }
    
    // Check if we can reach the gateway
    std::string cmd = "ip route show default";
    CommandLogger::logCommand(cmd);
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        CommandLogger::logCommandResult(cmd, "", -1);
        return createFailureEvent(FailureType::ROUTING_FAILURE,
                                "Cannot check routing table", 8, true);
    }
    
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    int exit_code = pclose(pipe);
    CommandLogger::logCommandResult(cmd, output, WEXITSTATUS(exit_code));
    
    if (output.empty()) {
        return createFailureEvent(FailureType::ROUTING_FAILURE,
                                "No default route found", 6, true);
    }
    
    return FailureEvent{FailureType::UNKNOWN, "", std::chrono::system_clock::now(), "", "", 0, false};
}

std::vector<FailureEvent> FailureDetector::getRecentFailures(int count) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_failure_history.size() <= static_cast<size_t>(count)) {
        return m_failure_history;
    }
    
    return std::vector<FailureEvent>(
        m_failure_history.end() - count, m_failure_history.end());
}

std::vector<FailureEvent> FailureDetector::getFailuresByType(FailureType type) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    std::vector<FailureEvent> filtered_failures;
    std::copy_if(m_failure_history.begin(), m_failure_history.end(),
                std::back_inserter(filtered_failures),
                [type](const FailureEvent& failure) {
                    return failure.type == type;
                });
    
    return filtered_failures;
}

void FailureDetector::clearFailureHistory() {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    m_failure_history.clear();
    
    // Reset failure counts
    for (auto& pair : m_failure_counts) {
        pair.second = 0;
    }
    
    std::cout << "Failure history cleared" << std::endl;
}

int FailureDetector::getFailureCount(FailureType type) const {
    auto it = m_failure_counts.find(type);
    return it != m_failure_counts.end() ? it->second : 0;
}

double FailureDetector::getFailureRate(FailureType type, std::chrono::minutes window) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto window_start = now - window;
    
    int failures_in_window = std::count_if(m_failure_history.begin(), m_failure_history.end(),
                                          [type, window_start](const FailureEvent& failure) {
                                              return failure.type == type && 
                                                     failure.timestamp >= window_start;
                                          });
    
    return static_cast<double>(failures_in_window) / window.count();
}

FailureType FailureDetector::getMostCommonFailure() const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    std::map<FailureType, int> type_counts;
    for (const auto& failure : m_failure_history) {
        type_counts[failure.type]++;
    }
    
    auto max_it = std::max_element(type_counts.begin(), type_counts.end(),
                                  [](const std::pair<FailureType, int>& a,
                                     const std::pair<FailureType, int>& b) {
                                      return a.second < b.second;
                                  });
    
    return max_it != type_counts.end() ? max_it->first : FailureType::UNKNOWN;
}

void FailureDetector::detectionLoop() {
    while (m_detecting) {
        performPeriodicChecks();
        
        std::unique_lock<std::mutex> lock(m_history_mutex);
        m_detection_cv.wait_for(lock, std::chrono::milliseconds(m_detection_interval_ms),
                               [this] { return !m_detecting; });
    }
}

void FailureDetector::performPeriodicChecks() {
    auto failures = performFullDiagnostic();
    
    for (const auto& failure : failures) {
        reportFailure(failure);
    }
    
    if (failures.empty()) {
        std::cout << "Failure detection: All systems healthy" << std::endl;
    }
}

void FailureDetector::reportFailure(const FailureEvent& failure) {
    {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        
        // Increment failure count
        m_failure_counts[failure.type]++;
        
        // Check if threshold is reached
        auto threshold_it = m_failure_thresholds.find(failure.type);
        if (threshold_it != m_failure_thresholds.end() &&
            m_failure_counts[failure.type] < threshold_it->second) {
            return; // Threshold not reached yet
        }
        
        // Add to history
        m_failure_history.push_back(failure);
        
        // Maintain history size
        if (m_failure_history.size() > MAX_HISTORY_SIZE) {
            m_failure_history.erase(m_failure_history.begin());
        }
    }
    
    std::cout << "Failure detected: " << failure.description 
              << " (severity: " << failure.severity << ")" << std::endl;
    
    // Notify callback
    if (m_failure_callback) {
        m_failure_callback(failure);
    }
}

FailureEvent FailureDetector::createFailureEvent(FailureType type, const std::string& description,
                                                int severity, bool auto_recoverable) {
    FailureEvent failure;
    failure.type = type;
    failure.description = description;
    failure.timestamp = std::chrono::system_clock::now();
    failure.severity = severity;
    failure.auto_recoverable = auto_recoverable;
    
    if (m_session_handler) {
        auto device_info = m_session_handler->getDeviceInfo();
        failure.device_path = device_info.device_path;
        
        auto settings = m_session_handler->getCurrentSettings();
        failure.interface_name = settings.interface_name;
    }
    
    return failure;
}
