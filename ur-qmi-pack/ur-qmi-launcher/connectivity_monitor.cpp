
#include "connectivity_monitor.h"
#include "command_logger.h"
#include "timeout_config.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>

ConnectivityMonitor::ConnectivityMonitor()
    : m_monitoring(false)
    , m_connected(false)
    , m_interval_ms(30000)
    , m_consecutive_failures(0)
    , m_consecutive_successes(0)
{
    // Default test targets
    m_test_targets.push_back({"8.8.8.8", 5000});
    m_test_targets.push_back({"1.1.1.1", 5000});
}

ConnectivityMonitor::~ConnectivityMonitor() {
    stopMonitoring();
}

void ConnectivityMonitor::startMonitoring(int interval_ms) {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    
    if (m_monitoring) {
        return;
    }
    
    m_interval_ms = interval_ms;
    m_monitoring = true;
    
    m_monitor_thread = std::make_unique<std::thread>(&ConnectivityMonitor::monitoringLoop, this);
    std::cout << "Connectivity monitoring started (interval: " << interval_ms << "ms)" << std::endl;
}

void ConnectivityMonitor::stopMonitoring() {
    {
        std::lock_guard<std::mutex> lock(m_targets_mutex);
        m_monitoring = false;
    }
    
    m_monitor_cv.notify_all();
    
    if (m_monitor_thread && m_monitor_thread->joinable()) {
        m_monitor_thread->join();
    }
    
    std::cout << "Connectivity monitoring stopped" << std::endl;
}

bool ConnectivityMonitor::isMonitoring() const {
    return m_monitoring;
}

void ConnectivityMonitor::setMonitoringInterval(int interval_ms) {
    m_interval_ms = interval_ms;
}

void ConnectivityMonitor::addTestTarget(const std::string& target, int timeout_ms) {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    
    // Remove existing target if present
    m_test_targets.erase(
        std::remove_if(m_test_targets.begin(), m_test_targets.end(),
                      [&target](const std::pair<std::string, int>& t) {
                          return t.first == target;
                      }),
        m_test_targets.end());
    
    m_test_targets.push_back({target, timeout_ms});
    std::cout << "Added test target: " << target << " (timeout: " << timeout_ms << "ms)" << std::endl;
}

void ConnectivityMonitor::removeTestTarget(const std::string& target) {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    
    m_test_targets.erase(
        std::remove_if(m_test_targets.begin(), m_test_targets.end(),
                      [&target](const std::pair<std::string, int>& t) {
                          return t.first == target;
                      }),
        m_test_targets.end());
    
    std::cout << "Removed test target: " << target << std::endl;
}

void ConnectivityMonitor::clearTestTargets() {
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    m_test_targets.clear();
    std::cout << "Cleared all test targets" << std::endl;
}

void ConnectivityMonitor::setConnectivityCallback(ConnectivityCallback callback) {
    m_connectivity_callback = callback;
}

bool ConnectivityMonitor::testConnectivity() {
    std::vector<ConnectivityTest> results = performConnectivityTests();
    
    // Count successful tests
    int successful_tests = std::count_if(results.begin(), results.end(),
                                        [](const ConnectivityTest& test) {
                                            return test.success;
                                        });
    
    // Consider connected if at least 50% of tests pass
    bool connected = (results.size() > 0) && (successful_tests >= static_cast<int>(results.size()) / 2);
    
    updateConnectivityStatus(connected, 
        connected ? "Connectivity verified" : "Connectivity tests failed");
    
    return connected;
}

std::vector<ConnectivityTest> ConnectivityMonitor::performConnectivityTests() {
    std::vector<ConnectivityTest> results;
    
    std::lock_guard<std::mutex> lock(m_targets_mutex);
    
    // Ping tests
    for (const auto& target : m_test_targets) {
        results.push_back(pingTest(target.first, target.second));
    }
    
    // DNS test
    results.push_back(dnsTest());
    
    // HTTP test
    results.push_back(httpTest());
    
    // Store in history
    {
        std::lock_guard<std::mutex> history_lock(m_history_mutex);
        for (const auto& result : results) {
            m_test_history.push_back(result);
            
            // Maintain history size
            if (m_test_history.size() > MAX_HISTORY_SIZE) {
                m_test_history.erase(m_test_history.begin());
            }
        }
    }
    
    return results;
}

ConnectivityTest ConnectivityMonitor::pingTest(const std::string& target, int timeout_ms) {
    ConnectivityTest test;
    test.target = target;
    // Use global timeout config if default timeout is requested
    test.timeout_ms = (timeout_ms > 0) ? timeout_ms : g_timeout_config.ping_timeout;
    test.success = false;
    test.response_time_ms = -1;
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::string cmd = "ping -c 1 -W " + std::to_string(timeout_ms / 1000) + " " + target + " 2>&1";
    
    CommandLogger::logCommand(cmd);
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        CommandLogger::logCommandResult(cmd, "", -1);
        test.error_message = "Failed to execute ping command";
        return test;
    }
    
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int exit_code = pclose(pipe);
    CommandLogger::logCommandResult(cmd, output, WEXITSTATUS(exit_code));
    
    auto end_time = std::chrono::steady_clock::now();
    test.response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    if (exit_code == 0) {
        test.success = true;
        
        // Extract ping time from output
        std::regex time_regex("time=([0-9.]+)");
        std::smatch match;
        if (std::regex_search(output, match, time_regex)) {
            test.response_time_ms = static_cast<int>(std::stof(match[1].str()));
        }
    } else {
        test.error_message = "Ping failed: " + output.substr(0, 100);
    }
    
    return test;
}

