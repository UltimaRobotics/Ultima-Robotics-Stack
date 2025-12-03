#ifndef QMI_SESSION_HANDLER_H
#define QMI_SESSION_HANDLER_H

#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>

struct SessionInfo {
    uint32_t connection_id;
    std::string packet_data_handle;
    std::string apn;
    std::string ip_address;
    std::string gateway;
    std::string dns_primary;
    std::string dns_secondary;
    int ip_type;
    std::string auth_type;
    bool is_active;
    std::string last_error;
    int retry_count;
};

struct PacketServiceStatus {
    bool connected;
    std::string connection_status;
    uint32_t data_bearer_technology;
};

struct CurrentSettings {
    std::string interface_name;
    std::string ip_address;
    std::string subnet_mask;
    std::string gateway;
    std::string dns_primary;
    std::string dns_secondary;
    uint32_t mtu;
};

struct SignalInfo {
    int rssi;
    int rsrp;
    int rsrq;
    int sinr;
    std::string network_type;
    std::string band;
    std::string carrier;
};

enum class ModemTechnology {
    AUTOMATIC = 0,
    LTE_ONLY = 1,
    FIVE_G_ONLY = 2,
    THREE_GPP_LEGACY = 3,
    WCDMA_GSM_AUTO = 4,
    GSM_ONLY = 5,
    LTE_FIVE_G_AUTO = 6
};

struct NetworkModePreference {
    ModemTechnology technology;
    std::vector<int> bands;
    bool persistent;
};

// Structure to hold connection details for better tracking and cleanup
struct QMIConnectionDetails {
    uint32_t connection_id;
    std::string packet_data_handle;
    std::string client_id;
    bool is_active;
};


class QMISessionHandler {
public:
    QMISessionHandler(const std::string& device_path, const std::string& interface_name = "");
    ~QMISessionHandler();

    // Enhanced session management following enhanced_qmi_connect.sh pattern
    bool initializeModem();
    bool resetWDSService();

    // Cellular mode configuration
    bool setCellularMode(ModemTechnology mode, const std::vector<int>& preferred_bands = {});
    bool setNetworkModePreference(const NetworkModePreference& preference);
    ModemTechnology getCurrentModemTechnology();
    bool enforceNetworkMode(ModemTechnology mode);
    std::string getModemTechnologyString(ModemTechnology mode);
    bool prepareInterface(const std::string& interface_name = "");
    bool startDataSession(const std::string& apn, int ip_type = 4, 
                         const std::string& username = "", 
                         const std::string& password = "",
                         const std::string& auth_type = "none",
                         const std::string& interface_name = "");
    bool verifyConnection();
    bool retrieveConnectionSettings();
    bool stopDataSession();
    bool stopDataSession(uint32_t connection_id);

    // Multi-connection support
    std::string getAssignedInterfaceName() const;
    void setInterfaceName(const std::string& interface_name);
    std::string getInterfaceName() const;

    // Connection status
    bool isSessionActive() const;
    bool isPacketServiceConnected();
    bool validateConnectionParameters();
    bool validateConnection();
    uint32_t getCurrentConnectionId() const;

    std::string autoDetectInterfaceName() const;
    bool isInterfaceAvailable(const std::string& interface_name) const;
    std::string getDeviceInstanceId() const;
    static std::vector<std::string> getAvailableInterfaces();
    static std::string findNextAvailableInterface(const std::string& base_name = "wwan");

    // Enhanced connection management with retry logic
    bool connectWithRetries(const std::string& apn, int ip_type = 4, 
                           const std::string& username = "", 
                           const std::string& password = "",
                           const std::string& auth_type = "none",
                           int max_retries = 3);

    // Diagnostic and analysis functions
    bool diagnoseConnectionPrerequisites();
    bool performConnectionDiagnostics();
    std::string analyzeConnectionError(const std::string& error_output);
    bool attemptConnectionRecovery(const std::string& error_type);

