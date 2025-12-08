#include "../include/NetworkExtensions.h"
#include "../include/NetworkConfigAPI.h"
#include "../include/Utils.h"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <map>

namespace OpenWrtNetwork {

// JSON Configuration Implementation
std::string JsonConfig::serialize(const NetworkStatus& status) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"connected\":" << (status.connected ? "true" : "false") << ",";
    oss << "\"connectionType\":\"" << escapeString(status.connectionType) << "\",";
    oss << "\"ipv4Address\":\"" << escapeString(status.ipv4Address) << "\",";
    oss << "\"ipv6Address\":\"" << escapeString(status.ipv6Address) << "\",";
    oss << "\"macAddress\":\"" << escapeString(status.macAddress) << "\",";
    oss << "\"linkSpeed\":\"" << escapeString(status.linkSpeed) << "\",";
    oss << "\"sessionDuration\":" << status.sessionDuration.count() << ",";
    oss << "\"gatewayAddress\":\"" << escapeString(status.gatewayAddress) << "\",";
    oss << "\"externalIP\":\"" << escapeString(status.externalIP) << "\",";
    oss << "\"downloadSpeed\":" << status.downloadSpeed << ",";
    oss << "\"uploadSpeed\":" << status.uploadSpeed << ",";
    oss << "\"dnsServers\":[";
    
    for (size_t i = 0; i < status.dnsServers.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeString(status.dnsServers[i]) << "\"";
    }
    
    oss << "]}";
    return oss.str();
}

std::string JsonConfig::serialize(const BasicSettings& settings) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"connectionMode\":\"" << escapeString(settings.connectionMode) << "\",";
    oss << "\"ipv4Address\":\"" << escapeString(settings.ipv4Address) << "\",";
    oss << "\"subnetMask\":\"" << escapeString(settings.subnetMask) << "\",";
    oss << "\"defaultGateway\":\"" << escapeString(settings.defaultGateway) << "\",";
    oss << "\"dhcpLeaseTimeRemaining\":" << settings.dhcpLeaseTimeRemaining << ",";
    oss << "\"interfacePriority\":" << settings.interfacePriority << ",";
    oss << "\"dnsServers\":[";
    
    for (size_t i = 0; i < settings.dnsServers.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeString(settings.dnsServers[i]) << "\"";
    }
    
    oss << "],\"searchDomains\":[";
    
    for (size_t i = 0; i < settings.searchDomains.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeString(settings.searchDomains[i]) << "\"";
    }
    
    oss << "]}";
    return oss.str();
}

std::string JsonConfig::serialize(const AdvancedSettings& settings) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"mtuSize\":" << settings.mtuSize << ",";
    oss << "\"checksumOffload\":" << (settings.checksumOffload ? "true" : "false") << ",";
    oss << "\"tcpSegmentationOffload\":" << (settings.tcpSegmentationOffload ? "true" : "false") << ",";
    oss << "\"rssOffload\":" << (settings.rssOffload ? "true" : "false") << ",";
    oss << "\"lsoOffload\":" << (settings.lsoOffload ? "true" : "false") << ",";
    oss << "\"linkNegotiationMode\":\"" << escapeString(settings.linkNegotiationMode) << "\",";
    oss << "\"forcedSpeed\":" << settings.forcedSpeed << ",";
    oss << "\"forcedDuplex\":\"" << escapeString(settings.forcedDuplex) << "\",";
    oss << "\"energyEfficientEthernet\":" << (settings.energyEfficientEthernet ? "true" : "false");
    oss << "}";
    return oss.str();
}

std::string JsonConfig::serialize(const ConnectionProfile& profile) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"name\":\"" << escapeString(profile.name) << "\",";
    oss << "\"description\":\"" << escapeString(profile.description) << "\",";
    oss << "\"basicSettings\":" << serialize(profile.basicSettings) << ",";
    oss << "\"advancedSettings\":" << serialize(profile.advancedSettings) << ",";
    oss << "\"lastUsed\":" << std::chrono::system_clock::to_time_t(profile.lastUsed) << ",";
    oss << "\"gatewayInfo\":\"" << escapeString(profile.gatewayInfo) << "\",";
    oss << "\"autoApplyConditions\":[";
    
    for (size_t i = 0; i < profile.autoApplyConditions.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeString(profile.autoApplyConditions[i]) << "\"";
    }
    
    oss << "]}";
    return oss.str();
}

