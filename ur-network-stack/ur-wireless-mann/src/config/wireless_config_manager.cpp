#include "urwt/config/wireless_config_manager.hpp"
#include "urwt/managers/interface_detector.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace urwt {
namespace config {

std::string wirelessModeToString(WirelessMode mode) {
    switch (mode) {
        case WirelessMode::STA: return "sta";
        case WirelessMode::AP: return "ap";
        default: return "unknown";
    }
}

WirelessMode stringToWirelessMode(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "sta" || lower == "station") return WirelessMode::STA;
    if (lower == "ap" || lower == "access_point") return WirelessMode::AP;
    return WirelessMode::Unknown;
}

std::string securityTypeToString(SecurityType type) {
    switch (type) {
        case SecurityType::Open: return "Open";
        case SecurityType::WEP: return "WEP";
        case SecurityType::WPA: return "WPA";
        case SecurityType::WPA2: return "WPA2";
        case SecurityType::WPA3: return "WPA3";
        case SecurityType::WPA2Enterprise: return "WPA2-Enterprise";
        default: return "Unknown";
    }
}

SecurityType stringToSecurityType(const std::string& str) {
    if (str == "Open" || str == "open") return SecurityType::Open;
    if (str == "WEP" || str == "wep") return SecurityType::WEP;
    if (str == "WPA" || str == "wpa") return SecurityType::WPA;
    if (str == "WPA2" || str == "wpa2") return SecurityType::WPA2;
    if (str == "WPA3" || str == "wpa3") return SecurityType::WPA3;
    if (str == "WPA2-Enterprise" || str == "wpa2-enterprise") return SecurityType::WPA2Enterprise;
    return SecurityType::Unknown;
}

void to_json(nlohmann::json& j, const NetworkProfile& profile) {
    j = nlohmann::json::object();
    j["ssid"] = profile.ssid;
    j["security"] = securityTypeToString(profile.security);
    j["password"] = profile.password;
    j["priority"] = profile.priority;
    j["auto_connect"] = profile.auto_connect;
    j["hidden"] = profile.hidden;
    
    if (profile.bssid && !profile.bssid->empty()) {
        j["bssid"] = *profile.bssid;
    }
    if (profile.identity && !profile.identity->empty()) {
        j["identity"] = *profile.identity;
    }
    if (profile.ca_cert && !profile.ca_cert->empty()) {
        j["ca_cert"] = *profile.ca_cert;
    }
    if (profile.client_cert && !profile.client_cert->empty()) {
        j["client_cert"] = *profile.client_cert;
    }
    if (profile.private_key && !profile.private_key->empty()) {
        j["private_key"] = *profile.private_key;
    }
}

void from_json(const nlohmann::json& j, NetworkProfile& profile) {
    profile.ssid = j.at("ssid").get<std::string>();
    profile.security = stringToSecurityType(j.value("security", "WPA2"));
    profile.password = j.value("password", "");
    profile.priority = j.value("priority", 5);
    profile.auto_connect = j.value("auto_connect", true);
    profile.hidden = j.value("hidden", false);
    if (j.contains("bssid") && !j["bssid"].is_null()) 
        profile.bssid = j["bssid"].get<std::string>();
    if (j.contains("identity") && !j["identity"].is_null()) 
        profile.identity = j["identity"].get<std::string>();
    if (j.contains("ca_cert") && !j["ca_cert"].is_null()) 
        profile.ca_cert = j["ca_cert"].get<std::string>();
    if (j.contains("client_cert") && !j["client_cert"].is_null()) 
        profile.client_cert = j["client_cert"].get<std::string>();
    if (j.contains("private_key") && !j["private_key"].is_null()) 
        profile.private_key = j["private_key"].get<std::string>();
}

void to_json(nlohmann::json& j, const StaticIPConfig& config) {
    j = nlohmann::json::object();
    if (config.ip_address) {
        j["ip_address"] = *config.ip_address;
    } else {
        j["ip_address"] = nullptr;
    }
    if (config.netmask) {
        j["netmask"] = *config.netmask;
    } else {
        j["netmask"] = nullptr;
    }
    if (config.gateway) {
        j["gateway"] = *config.gateway;
    } else {
        j["gateway"] = nullptr;
    }
    j["dns_servers"] = config.dns_servers;
}

