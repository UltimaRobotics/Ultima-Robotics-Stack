
#ifndef QMI_WATCHDOG_H
#define QMI_WATCHDOG_H

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <json/json.h>

// Data collection status
enum class CollectionStatus {
    SUCCESS = 0,
    FAILED = 1,
    TIMEOUT = 2,
    DEVICE_ERROR = 3,
    PARSE_ERROR = 4
};

// Signal information structure
struct SignalMetrics {
    double rssi = 0.0;          // dBm
    double rsrq = 0.0;          // dB  
    double rsrp = 0.0;          // dBm
    double snr = 0.0;           // dB
    std::string radio_interface;
    CollectionStatus status = CollectionStatus::FAILED;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;
    
    std::string toJson() const;
};



// Network serving system information
struct NetworkInfo {
    std::string registration_state;
    std::string cs_state;           // Circuit Switched
    std::string ps_state;           // Packet Switched
    std::string selected_network;
    std::vector<std::string> radio_interfaces;
    std::string roaming_status;
    std::vector<std::string> data_service_capabilities;
    
    // PLMN information
    std::string mcc;                // Mobile Country Code
    std::string mnc;                // Mobile Network Code
    std::string operator_description;
    
    // Location information
    std::string location_area_code;
    std::string cell_id;
    std::string tracking_area_code;
    
    // Status details
    std::string detailed_status;
    std::string capability;
    bool forbidden = false;
    
    CollectionStatus status = CollectionStatus::FAILED;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;
    
    std::string toJson() const;
};

// RF Band information structure
struct RFBandInfo {
    std::string radio_interface;
    std::string active_band_class;
    std::string active_channel;
    std::string bandwidth;
    CollectionStatus status = CollectionStatus::FAILED;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;
    
    std::string toJson() const;
};

// Health scoring weights
struct HealthWeights {
    double signal_weight = 0.5;     // 50% - Signal quality importance
    double network_weight = 0.35;   // 35% - Network status importance
    double rf_weight = 0.15;        // 15% - RF band stability
};

// Overall health assessment
struct HealthScore {
    double overall_score = 0.0;     // 0-100 scale
    double signal_score = 0.0;
    double network_score = 0.0;
    double rf_score = 0.0;
    
    std::string health_status;      // "EXCELLENT", "GOOD", "FAIR", "POOR", "CRITICAL"
    std::vector<std::string> warnings;
    std::vector<std::string> critical_issues;
    
    std::chrono::system_clock::time_point timestamp;
    
    std::string toJson() const;
};

// Device configuration from JSON
struct DeviceConfig {
    std::string device_path;
    std::string imei;
    std::string model;
    std::string manufacturer;
    bool is_available = false;
    
    // Monitoring configuration
    int collection_interval_ms = 5000;     // 5 seconds default
    int timeout_ms = 10000;                // 10 seconds timeout
    bool enable_health_scoring = true;
    HealthWeights weights;
};

// Failure detection configuration
struct FailureDetectionConfig {
    // Signal thresholds
    double critical_rssi_threshold = -110.0;    // dBm
    double warning_rssi_threshold = -95.0;      // dBm
    double critical_rsrp_threshold = -120.0;    // dBm
    double warning_rsrp_threshold = -105.0;     // dBm
    double critical_rsrq_threshold = -15.0;     // dB
    double warning_rsrq_threshold = -10.0;      // dB
    
    // Collection failure thresholds
    int max_consecutive_failures = 3;
    int failure_detection_window = 10;         // Number of samples to consider
};

// Complete monitoring data snapshot
struct MonitoringSnapshot {
    SignalMetrics signal;
    NetworkInfo network;
    RFBandInfo rf_band;
    HealthScore health;
    
    std::chrono::system_clock::time_point collection_time;
    std::string device_path;
    
    std::string toJson() const;
};

// Callback function types
using DataCollectionCallback = std::function<void(const MonitoringSnapshot&)>;
using FailureDetectionCallback = std::function<void(const std::string&, const std::vector<std::string>&)>;

Json::Value loadMonitoringConfig(const std::string& filePath);
Json::Value loadFailureDetectionConfig(const std::string& filePath);

class QMIWatchdog {
public:
    QMIWatchdog();
    ~QMIWatchdog();
    
    // Configuration
    bool loadDeviceConfig(const std::string& config_json);
    bool loadDeviceConfigFromFile(const std::string& config_file_path);
    void setFailureDetectionConfig(const FailureDetectionConfig& config);
    void setHealthWeights(const HealthWeights& weights);
    
    // Monitoring control
    bool startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Data collection (single shot)
    SignalMetrics collectSignalMetrics();
    NetworkInfo collectNetworkInfo();
    RFBandInfo collectRFBandInfo();
    MonitoringSnapshot collectFullSnapshot();
    
    // Health assessment
    HealthScore calculateHealthScore(const MonitoringSnapshot& snapshot);
    std::vector<std::string> detectFailures(const MonitoringSnapshot& snapshot);
    
    // Callbacks
    void setDataCollectionCallback(DataCollectionCallback callback);
    void setFailureDetectionCallback(FailureDetectionCallback callback);
    
    // Statistics and status
    struct WatchdogStats {
        uint64_t total_collections = 0;
        uint64_t successful_collections = 0;
        uint64_t failed_collections = 0;
        uint64_t detected_failures = 0;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_collection_time;
        
        std::string toJson() const;
    };
    
    WatchdogStats getStatistics() const;
    std::string getStatus() const;
    
private:
    // Configuration
    DeviceConfig m_device_config;
    FailureDetectionConfig m_failure_config;
    
    // Monitoring state
    std::atomic<bool> m_monitoring;
    std::unique_ptr<std::thread> m_monitor_thread;
    mutable std::mutex m_stats_mutex;
    WatchdogStats m_stats;
    
    // Callbacks
    DataCollectionCallback m_data_callback;
    FailureDetectionCallback m_failure_callback;
    
    // Failure tracking
    std::vector<CollectionStatus> m_recent_collection_status;
    mutable std::mutex m_failure_tracking_mutex;
    
    // Core monitoring loop
    void monitoringLoop();
    
    // QMI command execution
    std::string executeQMICommand(const std::string& command, int timeout_ms = 10000);
    
    // Data parsing methods
    SignalMetrics parseSignalInfo(const std::string& qmi_output);
    NetworkInfo parseNetworkInfo(const std::string& qmi_output);
    RFBandInfo parseRFBandInfo(const std::string& qmi_output);
    
    // Health scoring methods
    double calculateSignalScore(const SignalMetrics& signal);
    double calculateNetworkScore(const NetworkInfo& network);
    double calculateRFScore(const RFBandInfo& rf);
    
    // Failure detection methods
    std::vector<std::string> checkSignalFailures(const SignalMetrics& signal);
    std::vector<std::string> checkNetworkFailures(const NetworkInfo& network);
    std::vector<std::string> checkCollectionFailures();
    
    // Utility methods
    std::string getCurrentTimestamp() const;
    double extractNumericValue(const std::string& text, const std::string& unit = "");
    std::string extractStringValue(const std::string& text, const std::string& field);
    bool parseDeviceConfigJson(const std::string& json);
    void updateCollectionStatus(CollectionStatus status);
    void printJsonToTerminal(const std::string& json_data, const std::string& data_type);
};

#endif // QMI_WATCHDOG_H