std::string JsonConfig::serialize(const std::vector<ConnectionProfile>& profiles) {
    std::ostringstream oss;
    oss << "[";
    
    for (size_t i = 0; i < profiles.size(); ++i) {
        if (i > 0) oss << ",";
        oss << serialize(profiles[i]);
    }
    
    oss << "]";
    return oss.str();
}

std::string JsonConfig::escapeString(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

// Configuration Validator Implementation
bool ConfigValidator::validateIPv4(const std::string& ip) {
    return Utils::isValidIPv4(ip);
}

bool ConfigValidator::validateIPv6(const std::string& ip) {
    return Utils::isValidIPv6(ip);
}

bool ConfigValidator::validateMac(const std::string& mac) {
    return Utils::isValidMac(mac);
}

bool ConfigValidator::validateSubnetMask(const std::string& mask) {
    return Utils::isValidIPv4(mask);
}

bool ConfigValidator::validateMtu(int mtu) {
    return mtu >= 576 && mtu <= 9000;
}

bool ConfigValidator::validateConnectionMode(const std::string& mode) {
    std::string lowerMode = Utils::toLower(mode);
    return lowerMode == "dhcp" || lowerMode == "static";
}

bool ConfigValidator::validateDnsServers(const std::vector<std::string>& dns) {
    for (const auto& server : dns) {
        if (!validateIPv4(server)) {
            return false;
        }
    }
    return true;
}

bool ConfigValidator::validateProfile(const ConnectionProfile& profile) {
    auto errors = getValidationErrors(profile);
    return errors.empty();
}

std::vector<std::string> ConfigValidator::getValidationErrors(const ConnectionProfile& profile) {
    std::vector<std::string> errors;
    
    if (profile.name.empty()) {
        errors.push_back("Profile name cannot be empty");
    }
    
    if (!validateConnectionMode(profile.basicSettings.connectionMode)) {
        errors.push_back("Invalid connection mode: " + profile.basicSettings.connectionMode);
    }
    
    if (profile.basicSettings.connectionMode == "static") {
        if (!validateIPv4(profile.basicSettings.ipv4Address)) {
            errors.push_back("Invalid IPv4 address: " + profile.basicSettings.ipv4Address);
        }
        
        if (!validateSubnetMask(profile.basicSettings.subnetMask)) {
            errors.push_back("Invalid subnet mask: " + profile.basicSettings.subnetMask);
        }
        
        if (!validateIPv4(profile.basicSettings.defaultGateway)) {
            errors.push_back("Invalid gateway address: " + profile.basicSettings.defaultGateway);
        }
    }
    
    if (!validateDnsServers(profile.basicSettings.dnsServers)) {
        errors.push_back("One or more DNS servers are invalid");
    }
    
    if (!validateMtu(profile.advancedSettings.mtuSize)) {
        errors.push_back("MTU size must be between 576 and 9000");
    }
    
    return errors;
}

// Logger Implementation
LogLevel Logger::currentLevel = LogLevel::INFO;
std::string Logger::logFile = "";
bool Logger::fileLogging = false;

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::setLogFile(const std::string& filename) {
    logFile = filename;
}

void Logger::enableFileLogging(bool enable) {
    fileLogging = enable;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel) {
        return;
    }
    
    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = levelToString(level);
    std::string logLine = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    // Console output
    std::cout << logLine << std::endl;
    
    // File output
    if (fileLogging && !logFile.empty()) {
        std::ofstream file(logFile, std::ios::app);
        if (file.is_open()) {
            file << logLine << std::endl;
            file.close();
        }
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&timeT);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Event Manager Implementation
EventManager& EventManager::getInstance() {
    static EventManager instance;
    return instance;
}

void EventManager::subscribe(NetworkEvent event, std::function<void(const std::string&)> callback) {
    listeners[event].push_back(callback);
}

void EventManager::unsubscribe(NetworkEvent event) {
    listeners[event].clear();
}

void EventManager::emit(NetworkEvent event, const std::string& data) {
    for (auto& callback : listeners[event]) {
        try {
            callback(data);
        } catch (const std::exception& e) {
            Logger::error("Event callback error: " + std::string(e.what()));
        }
    }
}

// Backup Manager Implementation
bool BackupManager::createBackup(const std::string& configPath, const std::string& backupName) {
    std::string name = backupName.empty() ? "backup_" + Utils::getTimestamp() : backupName;
    std::string backupPath = getBackupPath(name);
    
    if (!Utils::fileExists(configPath)) {
        Logger::error("Config file not found: " + configPath);
        return false;
    }
    
    std::ifstream src(configPath, std::ios::binary);
    std::ofstream dst(backupPath, std::ios::binary);
    
    if (!src.is_open() || !dst.is_open()) {
        Logger::error("Failed to create backup: " + backupPath);
        return false;
    }
    
    dst << src.rdbuf();
    Logger::info("Backup created: " + backupPath);
    return true;
}

bool BackupManager::restoreBackup(const std::string& backupName, const std::string& configPath) {
    std::string backupPath = getBackupPath(backupName);
    
    if (!Utils::fileExists(backupPath)) {
        Logger::error("Backup not found: " + backupPath);
        return false;
    }
    
    // Create backup of current config before restoring
    createBackup(configPath, "before_restore_" + Utils::getTimestamp());
    
    std::ifstream src(backupPath, std::ios::binary);
    std::ofstream dst(configPath, std::ios::binary);
    
    if (!src.is_open() || !dst.is_open()) {
        Logger::error("Failed to restore backup");
        return false;
    }
    
    dst << src.rdbuf();
    Logger::info("Backup restored: " + backupPath);
    return true;
}

std::vector<std::string> BackupManager::listBackups() {
    std::vector<std::string> backups;
    std::string backupDir = getBackupDir();
    
    if (!Utils::fileExists(backupDir)) {
        return backups;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
        if (entry.is_regular_file()) {
            backups.push_back(entry.path().filename().string());
        }
    }
    
    std::sort(backups.rbegin(), backups.rend()); // Sort by name (timestamp) descending
    return backups;
}

bool BackupManager::deleteBackup(const std::string& backupName) {
    std::string backupPath = getBackupPath(backupName);
    
    if (!Utils::fileExists(backupPath)) {
        Logger::error("Backup not found: " + backupPath);
        return false;
    }
    
    if (std::filesystem::remove(backupPath)) {
        Logger::info("Backup deleted: " + backupPath);
        return true;
    }
    
    Logger::error("Failed to delete backup: " + backupPath);
    return false;
}

// Initialize static member
std::string BackupManager::backupDirectory = "/etc/network-backups";

std::string BackupManager::getBackupDir() {
    return backupDirectory;
}

void BackupManager::setBackupDirectory(const std::string& backupDir) {
    backupDirectory = backupDir;
}

std::string BackupManager::getBackupPath(const std::string& backupName) {
    return getBackupDir() + "/" + backupName;
}

// Network Diagnostics Implementation
NetworkDiagnostics::DiagnosticResult NetworkDiagnostics::testConnectivity(const std::string& host) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string command = "ping -c 3 -W 5 " + host + " > /dev/null 2>&1";
    int result = system(command.c_str());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    DiagnosticResult diag;
    diag.test = "Connectivity Test (" + host + ")";
    diag.duration = duration;
    diag.success = (result == 0);
    diag.message = diag.success ? "Successfully connected to " + host : "Failed to connect to " + host;
    
    return diag;
}

