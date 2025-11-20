
#ifndef QMI_SCANNER_H
#define QMI_SCANNER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

#include <gateway.hpp>
#include <ThreadManager.hpp>

class QMIDevScanAgent;
class QMIDeviceRegistry;

struct SIMStatus {
    std::string card_state;
    std::string upin_state;
    int upin_retries;
    int upuk_retries;
    std::string application_type;
    std::string application_state;
    std::string application_id;
    std::string personalization_state;
    bool upin_replaces_pin1;
    std::string pin1_state;
    int pin1_retries;
    int puk1_retries;
    std::string pin2_state;
    int pin2_retries;
    int puk2_retries;
    std::string primary_gw_slot;
    std::string primary_gw_application;
    std::string primary_1x_status;
    std::string secondary_gw_status;
    std::string secondary_1x_status;
};

struct DeviceProfile {
    std::string path;          // /dev/cdc-wdm0
    std::string imei;
    std::string model;
    std::string firmware;
    std::vector<std::string> bands;
    bool sim_present;
    bool pin_locked;
    bool gps_supported;
    uint8_t max_carriers;
};

struct AdvancedDeviceProfile {
    DeviceProfile basic;
    std::string manufacturer;
    std::string msisdn;
    std::string power_state;
    std::string hardware_revision;
    std::string operating_mode;
    std::string prl_version;
    std::string activation_state;
    std::string user_lock_state;
    std::string band_capabilities;
    std::string factory_sku;
    std::string software_version;
    std::string iccid;
    std::string imsi;
    std::string uim_state;
    std::string pin_status;
    std::string time;
    std::vector<std::string> stored_images;
    std::string firmware_preference;
    std::string boot_image_download_mode;
    std::string usb_composition;
    std::string mac_address_wlan;
    std::string mac_address_bt;
};

// Legacy struct for backward compatibility
struct QMIDevice {
    std::string device_path;
    std::string imei;
    std::string model;
    std::string manufacturer;
    std::string firmware_version;
    std::vector<std::string> supported_bands;
    bool is_available;
    std::string action;        // "added" or "removed"
    SIMStatus sim_status;
};

enum class ProfileMode {
    BASIC,
    ADVANCED,
    MANAGER
};

class QMIScanner {
public:
    using DeviceCallback = std::function<void(const QMIDevice&, bool /* added */)>;
    using ProfileCallback = std::function<void(const DeviceProfile&, bool /* added */)>;
    using AdvancedProfileCallback = std::function<void(const AdvancedDeviceProfile&, bool /* added */)>;
    
    QMIScanner();
    ~QMIScanner();
    
    // Initialize the scanner and perform initial device scan
    bool initialize(ProfileMode mode = ProfileMode::BASIC);
    
    // Start monitoring for device hotplug events
    void startMonitoring();
    
    // Stop monitoring
    void stopMonitoring();
    
    // Set callback for device events
    void setDeviceCallback(DeviceCallback callback);
    void setProfileCallback(ProfileCallback callback);
    void setAdvancedProfileCallback(AdvancedProfileCallback callback);
    
    // Get current list of devices
    std::vector<QMIDevice> getCurrentDevices() const;
    std::vector<DeviceProfile> getCurrentProfiles() const;
    std::vector<AdvancedDeviceProfile> getCurrentAdvancedProfiles() const;
    
    // Perform a one-time scan for devices
    std::vector<QMIDevice> scanDevices();
    std::vector<DeviceProfile> scanDeviceProfiles();
    std::vector<AdvancedDeviceProfile> scanAdvancedDeviceProfiles();
    
    // JSON operations
    std::string getCurrentDevicesAsJson(bool pretty = true);
    std::string getCurrentProfilesAsJson(bool pretty = true);
    std::string getCurrentAdvancedProfilesAsJson(bool pretty = true);
    std::string createScanReportJson(const std::string& scan_id = "");
    std::string generateDeviceWithSimStatusJson(const QMIDevice& device, bool pretty = true);
    std::string generateDevicesArrayWithSimStatusJson(const std::vector<QMIDevice>& devices, bool pretty = true);
    std::string validateAndExtractSIMJson(const std::string& json_string);
    bool saveCurrentStateToFile(const std::string& filename);
    bool loadStateFromFile(const std::string& filename);
    
    // Get JSON agent for direct access
    QMIDevScanAgent* getJsonAgent();
        
    // Registry access methods
    std::string getRegistryJson(bool pretty = true);
    int getRegistryDeviceCount();
    bool hasRegistryDevice(const std::string& device_path);

private:
    // Internal methods
    std::vector<std::string> findQMIDevices();
    QMIDevice queryDeviceInfo(const std::string& device_path);
    DeviceProfile queryDeviceProfile(const std::string& device_path);
    AdvancedDeviceProfile queryAdvancedDeviceProfile(const std::string& device_path);
    std::string executeCommand(const std::string& command);
    std::string executeCommandSafe(const std::string& command);
    void parseDeviceInfo(const std::string& output, QMIDevice& device);
    void parseDeviceProfile(const std::string& device_path, DeviceProfile& profile);
    void parseAdvancedDeviceProfile(const std::string& device_path, AdvancedDeviceProfile& profile);
    void reportDevice(const QMIDevice& device, bool added);
    void reportProfile(const DeviceProfile& profile, bool added);
    void reportAdvancedProfile(const AdvancedDeviceProfile& profile, bool added);
    void monitorLoop();
    void setupUdev();
    void cleanupUdev();
    
    // Production parsing methods
    std::string parseManufacturer(const std::string& output);
    std::string parseMSISDN(const std::string& output);
    std::string parsePowerState(const std::string& output);
    std::string parseHardwareRevision(const std::string& output);
    std::string parseOperatingMode(const std::string& output);
    std::string parseICCID(const std::string& output);
    std::string parseIMSI(const std::string& output);
    std::string parseUIMState(const std::string& output);
    std::string parsePINStatus(const std::string& output);
    std::string parseTime(const std::string& output);
    std::string parseBandCapabilities(const std::string& output);
    std::vector<std::string> parseStoredImages(const std::string& output);
    std::string parseSoftwareVersion(const std::string& output);
    std::string parseUSBComposition(const std::string& output);
    std::string parseFactorySKU(const std::string& output);
    std::string getMACAddress(const std::string& interface);
    
    // SIM status collection and parsing
    SIMStatus collectSIMStatus(const std::string& device_path);
    SIMStatus parseSIMCardStatus(const std::string& output);
    
    ProfileMode m_profile_mode;
    DeviceCallback m_callback;
    ProfileCallback m_profile_callback;
    AdvancedProfileCallback m_advanced_callback;
    std::vector<QMIDevice> m_current_devices;
    std::vector<DeviceProfile> m_current_profiles;
    std::vector<AdvancedDeviceProfile> m_current_advanced_profiles;
    std::unique_ptr<std::thread> m_monitor_thread;
    std::atomic<bool> m_monitoring;
    
    // udev related members
    struct udev* m_udev;
    struct udev_monitor* m_monitor;
    int m_monitor_fd;
    mutable std::mutex m_devices_mutex;
    
    // JSON processing agent
    std::unique_ptr<QMIDevScanAgent> m_json_agent;
};

#endif // QMI_SCANNER_H
