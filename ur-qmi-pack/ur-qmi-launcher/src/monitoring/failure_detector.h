#ifndef FAILURE_DETECTOR_H
#define FAILURE_DETECTOR_H

#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <functional>
#include <map> // Added for maps
#include <memory> // Added for unique_ptr
#include <thread> // Added for thread
#include <condition_variable> // Added for condition_variable

// Forward declarations
class QMISessionHandler;
class InterfaceController;
class ConnectivityMonitor;

// Enum for failure types
enum class FailureType {
    UNKNOWN,
    SESSION_LOST,
    IP_CONFIGURATION_LOST,
    DNS_FAILURE,
    CONNECTIVITY_LOST,
    SIGNAL_WEAK,
    MODEM_UNRESPONSIVE,
    INTERFACE_DOWN,
    ROUTING_FAILURE
};

// Structure for failure events
struct FailureEvent {
    FailureType type;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    std::string device_path; // Assuming this is still relevant based on context
    std::string interface_name; // Assuming this is still relevant based on context
    int severity;  // 1-10 scale
    bool auto_recoverable;
};

// Callback type for reporting failures
using FailureCallback = std::function<void(const FailureEvent&)>;

// Class for detecting and managing failures
class FailureDetector {
public:
    // Constructor
    FailureDetector(QMISessionHandler* session_handler,
                   InterfaceController* interface_controller,
                   ConnectivityMonitor* connectivity_monitor);
    // Destructor
    ~FailureDetector();

    // Detection control methods
    void startDetection();
    void stopDetection();
    bool isDetecting() const;

    // Configuration methods
    void setFailureCallback(FailureCallback callback);
    void setDetectionInterval(int interval_ms);
    void enableFailureType(FailureType type, bool enable = true);
    void setFailureThreshold(FailureType type, int threshold);

    // Manual diagnostic check methods
    std::vector<FailureEvent> performFullDiagnostic();
    FailureEvent checkSessionStatus();
    FailureEvent checkIPConfiguration();
    FailureEvent checkDNSResolution();
    FailureEvent checkConnectivity();
    FailureEvent checkSignalStrength();
    FailureEvent checkModemResponsiveness();
    FailureEvent checkInterfaceStatus();
    FailureEvent checkRouting();

    // Failure history management methods
    std::vector<FailureEvent> getRecentFailures(int count = 10) const;
    std::vector<FailureEvent> getFailuresByType(FailureType type) const;
    void clearFailureHistory();

    // Statistics methods
    int getFailureCount(FailureType type) const;
    double getFailureRate(FailureType type, std::chrono::minutes window) const;
    FailureType getMostCommonFailure() const;

private:
    // Internal methods for detection loop and reporting
    void detectionLoop();
    void performPeriodicChecks();
    void reportFailure(const FailureEvent& failure);
    FailureEvent createFailureEvent(FailureType type, const std::string& description,
                                   int severity = 5, bool auto_recoverable = true);

    // Member variables
    QMISessionHandler* m_session_handler;
    InterfaceController* m_interface_controller;
    ConnectivityMonitor* m_connectivity_monitor;

    std::vector<FailureEvent> m_failure_history;
    std::map<FailureType, bool> m_enabled_checks;
    std::map<FailureType, int> m_failure_thresholds;
    std::map<FailureType, int> m_failure_counts;

    FailureCallback m_failure_callback;

    std::unique_ptr<std::thread> m_detection_thread;
    std::atomic<bool> m_detecting;
    std::atomic<int> m_detection_interval_ms;

    mutable std::mutex m_history_mutex;
    std::condition_variable m_detection_cv;

    static const size_t MAX_HISTORY_SIZE = 200;
};

#endif // FAILURE_DETECTOR_H