NetworkDiagnostics::DiagnosticResult NetworkDiagnostics::testDnsResolution(const std::string& domain) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string command = "nslookup " + domain + " > /dev/null 2>&1";
    int result = system(command.c_str());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    DiagnosticResult diag;
    diag.test = "DNS Resolution (" + domain + ")";
    diag.duration = duration;
    diag.success = (result == 0);
    diag.message = diag.success ? "Successfully resolved " + domain : "Failed to resolve " + domain;
    
    return diag;
}

NetworkDiagnostics::DiagnosticResult NetworkDiagnostics::testGatewayReachability() {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string gateway = Utils::executeCommand("ip route show default | awk '/default/ {print $3}'");
    gateway = Utils::trim(gateway);
    
    if (gateway.empty()) {
        DiagnosticResult diag;
        diag.test = "Gateway Reachability";
        diag.duration = std::chrono::milliseconds(0);
        diag.success = false;
        diag.message = "No gateway found";
        return diag;
    }
    
    DiagnosticResult result = testConnectivity(gateway);
    result.test = "Gateway Reachability (" + gateway + ")";
    return result;
}

NetworkDiagnostics::DiagnosticResult NetworkDiagnostics::testInterfaceStatus(const std::string& interface) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string output = Utils::executeCommand("ip link show " + interface);
    bool isUp = output.find("UP") != std::string::npos;
    bool hasCarrier = output.find("NO-CARRIER") == std::string::npos;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    DiagnosticResult diag;
    diag.test = "Interface Status (" + interface + ")";
    diag.duration = duration;
    diag.success = isUp && hasCarrier;
    diag.message = diag.success ? "Interface is up and has carrier" : 
                                   std::string("Interface is ") + (isUp ? "up but no carrier" : "down");
    
    return diag;
}