void from_json(const nlohmann::json& j, StaticIPConfig& config) {
    if (j.contains("ip_address") && !j["ip_address"].is_null())
        config.ip_address = j["ip_address"].get<std::string>();
    if (j.contains("netmask") && !j["netmask"].is_null())
        config.netmask = j["netmask"].get<std::string>();
    if (j.contains("gateway") && !j["gateway"].is_null())
        config.gateway = j["gateway"].get<std::string>();
    if (j.contains("dns_servers"))
        config.dns_servers = j["dns_servers"].get<std::vector<std::string>>();
}

void to_json(nlohmann::json& j, const STAModeConfig& config) {
    j = nlohmann::json{
        {"saved_networks", config.saved_networks},
        {"interface", config.interface},
        {"dhcp_enabled", config.dhcp_enabled},
        {"static_ip_config", config.static_ip_config},
        {"power_save", config.power_save},
        {"regulatory_domain", config.regulatory_domain}
    };
}

void from_json(const nlohmann::json& j, STAModeConfig& config) {
    if (j.contains("saved_networks"))
        config.saved_networks = j["saved_networks"].get<std::vector<NetworkProfile>>();
    config.interface = j.value("interface", "");
    config.dhcp_enabled = j.value("dhcp_enabled", true);
    if (j.contains("static_ip_config"))
        config.static_ip_config = j["static_ip_config"].get<StaticIPConfig>();
    config.power_save = j.value("power_save", false);
    config.regulatory_domain = j.value("regulatory_domain", "US");
}

void to_json(nlohmann::json& j, const APModeConfig& config) {
    j = nlohmann::json{
        {"ssid", config.ssid},
        {"security", securityTypeToString(config.security)},
        {"password", config.password},
        {"channel", config.channel},
        {"interface", config.interface},
        {"ip_address", config.ip_address},
        {"netmask", config.netmask},
        {"dhcp_range_start", config.dhcp_range_start},
        {"dhcp_range_end", config.dhcp_range_end},
        {"dhcp_lease_time", config.dhcp_lease_time},
        {"max_clients", config.max_clients},
        {"hidden", config.hidden},
        {"country_code", config.country_code},
        {"hw_mode", config.hw_mode},
        {"ieee80211n", config.ieee80211n},
        {"wmm_enabled", config.wmm_enabled}
    };
}

void from_json(const nlohmann::json& j, APModeConfig& config) {
    config.ssid = j.value("ssid", "UR-Wireless-AP");
    config.security = stringToSecurityType(j.value("security", "WPA2"));
    config.password = j.value("password", "");
    config.channel = j.value("channel", 6);
    config.interface = j.value("interface", "");
    config.ip_address = j.value("ip_address", "192.168.4.1");
    config.netmask = j.value("netmask", "255.255.255.0");
    config.dhcp_range_start = j.value("dhcp_range_start", "192.168.4.100");
    config.dhcp_range_end = j.value("dhcp_range_end", "192.168.4.200");
    config.dhcp_lease_time = j.value("dhcp_lease_time", "12h");
    config.max_clients = j.value("max_clients", 10);
    config.hidden = j.value("hidden", false);
    config.country_code = j.value("country_code", "US");
    config.hw_mode = j.value("hw_mode", "g");
    config.ieee80211n = j.value("ieee80211n", true);
    config.wmm_enabled = j.value("wmm_enabled", true);
}

void to_json(nlohmann::json& j, const AutomationConfig& config) {
    j = nlohmann::json{
        {"enabled", config.enabled},
        {"auto_connect", config.auto_connect},
        {"auto_reconnect", config.auto_reconnect},
        {"reconnect_delay_seconds", config.reconnect_delay_seconds},
        {"connection_timeout_seconds", config.connection_timeout_seconds},
        {"scan_interval_seconds", config.scan_interval_seconds},
        {"max_connection_attempts", config.max_connection_attempts}
    };
}

void from_json(const nlohmann::json& j, AutomationConfig& config) {
    config.enabled = j.value("enabled", true);
    config.auto_connect = j.value("auto_connect", true);
    config.auto_reconnect = j.value("auto_reconnect", true);
    config.reconnect_delay_seconds = j.value("reconnect_delay_seconds", 10);
    config.connection_timeout_seconds = j.value("connection_timeout_seconds", 30);
    config.scan_interval_seconds = j.value("scan_interval_seconds", 30);
    config.max_connection_attempts = j.value("max_connection_attempts", 3);
}

