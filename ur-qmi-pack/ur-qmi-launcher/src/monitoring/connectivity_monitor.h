
#ifndef CONNECTIVITY_MONITOR_H
#define CONNECTIVITY_MONITOR_H

#include "connection_manager.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

struct ConnectivityTest {
    std::string target;
    int timeout_ms;
    bool success;
    int response_time_ms;
    std::string error_message;
};

using ConnectivityCallback = std::function<void(bool /* connected */, const std::string& /* reason */)>;

class ConnectivityMonitor {
public:
    ConnectivityMonitor();
    ~ConnectivityMonitor();
    
    // Monitoring control
    void startMonitoring(int interval_ms = 30000);
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Configuration
    void setMonitoringInterval(int interval_ms);
    void addTestTarget(const std::string& target, int timeout_ms = 5000);
    void removeTestTarget(const std::string& target);
    void clearTestTargets();
    void setConnectivityCallback(ConnectivityCallback callback);
    
    // Manual testing
    bool testConnectivity();
    std::vector<ConnectivityTest> performConnectivityTests();
    ConnectivityTest pingTest(const std::string& target, int timeout_ms = 5000);
    ConnectivityTest dnsTest(const std::string& hostname = "google.com", int timeout_ms = 5000);
    ConnectivityTest httpTest(const std::string& url = "http://detectportal.firefox.com/canonical.html", 
                             int timeout_ms = 10000);
    
    // Status
    bool isConnected() const;
    std::chrono::system_clock::time_point getLastSuccessfulTest() const;
    std::chrono::system_clock::time_point getLastFailedTest() const;
    int getConsecutiveFailures() const;
    int getConsecutiveSuccesses() const;
    
    // Statistics
    double getSuccessRate() const;
    int getAverageResponseTime() const;
    std::vector<ConnectivityTest> getRecentTests(int count = 10) const;

private:
    void monitoringLoop();
    void performPeriodicTest();
    void updateConnectivityStatus(bool connected, const std::string& reason = "");
    
    std::vector<std::pair<std::string, int>> m_test_targets;
    std::vector<ConnectivityTest> m_test_history;
    
    std::unique_ptr<std::thread> m_monitor_thread;
    std::atomic<bool> m_monitoring;
    std::atomic<bool> m_connected;
    std::atomic<int> m_interval_ms;
    std::atomic<int> m_consecutive_failures;
    std::atomic<int> m_consecutive_successes;
    
    std::chrono::system_clock::time_point m_last_successful_test;
    std::chrono::system_clock::time_point m_last_failed_test;
    
    ConnectivityCallback m_connectivity_callback;
    
    mutable std::mutex m_targets_mutex;
    mutable std::mutex m_history_mutex;
    std::condition_variable m_monitor_cv;
    
    static const size_t MAX_HISTORY_SIZE = 100;
};

#endif // CONNECTIVITY_MONITOR_H