    // Enhanced status queries
    PacketServiceStatus getPacketServiceStatus() const;
    CurrentSettings getCurrentSettings();
    // Method to get detailed connection information for better cleanup
    QMIConnectionDetails getConnectionDetails();
    SignalInfo getSignalInfo();

    // Modem control following script pattern
    bool initialize();
    bool isModemReady();
    bool setModemOnline();
    bool setModemOffline();
    bool setModemLowPower();
    bool resetModem();
    bool stopModemManager();

    // Connection validation with enhanced checks
    // Method to get detailed connection information for better cleanup
    // QMIConnectionDetails getConnectionDetails(); // Duplicate declaration, removed
    // bool isSessionActive(); // Already declared above
    // bool isPacketServiceConnected(); // Already declared above
    // bool validateConnectionParameters(); // Already declared above
    // bool validateConnection(); // Already declared above
    // uint32_t getCurrentConnectionId() const; // Already declared above


    // Raw command execution
    std::string executeQMICommand(const std::string& command);
    
    // Mutex-protected command execution helpers
    std::string executeWDSCommand(const std::string& command);
    std::string executeDMSCommand(const std::string& command);

    // Client management
    bool openClient();
    void releaseClient();
    SessionInfo getSessionInfo() const;

    // Device info
    struct DeviceInfo {
        std::string device_path;
        std::string imei;
        std::string model;
        std::string manufacturer;
        std::string firmware_version;
    };
    DeviceInfo getDeviceInfo() const;
    std::string getDevicePath() const;
    std::string getIMEI() const;

private:
    // Enhanced command execution with detailed logging and error analysis
    std::string executeCommandWithTimeout(const std::string& command, int timeout_seconds = 15);

    // Enhanced parsing functions following script patterns
    bool parseConnectionSettings(const std::string& output, CurrentSettings& settings);
    bool parseSignalInfo(const std::string& output, SignalInfo& signal);
    bool parsePacketStatus(const std::string& output, PacketServiceStatus& status);
    uint32_t extractConnectionId(const std::string& output);
    std::string extractPacketDataHandle(const std::string& output);

    // Network parameter extraction (matching script methods)
    std::string extractIPValue(const std::string& line);
    std::string extractNumericValue(const std::string& line);
    bool validateIPAddress(const std::string& ip);
    bool validateSubnetMask(const std::string& mask);
    bool validateGateway(const std::string& gateway, const std::string& ip, const std::string& mask);

    // Error analysis and recovery
    std::string categorizeQMIError(const std::string& error_output);
    bool isRetryableError(const std::string& error_type);
    void logConnectionAttempt(int attempt, const std::string& result);

    // Interface management
    bool bringInterfaceDown(const std::string& interface_name);
    bool bringInterfaceUp(const std::string& interface_name);

    // Legacy compatibility methods
    std::string executeCommand(const std::string& command) const;
    std::string parseCommandOutput(const std::string& output, const std::string& field) const;
    std::vector<std::string> parseMultipleFields(const std::string& output, const std::string& field);
    uint32_t parseConnectionId(const std::string& output);
    PacketServiceStatus parsePacketServiceStatus(const std::string& output) const;
    CurrentSettings parseCurrentSettings(const std::string& output);
    SignalInfo parseSignalInfo(const std::string& output);

    std::string m_device_path;
    SessionInfo m_session_info;
    mutable std::mutex m_session_mutex;
    mutable std::mutex m_wds_mutex;      // Mutex for WDS operations (--wds-*)
    mutable std::mutex m_dms_mutex;      // Mutex for DMS operations (--dms-*)
    std::atomic<bool> m_session_active;
    std::string m_interface_name;
    std::string m_device_instance_id;  // Unique identifier for this device instance
    int m_connection_timeout;
    int m_max_retries;
    bool m_interface_auto_detected;
    uint32_t m_client_id;
    uint32_t m_connection_id;
    std::string m_packet_data_handle;
};

#endif // QMI_SESSION_HANDLER_H