void to_json(nlohmann::json& j, const MonitoringConfig& config) {
    j = nlohmann::json{
        {"enabled", config.enabled},
        {"scan_interval_seconds", config.scan_interval_seconds},
        {"scan_timeout_seconds", config.scan_timeout_seconds},
        {"cache_duration_seconds", config.cache_duration_seconds},
        {"publish_events", config.publish_events},
        {"event_topic", config.event_topic}
    };
}

void from_json(const nlohmann::json& j, MonitoringConfig& config) {
    config.enabled = j.value("enabled", true);
    config.scan_interval_seconds = j.value("scan_interval_seconds", 30);
    config.scan_timeout_seconds = j.value("scan_timeout_seconds", 10);
    config.cache_duration_seconds = j.value("cache_duration_seconds", 120);
    config.publish_events = j.value("publish_events", true);
    config.event_topic = j.value("event_topic", "wireless/ur-wireless-mann/events");
}

void to_json(nlohmann::json& j, const PersistenceConfig& config) {
    j = nlohmann::json{
        {"config_file", config.config_file},
        {"saved_networks_file", config.saved_networks_file},
        {"state_file", config.state_file},
        {"backup_enabled", config.backup_enabled},
        {"backup_count", config.backup_count}
    };
}

void from_json(const nlohmann::json& j, PersistenceConfig& config) {
    config.config_file = j.value("config_file", "/etc/ur-wireless-mann/wireless-config.json");
    config.saved_networks_file = j.value("saved_networks_file", "/etc/ur-wireless-mann/saved-networks.json");
    config.state_file = j.value("state_file", "/var/lib/ur-wireless-mann/state.json");
    config.backup_enabled = j.value("backup_enabled", true);
    config.backup_count = j.value("backup_count", 5);
}

void to_json(nlohmann::json& j, const SecurityConfig& config) {
    j = nlohmann::json{
        {"encryption_enabled", config.encryption_enabled},
        {"encryption_key_file", config.encryption_key_file},
        {"require_encryption_for_passwords", config.require_encryption_for_passwords}
    };
}

void from_json(const nlohmann::json& j, SecurityConfig& config) {
    config.encryption_enabled = j.value("encryption_enabled", true);
    config.encryption_key_file = j.value("encryption_key_file", "/etc/ur-wireless-mann/encryption.key");
    config.require_encryption_for_passwords = j.value("require_encryption_for_passwords", true);
}

void to_json(nlohmann::json& j, const WirelessConfig& config) {
    j = nlohmann::json{
        {"wireless", {
            {"enabled", config.enabled},
            {"mode", wirelessModeToString(config.mode)},
            {"automation", config.automation},
            {"sta_mode", config.sta_mode},
            {"ap_mode", config.ap_mode},
            {"monitoring", config.monitoring}
        }},
        {"persistence", config.persistence},
        {"security", config.security}
    };
}

void from_json(const nlohmann::json& j, WirelessConfig& config) {
    if (j.contains("wireless")) {
        const auto& wireless = j["wireless"];
        config.enabled = wireless.value("enabled", true);
        config.mode = stringToWirelessMode(wireless.value("mode", "sta"));
        if (wireless.contains("automation"))
            config.automation = wireless["automation"].get<AutomationConfig>();
        if (wireless.contains("sta_mode"))
            config.sta_mode = wireless["sta_mode"].get<STAModeConfig>();
        if (wireless.contains("ap_mode"))
            config.ap_mode = wireless["ap_mode"].get<APModeConfig>();
        if (wireless.contains("monitoring"))
            config.monitoring = wireless["monitoring"].get<MonitoringConfig>();
    }
    if (j.contains("persistence"))
        config.persistence = j["persistence"].get<PersistenceConfig>();
    if (j.contains("security"))
        config.security = j["security"].get<SecurityConfig>();
}

WirelessConfigManager::WirelessConfigManager() 
    : persister_(std::make_shared<ConfigurationPersister>()) {}

WirelessConfigManager::~WirelessConfigManager() {}

