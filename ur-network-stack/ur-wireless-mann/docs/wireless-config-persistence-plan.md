
# Wireless Configuration Persistence Implementation Plan

## Executive Summary

This document outlines the comprehensive plan to integrate persistent wireless configuration storage into the `ur-wireless-mann` system. The implementation will ensure that all wireless configuration changes (mode switching, WiFi enable/disable, network management, automation settings) are persisted to a JSON configuration file and automatically restored on system startup.

---

## Table of Contents

1. [Overview](#overview)
2. [Configuration File Structure](#configuration-file-structure)
3. [Architecture Changes](#architecture-changes)
4. [Implementation Phases](#implementation-phases)
5. [File-by-File Modifications](#file-by-file-modifications)
6. [Configuration File Location](#configuration-file-location)
7. [Startup Initialization Flow](#startup-initialization-flow)
8. [Handler Modifications](#handler-modifications)
9. [State Synchronization Strategy](#state-synchronization-strategy)
10. [Error Handling and Recovery](#error-handling-and-recovery)
11. [Testing Strategy](#testing-strategy)
12. [Rollback and Migration](#rollback-and-migration)

---

## Overview

### Goals

1. **Persistence**: All wireless configuration changes must be written to `wireless-config.json`
2. **State Restoration**: On startup, the system must read the configuration file and apply the previous state
3. **Atomicity**: Configuration updates must be atomic to prevent corruption
4. **Backward Compatibility**: Handle missing or malformed configuration files gracefully
5. **Thread Safety**: Ensure configuration file access is thread-safe

### Non-Goals

- Real-time configuration watching (inotify-based)
- Multi-file configuration splitting
- Configuration versioning/migration (Phase 1)

---

## Configuration File Structure

### Complete Schema

```json
{
  "version": "1.0",
  "last_updated": "2024-01-15T10:30:00Z",
  "wireless": {
    "enabled": true,
    "mode": "sta",
    "automation": {
      "enabled": true,
      "auto_connect": true,
      "auto_reconnect": true,
      "reconnect_delay_seconds": 10,
      "connection_timeout_seconds": 30,
      "scan_interval_seconds": 30,
      "max_connection_attempts": 3
    },
    "sta_mode": {
      "saved_networks": [
        {
          "ssid": "HomeNetwork",
          "security": "WPA2",
          "password": "encrypted_base64_password",
          "priority": 10,
          "auto_connect": true,
          "hidden": false,
          "bssid": null,
          "identity": null,
          "ca_cert": null,
          "client_cert": null,
          "private_key": null
        },
        {
          "ssid": "WorkNetwork",
          "security": "WPA2-Enterprise",
          "password": "encrypted_base64_password",
          "priority": 8,
          "auto_connect": true,
          "hidden": false,
          "bssid": "AA:BB:CC:DD:EE:FF",
          "identity": "user@company.com",
          "ca_cert": "/etc/certs/ca.pem",
          "client_cert": "/etc/certs/client.crt",
          "private_key": "/etc/certs/client.key"
        }
      ],
      "interface": "wlan0",
      "dhcp_enabled": true,
      "static_ip_config": {
        "ip_address": null,
        "netmask": null,
        "gateway": null,
        "dns_servers": []
      },
      "power_save": false,
      "regulatory_domain": "US"
    },
    "ap_mode": {
      "ssid": "UR-Wireless-AP",
      "security": "WPA2",
      "password": "encrypted_base64_password",
      "channel": 6,
      "interface": "wlan0",
      "ip_address": "192.168.4.1",
      "netmask": "255.255.255.0",
      "dhcp_range_start": "192.168.4.100",
      "dhcp_range_end": "192.168.4.200",
      "dhcp_lease_time": "12h",
      "max_clients": 10,
      "hidden": false,
      "country_code": "US",
      "hw_mode": "g",
      "ieee80211n": true,
      "wmm_enabled": true
    },
    "monitoring": {
      "enabled": true,
      "scan_interval_seconds": 30,
      "scan_timeout_seconds": 10,
      "cache_duration_seconds": 120,
      "publish_events": true,
      "event_topic": "wireless/ur-wireless-mann/events"
    }
  },
  "persistence": {
    "config_file": "/etc/ur-wireless-mann/wireless-config.json",
    "saved_networks_file": "/etc/ur-wireless-mann/saved-networks.json",
    "state_file": "/var/lib/ur-wireless-mann/state.json",
    "backup_enabled": true,
    "backup_count": 5
  },
  "security": {
    "encryption_enabled": true,
    "encryption_key_file": "/etc/ur-wireless-mann/encryption.key",
    "require_encryption_for_passwords": true
  }
}
```

### Field Descriptions

- **version**: Configuration schema version for future migrations
- **last_updated**: ISO 8601 timestamp of last configuration change
- **wireless.enabled**: Master WiFi enable/disable flag
- **wireless.mode**: Current operating mode (`"sta"`, `"ap"`, `"monitor"`)
- **wireless.automation**: Automation behavior settings
- **wireless.sta_mode.saved_networks**: Array of saved network profiles
- **wireless.ap_mode**: Access Point configuration
- **wireless.monitoring**: Network monitoring settings
- **persistence**: File locations and backup settings
- **security**: Password encryption settings

---

## Architecture Changes

### New Components

#### 1. ConfigurationPersister
**Location**: `src/config/configuration_persister.cpp`
**Purpose**: Handles atomic file writes with backups

```cpp
class ConfigurationPersister {
public:
    Result<bool, std::string> saveConfig(const WirelessConfig& config, 
                                         const std::string& path);
    Result<WirelessConfig, std::string> loadConfig(const std::string& path);
    Result<bool, std::string> createBackup(const std::string& path);
    Result<bool, std::string> restoreBackup(const std::string& path, int backupIndex);
    
private:
    Result<bool, std::string> atomicWrite(const std::string& path, 
                                          const std::string& content);
    std::vector<std::string> listBackups(const std::string& path);
};
```

#### 2. StartupConfigurator
**Location**: `src/config/startup_configurator.cpp`
**Purpose**: Applies configuration state to system on startup

```cpp
class StartupConfigurator {
public:
    Result<bool, std::string> applyConfiguration(const WirelessConfig& config);
    
private:
    Result<bool, std::string> applyWiFiState(bool enabled);
    Result<bool, std::string> applyMode(WirelessMode mode);
    Result<bool, std::string> applySTAConfiguration(const STAModeConfig& config);
    Result<bool, std::string> applyAPConfiguration(const APModeConfig& config);
    Result<bool, std::string> configureNetworks(const std::vector<NetworkProfile>& networks);
};
```

#### 3. ConfigurationValidator
**Location**: `src/config/configuration_validator.cpp`
**Purpose**: Validates configuration before applying

```cpp
class ConfigurationValidator {
public:
    Result<bool, std::string> validate(const WirelessConfig& config);
    Result<bool, std::string> validateNetworkProfile(const NetworkProfile& profile);
    Result<bool, std::string> validateAPConfig(const APModeConfig& config);
    Result<bool, std::string> validateSTAConfig(const STAModeConfig& config);
};
```

### Modified Components

#### WirelessConfigManager
**Changes**: Add persistence layer integration

```cpp
class WirelessConfigManager {
    // Existing methods...
    
    // New methods
    Result<bool, std::string> persist();
    Result<bool, std::string> reload();
    Result<bool, std::string> setConfigFilePath(const std::string& path);
    std::string getConfigFilePath() const;
    
private:
    std::shared_ptr<ConfigurationPersister> persister_;
    std::string config_file_path_;
    bool auto_persist_{true}; // Auto-save on every change
};
```

#### RPCService
**Changes**: Add startup configuration application

```cpp
class RPCService {
    // Existing methods...
    
private:
    std::shared_ptr<StartupConfigurator> startup_configurator_;
    
    std::optional<std::string> loadAndApplyConfiguration();
    std::optional<std::string> initializeWithConfiguration();
};
```

---

## Implementation Phases

### Phase 1: Core Infrastructure (Week 1)

**Tasks:**
1. Implement `ConfigurationPersister` with atomic writes
2. Implement `ConfigurationValidator`
3. Add persistence methods to `WirelessConfigManager`
4. Update `WirelessConfig` serialization/deserialization
5. Add unit tests for persistence layer

**Deliverables:**
- Configuration can be saved to and loaded from JSON
- Atomic write operations with `.tmp` file swapping
- Basic validation logic

### Phase 2: Startup Integration (Week 2)

**Tasks:**
1. Implement `StartupConfigurator`
2. Integrate configuration loading into `RPCService::initialize()`
3. Add system state application logic
4. Handle missing/corrupted configuration files gracefully
5. Add startup logging for configuration actions

**Deliverables:**
- System reads configuration on startup
- Previous state is restored automatically
- Fallback to defaults if config missing

### Phase 3: Handler Integration (Week 2-3)

**Tasks:**
1. Update `SetModeHandler` to persist mode changes
2. Update `EnableWifiHandler` to persist enabled state
3. Update `DisableWifiHandler` to persist disabled state
4. Update `SaveNetworkHandler` to persist network additions
5. Update `RemoveNetworkHandler` to persist network removals
6. Update `SetAutomationHandler` to persist automation settings
7. Update `UpdateWirelessConfigHandler` to persist all changes

**Deliverables:**
- All configuration-modifying operations write to config file
- Configuration file updated atomically after each change

### Phase 4: Backup and Recovery (Week 3)

**Tasks:**
1. Implement automatic backup creation before writes
2. Add backup rotation (keep last N backups)
3. Implement restore from backup functionality
4. Add RPC action for listing/restoring backups

**Deliverables:**
- Automatic backups on every configuration change
- Ability to restore from previous configurations

### Phase 5: Testing and Documentation (Week 4)

**Tasks:**
1. Integration tests for full startup → modify → restart cycle
2. Stress tests for concurrent configuration modifications
3. Failure scenario tests (corrupted files, permission errors)
4. Update documentation with configuration management
5. Add example configuration files

**Deliverables:**
- Comprehensive test suite
- Updated user documentation
- Example configurations

---

## File-by-File Modifications

### 1. `src/config/wireless_config_manager.cpp`

**New Methods to Add:**

```cpp
Result<bool, std::string> WirelessConfigManager::setConfigFilePath(
    const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_file_path_ = path;
    return Result<bool, std::string>::ok(true);
}

std::string WirelessConfigManager::getConfigFilePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_file_path_;
}

Result<bool, std::string> WirelessConfigManager::enableAutoPersist(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto_persist_ = enable;
    return Result<bool, std::string>::ok(true);
}
```

**Methods to Modify:**

Each configuration-changing method needs to call `persist()` if `auto_persist_` is enabled:

```cpp
Result<bool, std::string> WirelessConfigManager::setWirelessEnabled(bool enabled) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.enabled = enabled;
    }
    
    if (auto_persist_) {
        return persist();
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::setMode(WirelessMode mode) {
    if (mode == WirelessMode::Unknown) {
        return Result<bool, std::string>::error("Cannot set mode to Unknown");
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = mode;
    }
    
    if (auto_persist_) {
        return persist();
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::addSavedNetwork(
    const NetworkProfile& profile) {
    auto validateResult = validateNetworkProfile(profile);
    if (validateResult.isError()) {
        return validateResult;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = std::find_if(config_.sta_mode.saved_networks.begin(), 
                               config_.sta_mode.saved_networks.end(),
                               [&profile](const NetworkProfile& p) { 
                                   return p.ssid == profile.ssid; 
                               });
        
        if (it != config_.sta_mode.saved_networks.end()) {
            return Result<bool, std::string>::error(
                "Network with SSID '" + profile.ssid + "' already exists");
        }
        
        config_.sta_mode.saved_networks.push_back(profile);
        std::sort(config_.sta_mode.saved_networks.begin(), 
                  config_.sta_mode.saved_networks.end());
    }
    
    if (auto_persist_) {
        return persist();
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::removeSavedNetwork(
    const std::string& ssid) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = std::find_if(config_.sta_mode.saved_networks.begin(), 
                               config_.sta_mode.saved_networks.end(),
                               [&ssid](const NetworkProfile& p) { 
                                   return p.ssid == ssid; 
                               });
        
        if (it == config_.sta_mode.saved_networks.end()) {
            return Result<bool, std::string>::error(
                "Network with SSID '" + ssid + "' not found");
        }
        
        config_.sta_mode.saved_networks.erase(it);
    }
    
    if (auto_persist_) {
        return persist();
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::setAutomationEnabled(bool enabled) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.automation.enabled = enabled;
    }
    
    if (auto_persist_) {
        return persist();
    }
    
    return Result<bool, std::string>::ok(true);
}
```

### 2. `src/config/configuration_persister.cpp` (NEW FILE)

**Full Implementation:**

```cpp
#include "urwt/config/configuration_persister.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace urwt {
namespace config {

namespace fs = std::filesystem;

ConfigurationPersister::ConfigurationPersister() = default;
ConfigurationPersister::~ConfigurationPersister() = default;

Result<bool, std::string> ConfigurationPersister::saveConfig(
    const WirelessConfig& config,
    const std::string& path) {
    
    // Create backup before modifying
    if (fs::exists(path)) {
        auto backupResult = createBackup(path);
        if (backupResult.isError()) {
            // Log warning but continue
            std::cerr << "Warning: Failed to create backup: " 
                      << backupResult.error() << std::endl;
        }
    }
    
    // Serialize config to JSON
    json j = config;
    
    // Add metadata
    j["version"] = "1.0";
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    j["last_updated"] = ss.str();
    
    std::string content = j.dump(2); // Pretty print with 2-space indent
    
    // Atomic write
    return atomicWrite(path, content);
}

Result<WirelessConfig, std::string> ConfigurationPersister::loadConfig(
    const std::string& path) {
    
    if (!fs::exists(path)) {
        return Result<WirelessConfig, std::string>::error(
            "Configuration file does not exist: " + path);
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<WirelessConfig, std::string>::error(
            "Cannot open configuration file: " + path);
    }
    
    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        return Result<WirelessConfig, std::string>::error(
            "JSON parse error: " + std::string(e.what()));
    }
    
    // Validate version
    if (!j.contains("version")) {
        return Result<WirelessConfig, std::string>::error(
            "Missing version field in configuration");
    }
    
    std::string version = j["version"].get<std::string>();
    if (version != "1.0") {
        return Result<WirelessConfig, std::string>::error(
            "Unsupported configuration version: " + version);
    }
    
    // Deserialize
    try {
        WirelessConfig config = j.get<WirelessConfig>();
        return Result<WirelessConfig, std::string>::ok(config);
    } catch (const json::exception& e) {
        return Result<WirelessConfig, std::string>::error(
            "Failed to deserialize configuration: " + std::string(e.what()));
    }
}

Result<bool, std::string> ConfigurationPersister::createBackup(
    const std::string& path) {
    
    if (!fs::exists(path)) {
        return Result<bool, std::string>::error(
            "Source file does not exist: " + path);
    }
    
    // Generate backup filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << path << ".backup." 
       << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
    std::string backup_path = ss.str();
    
    // Copy file
    try {
        fs::copy_file(path, backup_path, 
                      fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error& e) {
        return Result<bool, std::string>::error(
            "Failed to create backup: " + std::string(e.what()));
    }
    
    // Rotate backups (keep only last 5)
    rotateBackups(path, 5);
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ConfigurationPersister::restoreBackup(
    const std::string& path, 
    int backupIndex) {
    
    auto backups = listBackups(path);
    
    if (backupIndex < 0 || backupIndex >= static_cast<int>(backups.size())) {
        return Result<bool, std::string>::error(
            "Invalid backup index: " + std::to_string(backupIndex));
    }
    
    std::string backup_path = backups[backupIndex];
    
    try {
        fs::copy_file(backup_path, path, 
                      fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error& e) {
        return Result<bool, std::string>::error(
            "Failed to restore backup: " + std::string(e.what()));
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ConfigurationPersister::atomicWrite(
    const std::string& path,
    const std::string& content) {
    
    // Ensure parent directory exists
    fs::path file_path(path);
    fs::path parent_dir = file_path.parent_path();
    
    if (!parent_dir.empty() && !fs::exists(parent_dir)) {
        try {
            fs::create_directories(parent_dir);
        } catch (const fs::filesystem_error& e) {
            return Result<bool, std::string>::error(
                "Failed to create directory: " + std::string(e.what()));
        }
    }
    
    // Write to temporary file
    std::string tmp_path = path + ".tmp";
    std::ofstream tmp_file(tmp_path, std::ios::binary);
    
    if (!tmp_file.is_open()) {
        return Result<bool, std::string>::error(
            "Cannot open temporary file for writing: " + tmp_path);
    }
    
    tmp_file << content;
    tmp_file.close();
    
    if (tmp_file.fail()) {
        fs::remove(tmp_path);
        return Result<bool, std::string>::error(
            "Failed to write to temporary file: " + tmp_path);
    }
    
    // Atomic rename
    try {
        fs::rename(tmp_path, path);
    } catch (const fs::filesystem_error& e) {
        fs::remove(tmp_path);
        return Result<bool, std::string>::error(
            "Failed to rename temporary file: " + std::string(e.what()));
    }
    
    return Result<bool, std::string>::ok(true);
}

std::vector<std::string> ConfigurationPersister::listBackups(
    const std::string& path) {
    
    std::vector<std::string> backups;
    fs::path file_path(path);
    fs::path parent_dir = file_path.parent_path();
    std::string base_name = file_path.filename().string() + ".backup.";
    
    if (!fs::exists(parent_dir)) {
        return backups;
    }
    
    for (const auto& entry : fs::directory_iterator(parent_dir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.find(base_name) == 0) {
                backups.push_back(entry.path().string());
            }
        }
    }
    
    // Sort by modification time (newest first)
    std::sort(backups.begin(), backups.end(), 
              [](const std::string& a, const std::string& b) {
                  return fs::last_write_time(a) > fs::last_write_time(b);
              });
    
    return backups;
}

void ConfigurationPersister::rotateBackups(const std::string& path, 
                                           int max_backups) {
    auto backups = listBackups(path);
    
    // Remove oldest backups if exceeding max count
    if (static_cast<int>(backups.size()) > max_backups) {
        for (size_t i = max_backups; i < backups.size(); ++i) {
            fs::remove(backups[i]);
        }
    }
}

}
}
```

### 3. `src/config/startup_configurator.cpp` (NEW FILE)

**Full Implementation:**

```cpp
#include "urwt/config/startup_configurator.hpp"
#include "urwt/utils/process_executor.hpp"
#include <thread>
#include <chrono>

namespace urwt {
namespace config {

StartupConfigurator::StartupConfigurator(
    std::shared_ptr<WirelessToolsAPI> api,
    std::shared_ptr<mode::ModeController> modeController)
    : api_(api)
    , mode_controller_(modeController) {
}

StartupConfigurator::~StartupConfigurator() = default;

Result<bool, std::string> StartupConfigurator::applyConfiguration(
    const WirelessConfig& config) {
    
    std::cout << "Applying wireless configuration from file..." << std::endl;
    
    // Step 1: Apply WiFi enabled/disabled state
    auto wifiStateResult = applyWiFiState(config.enabled);
    if (wifiStateResult.isError()) {
        return Result<bool, std::string>::error(
            "Failed to apply WiFi state: " + wifiStateResult.error());
    }
    
    // If WiFi is disabled, we're done
    if (!config.enabled) {
        std::cout << "WiFi is disabled per configuration" << std::endl;
        return Result<bool, std::string>::ok(true);
    }
    
    // Step 2: Apply operating mode
    auto modeResult = applyMode(config.mode);
    if (modeResult.isError()) {
        return Result<bool, std::string>::error(
            "Failed to apply mode: " + modeResult.error());
    }
    
    // Step 3: Apply mode-specific configuration
    if (config.mode == WirelessMode::STA) {
        auto staResult = applySTAConfiguration(config.sta_mode);
        if (staResult.isError()) {
            std::cerr << "Warning: Failed to apply STA configuration: " 
                      << staResult.error() << std::endl;
        }
    } else if (config.mode == WirelessMode::AP) {
        auto apResult = applyAPConfiguration(config.ap_mode);
        if (apResult.isError()) {
            std::cerr << "Warning: Failed to apply AP configuration: " 
                      << apResult.error() << std::endl;
        }
    }
    
    std::cout << "Wireless configuration applied successfully" << std::endl;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::applyWiFiState(bool enabled) {
    // Get first interface (or configured interface)
    auto interfacesResult = api_->listInterfaces();
    if (interfacesResult.isError()) {
        return Result<bool, std::string>::error(interfacesResult.error());
    }
    
    if (interfacesResult.value().empty()) {
        return Result<bool, std::string>::error("No wireless interfaces found");
    }
    
    std::string interface = interfacesResult.value()[0].name();
    ProcessExecutor executor;
    
    if (enabled) {
        std::cout << "Enabling WiFi on " << interface << std::endl;
        
        // Unblock RF
        executor.executeShell("rfkill unblock wifi 2>&1", 
                              std::chrono::seconds(5));
        
        // Bring interface up
        auto result = executor.executeShell(
            "ip link set " + interface + " up 2>&1",
            std::chrono::seconds(5));
        
        if (result.isError() || result.value().exit_code != 0) {
            return Result<bool, std::string>::error(
                "Failed to enable WiFi: " + 
                (result.isOk() ? result.value().stderr_output : result.error()));
        }
    } else {
        std::cout << "Disabling WiFi on " << interface << std::endl;
        
        // Kill services
        executor.executeShell("killall wpa_supplicant 2>/dev/null", 
                              std::chrono::seconds(5));
        executor.executeShell("killall hostapd 2>/dev/null", 
                              std::chrono::seconds(5));
        
        // Bring interface down
        auto result = executor.executeShell(
            "ip link set " + interface + " down 2>&1",
            std::chrono::seconds(5));
        
        if (result.isError() || result.value().exit_code != 0) {
            return Result<bool, std::string>::error(
                "Failed to disable WiFi: " + 
                (result.isOk() ? result.value().stderr_output : result.error()));
        }
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::applyMode(WirelessMode mode) {
    if (mode == WirelessMode::Unknown) {
        return Result<bool, std::string>::error("Cannot apply Unknown mode");
    }
    
    std::cout << "Applying mode: " << wirelessModeToString(mode) << std::endl;
    
    // Use ModeController to switch mode
    // Note: This will be called with a dummy config, real config applied later
    WirelessConfig dummyConfig;
    dummyConfig.mode = mode;
    
    auto result = mode_controller_->switchMode(mode, dummyConfig);
    if (result.isError()) {
        return Result<bool, std::string>::error(result.error());
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::applySTAConfiguration(
    const STAModeConfig& config) {
    
    std::cout << "Applying STA configuration..." << std::endl;
    
    // Apply regulatory domain
    if (!config.regulatory_domain.empty()) {
        ProcessExecutor executor;
        executor.executeShell("iw reg set " + config.regulatory_domain + " 2>&1",
                              std::chrono::seconds(5));
    }
    
    // Apply power save setting
    ProcessExecutor executor;
    std::string powerSave = config.power_save ? "on" : "off";
    executor.executeShell("iw dev " + config.interface + " set power_save " + 
                          powerSave + " 2>&1",
                          std::chrono::seconds(5));
    
    // Configure networks (if any)
    if (!config.saved_networks.empty()) {
        auto networksResult = configureNetworks(config.saved_networks);
        if (networksResult.isError()) {
            std::cerr << "Warning: Failed to configure networks: " 
                      << networksResult.error() << std::endl;
        }
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::applyAPConfiguration(
    const APModeConfig& config) {
    
    std::cout << "Applying AP configuration..." << std::endl;
    
    // Note: Full AP configuration requires hostapd and dnsmasq setup
    // This is a placeholder for basic IP configuration
    
    ProcessExecutor executor;
    
    // Set IP address
    auto ipResult = executor.executeShell(
        "ip addr add " + config.ip_address + "/24 dev " + 
        config.interface + " 2>&1",
        std::chrono::seconds(5));
    
    if (ipResult.isError()) {
        return Result<bool, std::string>::error(
            "Failed to set AP IP address: " + ipResult.error());
    }
    
    // TODO: Generate and apply hostapd.conf
    // TODO: Generate and apply dnsmasq.conf
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::configureNetworks(
    const std::vector<NetworkProfile>& networks) {
    
    std::cout << "Configuring " << networks.size() << " saved networks..." 
              << std::endl;
    
    // Generate wpa_supplicant.conf from network profiles
    std::stringstream conf;
    conf << "ctrl_interface=/var/run/wpa_supplicant\n";
    conf << "update_config=1\n\n";
    
    for (const auto& network : networks) {
        conf << "network={\n";
        conf << "  ssid=\"" << network.ssid << "\"\n";
        
        if (network.security == SecurityType::Open) {
            conf << "  key_mgmt=NONE\n";
        } else if (network.security == SecurityType::WPA2) {
            conf << "  key_mgmt=WPA-PSK\n";
            conf << "  psk=\"" << network.password << "\"\n";
        } else if (network.security == SecurityType::WPA2Enterprise) {
            conf << "  key_mgmt=WPA-EAP\n";
            if (network.identity) {
                conf << "  identity=\"" << *network.identity << "\"\n";
            }
            if (network.ca_cert) {
                conf << "  ca_cert=\"" << *network.ca_cert << "\"\n";
            }
            if (network.client_cert) {
                conf << "  client_cert=\"" << *network.client_cert << "\"\n";
            }
            if (network.private_key) {
                conf << "  private_key=\"" << *network.private_key << "\"\n";
            }
        }
        
        conf << "  priority=" << network.priority << "\n";
        
        if (network.hidden) {
            conf << "  scan_ssid=1\n";
        }
        
        if (network.bssid) {
            conf << "  bssid=" << *network.bssid << "\n";
        }
        
        conf << "}\n\n";
    }
    
    // Write to /tmp/wpa_supplicant.conf
    std::ofstream confFile("/tmp/wpa_supplicant.conf");
    if (!confFile.is_open()) {
        return Result<bool, std::string>::error(
            "Failed to create wpa_supplicant.conf");
    }
    
    confFile << conf.str();
    confFile.close();
    
    // Start wpa_supplicant
    ProcessExecutor executor;
    auto result = executor.executeShell(
        "wpa_supplicant -B -i wlan0 -c /tmp/wpa_supplicant.conf 2>&1",
        std::chrono::seconds(10));
    
    if (result.isError()) {
        return Result<bool, std::string>::error(
            "Failed to start wpa_supplicant: " + result.error());
    }
    
    return Result<bool, std::string>::ok(true);
}

}
}
```

### 4. `src/rpc/rpc_service.cpp` - Modifications

**Add to `RPCService::initialize()`:**

```cpp
std::optional<std::string> RPCService::initialize(const std::string& configPath) {
    // ... existing initialization code ...
    
    // Initialize wireless configuration manager
    wireless_config_manager_ = std::make_shared<config::WirelessConfigManager>();
    
    // Set configuration file path
    std::string wirelessConfigPath = "/etc/ur-wireless-mann/wireless-config.json";
    wireless_config_manager_->setConfigFilePath(wirelessConfigPath);
    
    // Enable auto-persist for all configuration changes
    wireless_config_manager_->enableAutoPersist(true);
    
    // Load existing configuration (or create default)
    auto loadResult = loadAndApplyConfiguration();
    if (loadResult.has_value()) {
        std::cerr << "Warning: " << *loadResult << std::endl;
        std::cout << "Using default configuration" << std::endl;
    }
    
    // ... rest of initialization ...
    
    return std::nullopt;
}

std::optional<std::string> RPCService::loadAndApplyConfiguration() {
    std::string configPath = wireless_config_manager_->getConfigFilePath();
    
    // Try to load existing configuration
    auto loadResult = wireless_config_manager_->loadFromFile(configPath);
    
    if (loadResult.isError()) {
        // Configuration file doesn't exist or is invalid
        std::cout << "No existing configuration found, creating default..." 
                  << std::endl;
        
        // Create default configuration
        config::WirelessConfig defaultConfig;
        defaultConfig.enabled = true;
        defaultConfig.mode = config::WirelessMode::STA;
        defaultConfig.automation.enabled = true;
        
        // Save default configuration
        auto saveResult = wireless_config_manager_->saveToFile(configPath);
        if (saveResult.isError()) {
            return "Failed to save default configuration: " + saveResult.error();
        }
        
        return std::nullopt; // Using defaults is OK
    }
    
    // Configuration loaded successfully, apply it to the system
    std::cout << "Loaded wireless configuration from " << configPath << std::endl;
    
    // Create startup configurator
    startup_configurator_ = std::make_shared<config::StartupConfigurator>(
        wireless_api_, mode_controller_);
    
    // Apply configuration to system
    auto config = wireless_config_manager_->getConfig();
    auto applyResult = startup_configurator_->applyConfiguration(config);
    
    if (applyResult.isError()) {
        return "Failed to apply configuration: " + applyResult.error();
    }
    
    std::cout << "Wireless configuration applied successfully" << std::endl;
    return std::nullopt;
}
```

### 5. Handler Modifications

Each handler that modifies configuration needs minimal changes since auto-persist is enabled in `WirelessConfigManager`.

**Example for `SetModeHandler`:**

The `SetModeHandler` already calls `config_manager_->setMode()` which will now automatically persist due to `auto_persist_` being enabled.

**No changes needed** - automatic persistence is handled by `WirelessConfigManager`.

---

## Configuration File Location

### Default Locations

**Primary Configuration:**
- Path: `/etc/ur-wireless-mann/wireless-config.json`
- Owner: `root:root`
- Permissions: `0600` (read/write owner only)

**Backups:**
- Path: `/etc/ur-wireless-mann/wireless-config.json.backup.YYYYMMDD_HHMMSS`
- Retention: Last 5 backups

**Alternative for Development:**
- Path: `./config/wireless-config.json` (relative to binary)
- Used when `/etc/ur-wireless-mann/` is not writable

### Directory Structure

```
/etc/ur-wireless-mann/
├── wireless-config.json
├── wireless-config.json.backup.20240115_103045
├── wireless-config.json.backup.20240115_093021
├── encryption.key
└── certs/
    ├── ca.pem
    ├── client.crt
    └── client.key

/var/lib/ur-wireless-mann/
└── state.json (runtime state, separate from config)
```

---

## Startup Initialization Flow

### Detailed Sequence Diagram

```
Application Start
    ↓
RPCService::initialize()
    ↓
Create WirelessConfigManager
    ↓
Set config file path
    ↓
Enable auto-persist
    ↓
loadAndApplyConfiguration()
    ↓
WirelessConfigManager::loadFromFile()
    ├─ [File exists] → Parse JSON → Validate → Return Config
    └─ [File missing] → Create default config → Save to file
    ↓
Create StartupConfigurator
    ↓
StartupConfigurator::applyConfiguration()
    ↓
Apply WiFi State (enabled/disabled)
    ↓
Apply Operating Mode (STA/AP)
    ↓
Apply Mode-Specific Config
    ├─ [STA] → Configure networks, regulatory domain, power save
    └─ [AP] → Configure IP, hostapd, dnsmasq
    ↓
Continue normal initialization
    ↓
Service ready
```

### Error Handling During Startup

1. **Configuration file missing**: Create default configuration, log warning, continue
2. **Configuration file corrupted**: Try backup restore, fallback to default, log error
3. **Configuration valid but application fails**: Log error, do not modify config, continue with defaults
4. **Permission errors**: Log error, use read-only mode, continue

---

## Handler Modifications

### Pattern for All Handlers

Since `auto_persist_` is enabled in `WirelessConfigManager`, handlers don't need explicit `persist()` calls. However, they should check for errors:

```cpp
RPCResponse SomeHandler::handle(const RPCRequest& request) {
    // ... validation ...
    
    // Modify configuration through WirelessConfigManager
    auto result = config_manager_->setSomething(value);
    
    if (!result.isOk()) {
        // Configuration change failed (including persistence)
        operation_tracker_->completeOperation(request.transaction_id, false);
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::OPERATION_FAILED,
            result.error()
        );
    }
    
    // Success - configuration was modified and persisted
    operation_tracker_->completeOperation(request.transaction_id, true);
    return createSuccessResponse(request.transaction_id, data, executionTime);
}
```

### Specific Handler Changes

**SetModeHandler**: Already calls `config_manager_->setMode()` - no changes needed

**EnableWifiHandler**: Add call to `config_manager_->setWirelessEnabled(true)` after successful operation

**DisableWifiHandler**: Add call to `config_manager_->setWirelessEnabled(false)` after successful operation

**SaveNetworkHandler**: Already uses `network_manager_->addNetwork()` which should use `config_manager_` internally

**RemoveNetworkHandler**: Already uses `network_manager_->removeNetwork()` which should use `config_manager_` internally

**SetAutomationHandler**: Already calls `config_manager_->setAutomationEnabled()` - no changes needed

---

## State Synchronization Strategy

### In-Memory vs. On-Disk State

**In-Memory State (WirelessConfig)**:
- Current configuration held in `WirelessConfigManager`
- Modified by handlers
- Source of truth for runtime behavior

**On-Disk State (wireless-config.json)**:
- Persistent storage
- Updated atomically on every configuration change
- Used for startup restoration

**Synchronization Flow**:

```
User Request → Handler → WirelessConfigManager (in-memory update)
                              ↓
                        [auto_persist enabled]
                              ↓
                    ConfigurationPersister::saveConfig()
                              ↓
                    Atomic write to disk
                              ↓
                        Success/Error → Handler
```

### Thread Safety

- `WirelessConfigManager` uses `std::mutex` for all operations
- File writes are atomic (write to `.tmp`, then rename)
- No concurrent file access issues

---

## Error Handling and Recovery

### Error Scenarios

#### 1. Configuration File Corrupted

**Detection**: JSON parse error during `loadFromFile()`

**Recovery**:
1. Attempt to load from backup (most recent)
2. If backup succeeds, use it and log warning
3. If all backups fail, create default configuration
4. Log error with details

**Code**:
```cpp
auto loadResult = wireless_config_manager_->loadFromFile(configPath);
if (loadResult.isError()) {
    std::cerr << "Failed to load config: " << loadResult.error() << std::endl;
    
    // Try backups
    for (int i = 0; i < 5; i++) {
        auto restoreResult = wireless_config_manager_->restoreFromBackup(i);
        if (restoreResult.isOk()) {
            auto reloadResult = wireless_config_manager_->reload();
            if (reloadResult.isOk()) {
                std::cout << "Recovered from backup " << i << std::endl;
                break;
            }
        }
    }
    
    // If still failed, use defaults
    // ... create default config ...
}
```

#### 2. Disk Full / Permission Denied

**Detection**: `atomicWrite()` fails

**Recovery**:
1. Log error immediately
2. Continue with in-memory configuration
3. Set `auto_persist_` to `false` to prevent repeated failures
4. Return error to user for configuration changes
5. Periodically retry persistence (e.g., every 60 seconds)

#### 3. Invalid Configuration Values

**Detection**: `ConfigurationValidator::validate()` fails

**Recovery**:
1. Reject the change
2. Return error to user
3. Keep previous valid configuration
4. Do not write to disk

---

## Testing Strategy

### Unit Tests

**ConfigurationPersister Tests** (`test_configuration_persister.cpp`):
- Test atomic write operations
- Test backup creation and rotation
- Test backup restoration
- Test error handling (permission denied, disk full)

**WirelessConfigManager Tests** (`test_wireless_config_manager.cpp`):
- Test auto-persist functionality
- Test configuration serialization/deserialization
- Test validation logic
- Test thread safety (concurrent modifications)

**StartupConfigurator Tests** (`test_startup_configurator.cpp`):
- Test configuration application
- Test mode switching on startup
- Test network configuration
- Test error handling

### Integration Tests

**Startup Restoration Test**:
```python
def test_startup_restoration():
    # 1. Start service
    # 2. Set mode to AP via RPC
    # 3. Save network via RPC
    # 4. Stop service
    # 5. Verify wireless-config.json contains changes
    # 6. Start service again
    # 7. Verify mode is still AP
    # 8. Verify saved network is present
```

**Configuration Persistence Test**:
```python
def test_configuration_persistence():
    # 1. Start service
    # 2. Perform 10 different configuration changes via RPC
    # 3. After each change, verify config file is updated
    # 4. Verify backup files are created
    # 5. Verify backup rotation (only 5 backups kept)
```

**Concurrent Modification Test**:
```python
def test_concurrent_modifications():
    # 1. Start service
    # 2. Send 50 concurrent RPC requests modifying different configs
    # 3. Verify all changes are persisted correctly
    # 4. Verify no corruption in config file
    # 5. Verify final state is consistent
```

### Failure Scenario Tests

**Corrupted Config Test**:
```python
def test_corrupted_config_recovery():
    # 1. Create valid config and backups
    # 2. Corrupt main config file (invalid JSON)
    # 3. Start service
    # 4. Verify service recovers from backup
    # 5. Verify service continues to operate
```

**Permission Error Test**:
```python
def test_permission_error_handling():
    # 1. Start service
    # 2. Make config file read-only
    # 3. Attempt configuration change via RPC
    # 4. Verify error is returned to user
    # 5. Verify in-memory state is still updated
    # 6. Make file writable again
    # 7. Verify next change persists successfully
```

---

## Rollback and Migration

### Configuration Version Migration

**Future Work**: When configuration schema changes (v1.0 → v2.0), implement migration logic:

```cpp
Result<WirelessConfig, std::string> ConfigurationPersister::migrate(
    const json& oldConfig,
    const std::string& fromVersion,
    const std::string& toVersion) {
    
    if (fromVersion == "1.0" && toVersion == "2.0") {
        // Apply migration transformations
        json newConfig = oldConfig;
        // ... add new fields, rename fields, etc. ...
        return Result<WirelessConfig, std::string>::ok(
            newConfig.get<WirelessConfig>());
    }
    
    return Result<WirelessConfig, std::string>::error(
        "Unsupported migration path");
}
```

### Manual Rollback Procedure

**For System Administrator**:

```bash
# List available backups
ls -lt /etc/ur-wireless-mann/wireless-config.json.backup.*

# Manually restore a specific backup
cp /etc/ur-wireless-mann/wireless-config.json.backup.20240115_103045 \
   /etc/ur-wireless-mann/wireless-config.json

# Restart service
systemctl restart ur-wireless-mann
```

**Via RPC** (future enhancement):

```json
{
  "jsonrpc": "2.0",
  "transaction_id": "restore-backup-1",
  "action": "restore_configuration_backup",
  "params": {
    "backup_index": 0
  }
}
```

---

## Implementation Checklist

### Phase 1: Core Infrastructure
- [ ] Create `ConfigurationPersister` class
- [ ] Implement atomic write with `.tmp` files
- [ ] Implement backup creation and rotation
- [ ] Add `persist()` method to `WirelessConfigManager`
- [ ] Add `auto_persist_` flag to `WirelessConfigManager`
- [ ] Update all setter methods to call `persist()` when `auto_persist_` enabled
- [ ] Add unit tests for `ConfigurationPersister`
- [ ] Add unit tests for `WirelessConfigManager` persistence

### Phase 2: Startup Integration
- [ ] Create `StartupConfigurator` class
- [ ] Implement `applyConfiguration()` method
- [ ] Implement `applyWiFiState()` method
- [ ] Implement `applyMode()` method
- [ ] Implement `applySTAConfiguration()` method
- [ ] Implement `applyAPConfiguration()` method
- [ ] Implement `configureNetworks()` method
- [ ] Integrate `loadAndApplyConfiguration()` into `RPCService::initialize()`
- [ ] Add logging for startup configuration application
- [ ] Add unit tests for `StartupConfigurator`

### Phase 3: Handler Integration
- [ ] Update `SetModeHandler` to verify auto-persist
- [ ] Update `EnableWifiHandler` to call `setWirelessEnabled(true)`
- [ ] Update `DisableWifiHandler` to call `setWirelessEnabled(false)`
- [ ] Update `SaveNetworkHandler` to use `config_manager_`
- [ ] Update `RemoveNetworkHandler` to use `config_manager_`
- [ ] Update `SetAutomationHandler` to verify auto-persist
- [ ] Update `UpdateWirelessConfigHandler` to verify auto-persist
- [ ] Add integration tests for each handler

### Phase 4: Backup and Recovery
- [ ] Verify backup creation on every config change
- [ ] Verify backup rotation (max 5 backups)
- [ ] Implement restore from backup functionality
- [ ] Add RPC action for listing backups
- [ ] Add RPC action for restoring from backup
- [ ] Add tests for backup and recovery

### Phase 5: Testing and Documentation
- [ ] Create integration test for full startup → modify → restart cycle
- [ ] Create stress test for concurrent modifications
- [ ] Create failure scenario tests (corrupted files, permissions)
- [ ] Update runtime workflow documentation
- [ ] Create user guide for configuration management
- [ ] Create example configuration files
- [ ] Add inline code documentation

### Phase 6: Deployment
- [ ] Create installation script for creating `/etc/ur-wireless-mann/`
- [ ] Set proper file permissions (0600 for config files)
- [ ] Create systemd service file with proper dependencies
- [ ] Add logrotate configuration for backup files
- [ ] Create migration guide from non-persistent version

---

## Timeline

**Week 1**: Phases 1-2 (Core infrastructure and startup integration)
**Week 2-3**: Phase 3 (Handler integration and testing)
**Week 3**: Phase 4 (Backup and recovery)
**Week 4**: Phase 5-6 (Final testing, documentation, deployment)

**Total Duration**: 4 weeks

---

## Success Criteria

1. **Persistence**: All configuration changes are written to file within 100ms
2. **Restoration**: System restarts with previous configuration state 100% of the time
3. **Reliability**: No configuration corruption even under concurrent modifications
4. **Recovery**: Automatic recovery from corrupted configuration using backups
5. **Performance**: Configuration file operations do not impact RPC response times
6. **Testing**: 95%+ code coverage for configuration management components

---

## Risks and Mitigation

### Risk 1: Configuration Corruption
**Probability**: Low
**Impact**: High
**Mitigation**: 
- Atomic writes with `.tmp` files
- Automatic backups before every write
- Configuration validation before applying
- Backup rotation to keep multiple restore points

### Risk 2: Disk Space Exhaustion
**Probability**: Low
**Impact**: Medium
**Mitigation**:
- Backup rotation (only keep 5 backups)
- Monitor disk space and log warnings
- Disable auto-persist if disk space critical

### Risk 3: Permission Issues
**Probability**: Medium
**Impact**: Medium
**Mitigation**:
- Graceful degradation (continue with in-memory config)
- Clear error messages to user
- Retry mechanism for transient failures

### Risk 4: Concurrent Access
**Probability**: Medium
**Impact**: Medium
**Mitigation**:
- Mutex protection in `WirelessConfigManager`
- Atomic file operations (rename)
- Thorough concurrent modification testing

---

## Conclusion

This implementation plan provides a comprehensive approach to adding persistent wireless configuration management to the `ur-wireless-mann` system. The design emphasizes:

- **Reliability**: Atomic operations, backups, validation
- **Simplicity**: Auto-persist flag eliminates manual save calls
- **Recoverability**: Multiple backup levels, graceful degradation
- **Maintainability**: Clean separation of concerns, well-tested components

By following this plan, the system will achieve full configuration persistence with minimal changes to existing handlers, while maintaining backward compatibility and providing robust error handling.