NetworkDiagnostics::DiagnosticResult NetworkDiagnostics::testDhcpServer() {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string command = "udhcpc -n -t 3 -T 5 eth0 > /dev/null 2>&1";
    int result = system(command.c_str());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    DiagnosticResult diag;
    diag.test = "DHCP Server Test";
    diag.duration = duration;
    diag.success = (result == 0);
    diag.message = diag.success ? "DHCP server responded successfully" : "DHCP server not responding";
    
    return diag;
}

NetworkDiagnostics::DiagnosticResult NetworkDiagnostics::testSpeed() {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string output = Utils::executeCommand("ethtool eth0 | grep Speed");
    bool hasSpeed = output.find("Speed:") != std::string::npos;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    DiagnosticResult diag;
    diag.test = "Link Speed Test";
    diag.duration = duration;
    diag.success = hasSpeed;
    diag.message = diag.success ? Utils::trim(output) : "Unable to determine link speed";
    
    return diag;
}

std::vector<NetworkDiagnostics::DiagnosticResult> NetworkDiagnostics::runFullDiagnostics() {
    std::vector<DiagnosticResult> results;
    
    Logger::info("Running full network diagnostics...");
    
    results.push_back(testInterfaceStatus());
    results.push_back(testGatewayReachability());
    results.push_back(testConnectivity());
    results.push_back(testDnsResolution());
    results.push_back(testDhcpServer());
    results.push_back(testSpeed());
    
    Logger::info("Diagnostics completed");
    return results;
}

std::string NetworkDiagnostics::generateReport(const std::vector<DiagnosticResult>& results) {
    std::ostringstream oss;
    
    oss << "Network Diagnostics Report" << std::endl;
    oss << "==========================" << std::endl;
    oss << "Generated: " << Logger::getCurrentTimestamp() << std::endl;
    oss << std::endl;
    
    int passed = 0, total = results.size();
    
    for (const auto& result : results) {
        oss << "[" << (result.success ? "PASS" : "FAIL") << "] " << result.test << std::endl;
        oss << "    Message: " << result.message << std::endl;
        oss << "    Duration: " << result.duration.count() << "ms" << std::endl;
        oss << std::endl;
        
        if (result.success) {
            passed++;
        }
    }
    
    oss << "Summary: " << passed << "/" << total << " tests passed" << std::endl;
    
    if (passed == total) {
        oss << "Overall Status: ✅ All tests passed" << std::endl;
    } else {
        oss << "Overall Status: ❌ " << (total - passed) << " test(s) failed" << std::endl;
    }
    
    return oss.str();
}

} // namespace OpenWrtNetwork