Result<bool, std::string> WirelessConfigManager::loadFromFile(const std::string& configPath) {
    auto fileResult = readFile(configPath);
    if (fileResult.isError()) {
        return Result<bool, std::string>::error("Failed to read config file: " + fileResult.error());
    }

    // Set config file path BEFORE loading so verifyAndCorrectInterfaces can save
    config_file_path_ = configPath;

    try {
        json j = json::parse(fileResult.value().content);
        auto loadResult = loadFromJSON(j);
        return loadResult;
    } catch (const json::exception& e) {
        return Result<bool, std::string>::error("JSON parse error: " + std::string(e.what()));
    }
}

Result<bool, std::string> WirelessConfigManager::loadFromJSON(const json& configJson) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            config_ = configJson.get<WirelessConfig>();
        } catch (const json::exception& e) {
            return Result<bool, std::string>::error("Failed to parse configuration: " + std::string(e.what()));
        }
    }
    
    // Verify and correct interface names (outside mutex to allow saving)
    auto verifyResult = verifyAndCorrectInterfaces();
    if (verifyResult.isError()) {
        std::cerr << "Warning: Interface verification issue: " << verifyResult.error() << std::endl;
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::saveToFile(const std::string& configPath) {
    std::cout << "[DEBUG] saveToFile: Called with path: '" << configPath << "'" << std::endl;
    std::cout << "[DEBUG] saveToFile: Path length: " << configPath.length() << std::endl;
    
    if (configPath.empty()) {
        std::cerr << "[DEBUG] saveToFile: Empty config path provided" << std::endl;
        return Result<bool, std::string>::error("Empty configuration file path");
    }
    
    try {
        json j = serializeConfig();
        std::cout << "[DEBUG] saveToFile: Config serialized" << std::endl;
        
        std::string jsonStr = j.dump(2);
        std::cout << "[DEBUG] saveToFile: JSON dumped, length: " << jsonStr.length() << std::endl;
        
        if (jsonStr.empty()) {
            std::cerr << "[DEBUG] saveToFile: JSON serialization produced empty string" << std::endl;
            return Result<bool, std::string>::error("JSON serialization produced empty string");
        }
        
        std::cout << "[DEBUG] saveToFile: Calling writeFile" << std::endl;
        auto result = writeFile(configPath, jsonStr);
        
        if (result.isError()) {
            std::cerr << "[DEBUG] saveToFile: writeFile failed: " << result.error() << std::endl;
        } else {
            std::cout << "[DEBUG] saveToFile: writeFile succeeded" << std::endl;
        }
        
        return result;
    } catch (const std::exception& e) {
        std::string errorMsg = std::string("Exception in saveToFile: ") + e.what();
        std::cerr << "[DEBUG] " << errorMsg << std::endl;
        return Result<bool, std::string>::error(errorMsg);
    }
}

WirelessConfig WirelessConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool WirelessConfigManager::isWirelessEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.enabled;
}

WirelessMode WirelessConfigManager::getMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.mode;
}

AutomationConfig WirelessConfigManager::getAutomationConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.automation;
}

STAModeConfig WirelessConfigManager::getSTAConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.sta_mode;
}

APModeConfig WirelessConfigManager::getAPConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.ap_mode;
}

MonitoringConfig WirelessConfigManager::getMonitoringConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.monitoring;
}

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

