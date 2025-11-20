
#include "qmi_device_registry.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

QMIDeviceRegistry& QMIDeviceRegistry::getInstance() {
    static QMIDeviceRegistry instance;
    return instance;
}

void QMIDeviceRegistry::initializeFromStartupScan(const std::vector<QMIDevice>& discovered_devices) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clear any existing devices
    m_devices.clear();
    
    // Reset counters
    m_devices_added_since_startup = 0;
    m_devices_removed_since_startup = 0;
    
    // Add all discovered devices
    for (const auto& device : discovered_devices) {
        m_devices.push_back(device);
    }
    
    m_startup_device_count = static_cast<int>(m_devices.size());
    m_startup_timestamp = getCurrentTimestamp();
    
    // Mark as initialized
    m_initialized = true;
    
    std::cout << "QMI Device Registry initialized with " << m_startup_device_count 
              << " devices discovered during startup scan" << std::endl;
}

bool QMIDeviceRegistry::isInitialized() const {
    return m_initialized.load();
}

void QMIDeviceRegistry::markAsInitialized() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = true;
    m_startup_timestamp = getCurrentTimestamp();
}

void QMIDeviceRegistry::addDevice(const QMIDevice& device) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if device already exists
    auto it = std::find_if(m_devices.begin(), m_devices.end(),
        [&device](const QMIDevice& existing) {
            return existing.device_path == device.device_path;
        });
    
    if (it != m_devices.end()) {
        // Update existing device
        *it = device;
        notifyChange(device, false); // Update notification
        std::cout << "Registry: Updated existing device " << device.device_path << std::endl;
    } else {
        // Add new device
        m_devices.push_back(device);
        if (m_initialized.load()) {
            m_devices_added_since_startup++;
        }
        notifyChange(device, true); // Add notification
        std::cout << "Registry: Added new device " << device.device_path 
                  << " (Total devices: " << m_devices.size() << ")" << std::endl;
    }
}

void QMIDeviceRegistry::removeDevice(const std::string& device_path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_devices.begin(), m_devices.end(),
        [&device_path](const QMIDevice& device) {
            return device.device_path == device_path;
        });
    
    if (it != m_devices.end()) {
        QMIDevice removed_device = *it;
        removed_device.action = "removed";
        m_devices.erase(it);
        
        if (m_initialized.load()) {
            m_devices_removed_since_startup++;
        }
        
        notifyChange(removed_device, false); // Remove notification
        std::cout << "Registry: Removed device " << device_path 
                  << " (Total devices: " << m_devices.size() << ")" << std::endl;
    }
}

void QMIDeviceRegistry::updateDevice(const QMIDevice& device) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_devices.begin(), m_devices.end(),
        [&device](const QMIDevice& existing) {
            return existing.device_path == device.device_path;
        });
    
    if (it != m_devices.end()) {
        *it = device;
        notifyChange(device, false); // Update notification
        std::cout << "Registry: Updated device " << device.device_path << std::endl;
    }
}

void QMIDeviceRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_devices.clear();
    m_initialized = false;
    m_startup_device_count = 0;
    m_devices_added_since_startup = 0;
    m_devices_removed_since_startup = 0;
    m_startup_timestamp.clear();
    std::cout << "Registry: Cleared all devices and reset state" << std::endl;
}

std::string QMIDeviceRegistry::getCurrent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    Json::Value root = devicesToJson();
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    
    std::ostringstream oss;
    writer->write(root, &oss);
    return oss.str();
}

std::string QMIDeviceRegistry::getCurrentPretty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    Json::Value root = devicesToJson();
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    
    std::ostringstream oss;
    writer->write(root, &oss);
    return oss.str();
}

int QMIDeviceRegistry::getDeviceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_devices.size());
}

std::vector<QMIDevice> QMIDeviceRegistry::getDevices() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices;
}

QMIDevice* QMIDeviceRegistry::findDevice(const std::string& device_path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_devices.begin(), m_devices.end(),
        [&device_path](const QMIDevice& device) {
            return device.device_path == device_path;
        });
    
    return (it != m_devices.end()) ? &(*it) : nullptr;
}

bool QMIDeviceRegistry::hasDevice(const std::string& device_path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    return std::any_of(m_devices.begin(), m_devices.end(),
        [&device_path](const QMIDevice& device) {
            return device.device_path == device_path;
        });
}

bool QMIDeviceRegistry::isEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_devices.empty();
}

void QMIDeviceRegistry::setChangeCallback(RegistryChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_change_callback = callback;
}

void QMIDeviceRegistry::setIncludeTimestamp(bool include) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_include_timestamp = include;
}

