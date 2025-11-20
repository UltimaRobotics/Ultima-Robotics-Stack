
#ifndef QMI_DEVICE_REGISTRY_H
#define QMI_DEVICE_REGISTRY_H

#include "qmi_scanner.h"
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <json/json.h>
#include <iostream>

class QMIDeviceRegistry {
public:
    static QMIDeviceRegistry& getInstance();
    
    // Initialization and startup scan
    void initializeFromStartupScan(const std::vector<QMIDevice>& discovered_devices);
    bool isInitialized() const;
    void markAsInitialized();
    
    // Device management
    void addDevice(const QMIDevice& device);
    void removeDevice(const std::string& device_path);
    void updateDevice(const QMIDevice& device);
    void clear();
    
    // Query methods
    std::string getCurrent() const;
    std::string getCurrentPretty() const;
    int getDeviceCount() const;
    std::vector<QMIDevice> getDevices() const;
    QMIDevice* findDevice(const std::string& device_path);
    
    // Status methods
    bool hasDevice(const std::string& device_path) const;
    bool isEmpty() const;
    
    // Callback registration for change notifications
    using RegistryChangeCallback = std::function<void(const QMIDevice&, bool /* added */)>;
    void setChangeCallback(RegistryChangeCallback callback);
    
    // JSON format control
    void setIncludeTimestamp(bool include);
    void setIncludeMetadata(bool include);

    // Registry statistics
    struct RegistryStats {
        int total_devices_discovered;
        int devices_currently_active;
        int devices_added_since_startup;
        int devices_removed_since_startup;
        std::string last_scan_timestamp;
        bool is_initialized;
    };
    RegistryStats getRegistryStats() const;

private:
    QMIDeviceRegistry() = default;
    ~QMIDeviceRegistry() = default;
    QMIDeviceRegistry(const QMIDeviceRegistry&) = delete;
    QMIDeviceRegistry& operator=(const QMIDeviceRegistry&) = delete;
    
    // Internal helpers
    Json::Value devicesToJson() const;
    void notifyChange(const QMIDevice& device, bool added);
    std::string getCurrentTimestamp() const;
    
    mutable std::mutex m_mutex;
    std::vector<QMIDevice> m_devices;
    RegistryChangeCallback m_change_callback;
    bool m_include_timestamp = true;
    bool m_include_metadata = true;
    
    // Registry state tracking
    std::atomic<bool> m_initialized{false};
    int m_startup_device_count = 0;
    int m_devices_added_since_startup = 0;
    int m_devices_removed_since_startup = 0;
    std::string m_startup_timestamp;
};

// Global convenience functions
std::string GetCurrentQMIDevicesJson();
std::string GetCurrentQMIDevicesJsonPretty();
int GetQMIDeviceCount();
bool IsRegistryInitialized();

#endif // QMI_DEVICE_REGISTRY_H