Result<bool, std::string> WirelessConfigManager::updateSTAConfig(const STAModeConfig& staConfig) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.sta_mode = staConfig;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::updateAPConfig(const APModeConfig& apConfig) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.ap_mode = apConfig;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::addSavedNetwork(const NetworkProfile& profile) {
    auto validateResult = validateNetworkProfile(profile);
    if (validateResult.isError()) {
        return validateResult;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = std::find_if(config_.sta_mode.saved_networks.begin(), 
                               config_.sta_mode.saved_networks.end(),
                               [&profile](const NetworkProfile& p) { return p.ssid == profile.ssid; });
        
        if (it != config_.sta_mode.saved_networks.end()) {
            return Result<bool, std::string>::error("Network with SSID '" + profile.ssid + "' already exists");
        }
        
        config_.sta_mode.saved_networks.push_back(profile);
        std::sort(config_.sta_mode.saved_networks.begin(), config_.sta_mode.saved_networks.end());
    }
    
    if (auto_persist_) {
        return persist();
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::removeSavedNetwork(const std::string& ssid) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = std::find_if(config_.sta_mode.saved_networks.begin(), 
                               config_.sta_mode.saved_networks.end(),
                               [&ssid](const NetworkProfile& p) { return p.ssid == ssid; });
        
        if (it == config_.sta_mode.saved_networks.end()) {
            return Result<bool, std::string>::error("Network with SSID '" + ssid + "' not found");
        }
        
        config_.sta_mode.saved_networks.erase(it);
    }
    
    if (auto_persist_) {
        return persist();
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::updateSavedNetwork(const NetworkProfile& profile) {
    auto validateResult = validateNetworkProfile(profile);
    if (validateResult.isError()) {
        return validateResult;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(config_.sta_mode.saved_networks.begin(), 
                           config_.sta_mode.saved_networks.end(),
                           [&profile](const NetworkProfile& p) { return p.ssid == profile.ssid; });
    
    if (it == config_.sta_mode.saved_networks.end()) {
        return Result<bool, std::string>::error("Network with SSID '" + profile.ssid + "' not found");
    }
    
    *it = profile;
    std::sort(config_.sta_mode.saved_networks.begin(), config_.sta_mode.saved_networks.end());
    
    return Result<bool, std::string>::ok(true);
}

std::vector<NetworkProfile> WirelessConfigManager::getSavedNetworks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.sta_mode.saved_networks;
}

std::optional<NetworkProfile> WirelessConfigManager::getSavedNetwork(const std::string& ssid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(config_.sta_mode.saved_networks.begin(), 
                           config_.sta_mode.saved_networks.end(),
                           [&ssid](const NetworkProfile& p) { return p.ssid == ssid; });
    
    if (it == config_.sta_mode.saved_networks.end()) {
        return std::nullopt;
    }
    
    return *it;
}

Result<bool, std::string> WirelessConfigManager::verifyAndCorrectInterfaces() {
    std::cout << "[DEBUG] verifyAndCorrectInterfaces: Starting interface verification" << std::endl;
    
    // Detect available interfaces
    InterfaceDetector detector;
    auto interfacesResult = detector.detectInterfaces();
    
    if (interfacesResult.isError()) {
        std::cerr << "[DEBUG] verifyAndCorrectInterfaces: Interface detection failed: " << interfacesResult.error() << std::endl;
        return Result<bool, std::string>::error("Failed to detect interfaces: " + interfacesResult.error());
    }
    
    auto interfaces = interfacesResult.value();
    std::cout << "[DEBUG] verifyAndCorrectInterfaces: Detected " << interfaces.size() << " interfaces" << std::endl;
    
    if (interfaces.empty()) {
        std::cerr << "[DEBUG] verifyAndCorrectInterfaces: No interfaces found" << std::endl;
        return Result<bool, std::string>::error("No wireless interfaces found on system");
    }
    
    bool configModified = false;
    std::string primaryInterface = interfaces[0].name();
    std::string configFilePath;
    
    std::cout << "[DEBUG] verifyAndCorrectInterfaces: Primary interface: '" << primaryInterface << "'" << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        configFilePath = config_file_path_;
        
        std::cout << "[DEBUG] verifyAndCorrectInterfaces: Config file path: '" << configFilePath << "'" << std::endl;
        std::cout << "[DEBUG] verifyAndCorrectInterfaces: Current STA interface: '" << config_.sta_mode.interface << "'" << std::endl;
        std::cout << "[DEBUG] verifyAndCorrectInterfaces: Current AP interface: '" << config_.ap_mode.interface << "'" << std::endl;
        
        // Check if only one interface exists
        if (interfaces.size() == 1) {
            // Auto-correct both STA and AP interface names (including empty)
            if (config_.sta_mode.interface.empty() || config_.sta_mode.interface != primaryInterface) {
                if (!config_.sta_mode.interface.empty()) {
                    std::cout << "Auto-correcting STA interface from '" << config_.sta_mode.interface 
                              << "' to '" << primaryInterface << "' (single interface detected)" << std::endl;
                } else {
                    std::cout << "Setting STA interface to '" << primaryInterface << "' (was empty)" << std::endl;
                }
                config_.sta_mode.interface = primaryInterface;
                configModified = true;
            }
            
            if (config_.ap_mode.interface.empty() || config_.ap_mode.interface != primaryInterface) {
                if (!config_.ap_mode.interface.empty()) {
                    std::cout << "Auto-correcting AP interface from '" << config_.ap_mode.interface 
                              << "' to '" << primaryInterface << "' (single interface detected)" << std::endl;
                } else {
                    std::cout << "Setting AP interface to '" << primaryInterface << "' (was empty)" << std::endl;
                }
                config_.ap_mode.interface = primaryInterface;
                configModified = true;
            }
        } else {
            // Multiple interfaces - verify configured ones exist
            bool staInterfaceExists = !config_.sta_mode.interface.empty();
            bool apInterfaceExists = !config_.ap_mode.interface.empty();
            
            for (const auto& iface : interfaces) {
                if (!config_.sta_mode.interface.empty() && iface.name() == config_.sta_mode.interface) {
                    staInterfaceExists = true;
                }
                if (!config_.ap_mode.interface.empty() && iface.name() == config_.ap_mode.interface) {
                    apInterfaceExists = true;
                }
            }
            
            // Replace with primary if configured interface doesn't exist or is empty
            if (!staInterfaceExists) {
                if (!config_.sta_mode.interface.empty()) {
                    std::cout << "STA interface '" << config_.sta_mode.interface 
                              << "' not found, replacing with '" << primaryInterface << "'" << std::endl;
                } else {
                    std::cout << "Setting STA interface to '" << primaryInterface << "' (was empty)" << std::endl;
                }
                config_.sta_mode.interface = primaryInterface;
                configModified = true;
            }
            
            if (!apInterfaceExists) {
                if (!config_.ap_mode.interface.empty()) {
                    std::cout << "AP interface '" << config_.ap_mode.interface 
                              << "' not found, replacing with '" << primaryInterface << "'" << std::endl;
                } else {
                    std::cout << "Setting AP interface to '" << primaryInterface << "' (was empty)" << std::endl;
                }
                config_.ap_mode.interface = primaryInterface;
                configModified = true;
            }
        }
    }
    
    std::cout << "[DEBUG] verifyAndCorrectInterfaces: Config modified: " << (configModified ? "true" : "false") << std::endl;
    
    // Persist changes if config was modified
    if (configModified) {
        if (!configFilePath.empty()) {
            std::cout << "[DEBUG] verifyAndCorrectInterfaces: Calling saveToFile with path: '" << configFilePath << "'" << std::endl;
            auto saveResult = saveToFile(configFilePath);
            if (saveResult.isError()) {
                std::cerr << "[DEBUG] verifyAndCorrectInterfaces: Save failed: " << saveResult.error() << std::endl;
                std::cerr << "Warning: Failed to save corrected configuration: " << saveResult.error() << std::endl;
            } else {
                std::cout << "[DEBUG] verifyAndCorrectInterfaces: Save succeeded" << std::endl;
                std::cout << "Interface corrections saved to configuration file" << std::endl;
            }
        } else {
            std::cout << "[DEBUG] verifyAndCorrectInterfaces: No config file path, skipping save" << std::endl;
            std::cout << "Interface corrections applied (not saved - no config file path set)" << std::endl;
        }
    }
    
    std::cout << "[DEBUG] verifyAndCorrectInterfaces: Completed successfully" << std::endl;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::persist() {
    if (config_file_path_.empty()) {
        config_file_path_ = config_.persistence.config_file;
    }
    
    WirelessConfig configCopy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        configCopy = config_;
    }
    
    return persister_->saveConfig(configCopy, config_file_path_);
}

Result<bool, std::string> WirelessConfigManager::reload() {
    if (config_file_path_.empty()) {
        return Result<bool, std::string>::error("No configuration file path set");
    }
    return loadFromFile(config_file_path_);
}

Result<bool, std::string> WirelessConfigManager::createBackup() {
    if (config_file_path_.empty()) {
        return Result<bool, std::string>::error("No configuration file path set");
    }

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << config_file_path_ << ".backup." << std::put_time(&tm, "%Y%m%d_%H%M%S");
    std::string backupPath = oss.str();
    
    return saveToFile(backupPath);
}

Result<bool, std::string> WirelessConfigManager::restoreFromBackup(int backupIndex) {
    if (config_file_path_.empty()) {
        return Result<bool, std::string>::error("No configuration file path set");
    }
    
    return persister_->restoreBackup(config_file_path_, backupIndex);
}

Result<bool, std::string> WirelessConfigManager::validate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_.mode == WirelessMode::Unknown) {
        return Result<bool, std::string>::error("Wireless mode cannot be Unknown");
    }
    
    if (config_.mode == WirelessMode::AP) {
        if (config_.ap_mode.ssid.empty()) {
            return Result<bool, std::string>::error("AP mode SSID cannot be empty");
        }
        if (config_.ap_mode.channel < 1 || config_.ap_mode.channel > 13) {
            return Result<bool, std::string>::error("AP mode channel must be between 1 and 13");
        }
    }
    
    for (const auto& network : config_.sta_mode.saved_networks) {
        if (network.ssid.empty()) {
            return Result<bool, std::string>::error("Saved network SSID cannot be empty");
        }
        if (network.priority < 0 || network.priority > 10) {
            return Result<bool, std::string>::error("Network priority must be between 0 and 10");
        }
    }
    
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::validateNetworkProfile(const NetworkProfile& profile) const {
    if (profile.ssid.empty()) {
        return Result<bool, std::string>::error("SSID cannot be empty");
    }
    
    if (profile.priority < 0 || profile.priority > 10) {
        return Result<bool, std::string>::error("Priority must be between 0 and 10");
    }
    
    if (profile.security == SecurityType::Unknown) {
        return Result<bool, std::string>::error("Security type cannot be Unknown");
    }
    
    return Result<bool, std::string>::ok(true);
}