void QMIDeviceRegistry::setIncludeMetadata(bool include) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_include_metadata = include;
}

QMIDeviceRegistry::RegistryStats QMIDeviceRegistry::getRegistryStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    RegistryStats stats;
    stats.total_devices_discovered = m_startup_device_count;
    stats.devices_currently_active = static_cast<int>(m_devices.size());
    stats.devices_added_since_startup = m_devices_added_since_startup;
    stats.devices_removed_since_startup = m_devices_removed_since_startup;
    stats.last_scan_timestamp = m_startup_timestamp;
    stats.is_initialized = m_initialized.load();
    
    return stats;
}

Json::Value QMIDeviceRegistry::devicesToJson() const {
    Json::Value root;
    
    // Add metadata if enabled
    if (m_include_metadata) {
        root["device_count"] = static_cast<int>(m_devices.size());
        root["registry_version"] = "1.1";
        root["registry_initialized"] = m_initialized.load();
        root["startup_device_count"] = m_startup_device_count;
        root["devices_added_since_startup"] = m_devices_added_since_startup;
        root["devices_removed_since_startup"] = m_devices_removed_since_startup;
        if (!m_startup_timestamp.empty()) {
            root["startup_timestamp"] = m_startup_timestamp;
        }
    }
    
    // Add timestamp if enabled
    if (m_include_timestamp) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        root["timestamp"] = static_cast<int64_t>(timestamp);
    }
    
    // Add devices array
    Json::Value devices_array(Json::arrayValue);
    for (const auto& device : m_devices) {
        Json::Value device_json;
        device_json["device_path"] = device.device_path;
        device_json["imei"] = device.imei;
        device_json["model"] = device.model;
        device_json["manufacturer"] = device.manufacturer;
        device_json["firmware_version"] = device.firmware_version;
        device_json["is_available"] = device.is_available;
        device_json["action"] = device.action;
        
        // Add supported bands
        Json::Value bands_array(Json::arrayValue);
        for (const auto& band : device.supported_bands) {
            bands_array.append(band);
        }
        device_json["supported_bands"] = bands_array;
        
        // Add SIM status
        Json::Value sim_status_json;
        sim_status_json["card_state"] = device.sim_status.card_state;
        sim_status_json["application_state"] = device.sim_status.application_state;
        sim_status_json["application_id"] = device.sim_status.application_id;
        sim_status_json["application_type"] = device.sim_status.application_type;
        sim_status_json["personalization_state"] = device.sim_status.personalization_state;
        sim_status_json["upin_replaces_pin1"] = device.sim_status.upin_replaces_pin1;
        sim_status_json["pin1_state"] = device.sim_status.pin1_state;
        sim_status_json["pin1_retries"] = device.sim_status.pin1_retries;
        sim_status_json["puk1_retries"] = device.sim_status.puk1_retries;
        sim_status_json["pin2_state"] = device.sim_status.pin2_state;
        sim_status_json["pin2_retries"] = device.sim_status.pin2_retries;
        sim_status_json["puk2_retries"] = device.sim_status.puk2_retries;
        sim_status_json["upin_state"] = device.sim_status.upin_state;
        sim_status_json["upin_retries"] = device.sim_status.upin_retries;
        sim_status_json["upuk_retries"] = device.sim_status.upuk_retries;
        sim_status_json["primary_gw_slot"] = device.sim_status.primary_gw_slot;
        sim_status_json["primary_gw_application"] = device.sim_status.primary_gw_application;
        sim_status_json["primary_1x_status"] = device.sim_status.primary_1x_status;
        sim_status_json["secondary_gw_status"] = device.sim_status.secondary_gw_status;
        sim_status_json["secondary_1x_status"] = device.sim_status.secondary_1x_status;
        
        device_json["sim-status"] = sim_status_json;
        devices_array.append(device_json);
    }
    
    root["devices"] = devices_array;
    return root;
}

void QMIDeviceRegistry::notifyChange(const QMIDevice& device, bool added) {
    if (m_change_callback) {
        // Call callback without holding the mutex to avoid deadlocks
        auto callback = m_change_callback;
        callback(device, added);
    }
}

std::string QMIDeviceRegistry::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Global convenience functions
std::string GetCurrentQMIDevicesJson() {
    return QMIDeviceRegistry::getInstance().getCurrent();
}

std::string GetCurrentQMIDevicesJsonPretty() {
    return QMIDeviceRegistry::getInstance().getCurrentPretty();
}

int GetQMIDeviceCount() {
    return QMIDeviceRegistry::getInstance().getDeviceCount();
}

bool IsRegistryInitialized() {
    return QMIDeviceRegistry::getInstance().isInitialized();
}
