#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "metrics_reporter.h"
#include <json/json.h>

// Forward declarations
class QMISessionHandler;
class ConnectionStateMachine;
class InterfaceController;
class ConnectivityMonitor;
class FailureDetector;
class RecoveryEngine;
class MetricsReporter;
class ConnectionLifecycleManager;
class IPMonitor;

enum class ConnectionState {
    IDLE,
    MODEM_ONLINE,
    SESSION_ACTIVE,
    IP_CONFIGURED,
    CONNECTED,
    RECONNECTING,
    ERROR
};

struct DeviceInfo {
    std::string device_path;
    std::string imei;
    std::string model;
    std::string manufacturer;
    bool is_available;
};

enum class CellularMode {
    AUTO = 0,
    LTE_ONLY = 1,
    FiveG_ONLY = 2,
    ThreeGPP_LEGACY = 3,
    WCDMA_GSM = 4,
    GSM_ONLY = 5,
    LTE_FiveG = 6
};

struct CellularModeConfig {
    CellularMode mode;
    std::vector<int> preferred_bands;  // Preferred frequency bands
    int preference_duration;           // How long to enforce preference (seconds)
    bool force_mode_selection;         // Force mode even if signal is weak
    std::string mode_description;      // Human readable description

    CellularModeConfig() : 
        mode(CellularMode::AUTO), 
        preference_duration(0), 
        force_mode_selection(false),
        mode_description("Automatic") {}
};

struct ConnectionConfig {
    std::string apn;
    std::string username;
    std::string password;
    int ip_type;  // 4 for IPv4, 6 for IPv6, 46 for dual
    bool auto_connect;
    int retry_attempts;
    int retry_delay_ms;
    bool enable_monitoring;
    int health_check_interval_ms;

    // New cellular mode configuration
    CellularModeConfig cellular_mode_config;
    bool enforce_mode_before_connection;

    ConnectionConfig() : 
        ip_type(4), 
        auto_connect(true), 
        retry_attempts(3), 
        retry_delay_ms(5000),
        enable_monitoring(false), 
        health_check_interval_ms(30000),
        enforce_mode_before_connection(true) {}
};

struct ConnectionMetrics {
    int signal_strength;
    std::string network_type;
    std::string ip_address;
    std::string dns_primary;
    std::string dns_secondary;
    std::string interface_name;
    bool is_connected;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    std::chrono::system_clock::time_point connected_since;
};

using StateChangeCallback = std::function<void(ConnectionState, const std::string&)>;
using MetricsCallback = std::function<void(const ConnectionMetrics&)>;

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    // Initialization
    bool initialize(const std::string& device_json);
    bool initializeFromBasicProfile(const Json::Value& basic_profile);
    bool initializeFromAdvancedProfile(const Json::Value& advanced_profile);

    // Connection management
    bool connect(const ConnectionConfig& config);
    bool disconnect();
    bool reconnect();

    // State management
    ConnectionState getCurrentState() const;
    std::string getStateString() const;
    bool isConnected() const;

    // Configuration
    void setConnectionConfig(const ConnectionConfig& config);
    ConnectionConfig getConnectionConfig() const;

    // Callbacks
    void setStateChangeCallback(StateChangeCallback callback);
    void setMetricsCallback(MetricsCallback callback);

    // Monitoring
    void startMonitoring();
    void stopMonitoring();
    ConnectionMetrics getCurrentMetrics();

    // Device management
    std::vector<DeviceInfo> getAvailableDevices();
    bool selectDevice(const std::string& device_path);
    DeviceInfo getCurrentDevice() const;

    // JSON operations
    std::string getStatusJson() const;
    std::string getMetricsJson() const;
    std::string getConfigJson() const;
    bool loadConfigFromJson(const std::string& json_str);

    // Recovery operations
    void enableAutoRecovery(bool enable);
    void triggerRecovery();
    void resetModem();

    // Hot-disconnect support
    void performEmergencyCleanup();
    static ConnectionManager* getActiveInstance();

    // Cellular mode configuration
    bool setCellularMode(const CellularModeConfig& mode_config);
    CellularMode getCurrentCellularMode();
    std::string getCellularModeString(CellularMode mode);
    bool loadCellularConfigFromJson(const Json::Value& config);

    // Interface management
    std::vector<std::string> getExistingWWANInterfaces();
    std::string generateUniqueInterfaceName(const std::string& base_name = "wwan");
    bool isInterfaceNameAvailable(const std::string& interface_name);

    // Smart cleanup and optimal interface selection
    bool performStartupCleanup();
    std::string selectOptimalInterface(const std::string& device_path);
    bool cleanupInactiveConnections();
    std::vector<std::string> getActiveWWANInterfaces();
    std::vector<std::string> getInactiveWWANInterfaces();

private:
    void transitionToState(ConnectionState new_state, const std::string& reason = "");
    void notifyStateChange(ConnectionState state, const std::string& reason);
    void notifyMetrics(const ConnectionMetrics& metrics);
    void parseDeviceJson(const std::string& device_json);

    // Components
    std::unique_ptr<QMISessionHandler> m_session_handler;
    std::unique_ptr<ConnectionStateMachine> m_state_machine;
    std::unique_ptr<InterfaceController> m_interface_controller;
    std::unique_ptr<ConnectivityMonitor> m_connectivity_monitor;
    std::unique_ptr<FailureDetector> m_failure_detector;
    std::unique_ptr<RecoveryEngine> m_recovery_engine;
    std::unique_ptr<MetricsReporter> m_metrics_reporter;
    std::unique_ptr<ConnectionLifecycleManager> m_lifecycle_manager; // Added lifecycle manager
    std::unique_ptr<IPMonitor> m_ip_monitor; // Added IP monitor

    // State
    ConnectionState m_current_state;
    DeviceInfo m_current_device;
    ConnectionConfig m_config;
    ConnectionMetrics m_metrics;

    // Threading
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_config_mutex;
    mutable std::mutex m_metrics_mutex;
    std::condition_variable m_state_cv;

    // Callbacks
    StateChangeCallback m_state_callback;
    MetricsCallback m_metrics_callback;

    // Control flags
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_monitoring_enabled;
    std::atomic<bool> m_auto_recovery_enabled;

    // Static instance for signal handling
    static ConnectionManager* s_active_instance;
};

#endif // CONNECTION_MANAGER_H