json WirelessConfigManager::serializeConfig() const {
    std::cout << "[DEBUG] serializeConfig: Starting serialization" << std::endl;
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "[DEBUG] serializeConfig: Lock acquired" << std::endl;
    
    try {
        json j = config_;
        std::cout << "[DEBUG] serializeConfig: Config converted to JSON successfully" << std::endl;
        std::cout << "[DEBUG] serializeConfig: JSON is_null: " << j.is_null() << std::endl;
        std::cout << "[DEBUG] serializeConfig: JSON is_object: " << j.is_object() << std::endl;
        return j;
    } catch (const std::exception& e) {
        std::cerr << "[DEBUG] serializeConfig: Exception: " << e.what() << std::endl;
        throw;
    }
}

json WirelessConfigManager::serializeNetworkProfile(const NetworkProfile& profile) const {
    json j = profile;
    return j;
}

std::string WirelessConfigManager::encryptPassword(const std::string& password) const {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string result;
    int val = 0;
    int valb = -6;
    
    for (unsigned char c : password) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.size() % 4) {
        result.push_back('=');
    }
    
    return result;
}

std::string WirelessConfigManager::decryptPassword(const std::string& encrypted) const {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string result;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;
    
    int val = 0;
    int valb = -8;
    
    for (unsigned char c : encrypted) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return result;
}