ConnectivityTest ConnectivityMonitor::dnsTest(const std::string& hostname, int timeout_ms) {
    ConnectivityTest test;
    test.target = hostname;
    // Use global timeout config if default timeout is requested
    test.timeout_ms = (timeout_ms > 0) ? timeout_ms : g_timeout_config.dns_resolution_timeout;
    test.success = false;
    test.response_time_ms = -1;
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::string cmd = "nslookup " + hostname + " 2>&1";
    
    CommandLogger::logCommand(cmd);
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        CommandLogger::logCommandResult(cmd, "", -1);
        test.error_message = "Failed to execute nslookup command";
        return test;
    }
    
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int exit_code = pclose(pipe);
    CommandLogger::logCommandResult(cmd, output, WEXITSTATUS(exit_code));
    
    auto end_time = std::chrono::steady_clock::now();
    test.response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // Check if DNS resolution was successful
    if (output.find("Address:") != std::string::npos && 
        output.find("can't find") == std::string::npos &&
        output.find("NXDOMAIN") == std::string::npos) {
        test.success = true;
    } else {
        test.error_message = "DNS resolution failed";
    }
    
    return test;
}

ConnectivityTest ConnectivityMonitor::httpTest(const std::string& url, int timeout_ms) {
    ConnectivityTest test;
    test.target = url;
    // Use global timeout config if default timeout is requested
    test.timeout_ms = (timeout_ms > 0) ? timeout_ms : g_timeout_config.http_test_timeout;
    test.success = false;
    test.response_time_ms = -1;
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::string cmd = "curl -s --connect-timeout " + std::to_string(timeout_ms / 1000) + 
                     " --max-time " + std::to_string(timeout_ms / 1000) + 
                     " -o /dev/null -w '%{http_code}' " + url + " 2>/dev/null";
    
    CommandLogger::logCommand(cmd);
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        CommandLogger::logCommandResult(cmd, "", -1);
        test.error_message = "Failed to execute curl command";
        return test;
    }
    
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int exit_code = pclose(pipe);
    CommandLogger::logCommandResult(cmd, output, WEXITSTATUS(exit_code));
    
    auto end_time = std::chrono::steady_clock::now();
    test.response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // Check HTTP response code
    try {
        int http_code = std::stoi(output);
        if (http_code >= 200 && http_code < 400) {
            test.success = true;
        } else {
            test.error_message = "HTTP error: " + std::to_string(http_code);
        }
    } catch (const std::exception& e) {
        test.error_message = "Failed to parse HTTP response";
    }
    
    return test;
}

bool ConnectivityMonitor::isConnected() const {
    return m_connected;
}

std::chrono::system_clock::time_point ConnectivityMonitor::getLastSuccessfulTest() const {
    return m_last_successful_test;
}

std::chrono::system_clock::time_point ConnectivityMonitor::getLastFailedTest() const {
    return m_last_failed_test;
}

int ConnectivityMonitor::getConsecutiveFailures() const {
    return m_consecutive_failures;
}

int ConnectivityMonitor::getConsecutiveSuccesses() const {
    return m_consecutive_successes;
}

double ConnectivityMonitor::getSuccessRate() const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_test_history.empty()) {
        return 0.0;
    }
    
    int successful = std::count_if(m_test_history.begin(), m_test_history.end(),
                                  [](const ConnectivityTest& test) {
                                      return test.success;
                                  });
    
    return static_cast<double>(successful) / m_test_history.size() * 100.0;
}

int ConnectivityMonitor::getAverageResponseTime() const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_test_history.empty()) {
        return 0;
    }
    
    int total_time = 0;
    int valid_tests = 0;
    
    for (const auto& test : m_test_history) {
        if (test.success && test.response_time_ms > 0) {
            total_time += test.response_time_ms;
            valid_tests++;
        }
    }
    
    return valid_tests > 0 ? total_time / valid_tests : 0;
}

std::vector<ConnectivityTest> ConnectivityMonitor::getRecentTests(int count) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    
    if (m_test_history.size() <= static_cast<size_t>(count)) {
        return m_test_history;
    }
    
    return std::vector<ConnectivityTest>(
        m_test_history.end() - count, m_test_history.end());
}

void ConnectivityMonitor::monitoringLoop() {
    while (m_monitoring) {
        performPeriodicTest();
        
        std::unique_lock<std::mutex> lock(m_targets_mutex);
        m_monitor_cv.wait_for(lock, std::chrono::milliseconds(m_interval_ms),
                             [this] { return !m_monitoring; });
    }
}

void ConnectivityMonitor::performPeriodicTest() {
    bool connected = testConnectivity();
    
    if (connected) {
        std::cout << "Connectivity check: CONNECTED" << std::endl;
    } else {
        std::cout << "Connectivity check: FAILED" << std::endl;
    }
}

void ConnectivityMonitor::updateConnectivityStatus(bool connected, const std::string& reason) {
    bool status_changed = (m_connected != connected);
    m_connected = connected;
    
    auto now = std::chrono::system_clock::now();
    
    if (connected) {
        m_last_successful_test = now;
        m_consecutive_successes++;
        m_consecutive_failures = 0;
    } else {
        m_last_failed_test = now;
        m_consecutive_failures++;
        m_consecutive_successes = 0;
    }
    
    if (status_changed && m_connectivity_callback) {
        m_connectivity_callback(connected, reason);
    }
}