Result<WirelessConfigManager::FileContent, std::string> WirelessConfigManager::readFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<WirelessConfigManager::FileContent, std::string>::error("Cannot open file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return Result<WirelessConfigManager::FileContent, std::string>::ok(FileContent{buffer.str()});
}

Result<bool, std::string> WirelessConfigManager::writeFile(const std::string& path, 
                                                           const std::string& content) const {
    std::cout << "[DEBUG] writeFile: Called with path: '" << path << "'" << std::endl;
    std::cout << "[DEBUG] writeFile: Path length: " << path.length() << std::endl;
    std::cout << "[DEBUG] writeFile: Content length: " << content.length() << std::endl;
    
    std::cout << "[DEBUG] writeFile: Opening file..." << std::endl;
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[DEBUG] writeFile: Failed to open file" << std::endl;
        return Result<bool, std::string>::error("Cannot open file for writing: " + path);
    }
    
    std::cout << "[DEBUG] writeFile: File opened, writing content..." << std::endl;
    file << content;
    file.close();
    
    if (file.fail()) {
        std::cerr << "[DEBUG] writeFile: Write operation failed" << std::endl;
        return Result<bool, std::string>::error("Failed to write to file: " + path);
    }
    
    std::cout << "[DEBUG] writeFile: Write completed successfully" << std::endl;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> WirelessConfigManager::setConfigFilePath(const std::string& path) {
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

}
}
