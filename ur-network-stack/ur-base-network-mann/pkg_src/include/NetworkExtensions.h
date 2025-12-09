#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <chrono>
#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include "NetworkConfigAPI.h"

namespace OpenWrtNetwork {

// Exception classes for error handling
class NetworkException : public std::exception {
private:
    std::string message;
public:
    explicit NetworkException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class ConfigException : public NetworkException {
public:
    explicit ConfigException(const std::string& msg) : NetworkException(msg) {}
};

class ProfileException : public NetworkException {
public:
    explicit ProfileException(const std::string& msg) : NetworkException(msg) {}
};

class MonitorException : public NetworkException {
public:
    explicit MonitorException(const std::string& msg) : NetworkException(msg) {}
};

// JSON serialization support
class JsonConfig {
public:
    static std::string serialize(const NetworkStatus& status);
    static std::string serialize(const BasicSettings& settings);
    static std::string serialize(const AdvancedSettings& settings);
    static std::string serialize(const ConnectionProfile& profile);
    static std::string serialize(const std::vector<ConnectionProfile>& profiles);
    
    static NetworkStatus deserializeNetworkStatus(const std::string& json);
    static BasicSettings deserializeBasicSettings(const std::string& json);
    static AdvancedSettings deserializeAdvancedSettings(const std::string& json);
    static ConnectionProfile deserializeProfile(const std::string& json);
    static std::vector<ConnectionProfile> deserializeProfiles(const std::string& json);
    
private:
    static std::string escapeString(const std::string& str);
    static std::string unescapeString(const std::string& str);
    static std::map<std::string, std::string> parseJsonMap(const std::string& json);
};

// Configuration validation
class ConfigValidator {
public:
    static bool validateIPv4(const std::string& ip);
    static bool validateIPv6(const std::string& ip);
    static bool validateMac(const std::string& mac);
    static bool validateSubnetMask(const std::string& mask);
    static bool validateMtu(int mtu);
    static bool validateConnectionMode(const std::string& mode);
    static bool validateDnsServers(const std::vector<std::string>& dns);
    static bool validateProfile(const ConnectionProfile& profile);
    
    static std::vector<std::string> getValidationErrors(const ConnectionProfile& profile);
};

// Logging system
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
private:
    static LogLevel currentLevel;
    static std::string logFile;
    static bool fileLogging;
    
public:
    static void setLevel(LogLevel level);
    static void setLogFile(const std::string& filename);
    static void enableFileLogging(bool enable);
    
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    
    static void log(LogLevel level, const std::string& message);
    
    static std::string getCurrentTimestamp();
    
private:
    static std::string levelToString(LogLevel level);
};

// Event system for notifications
enum class NetworkEvent {
    CONNECTION_ESTABLISHED,
    CONNECTION_LOST,
    IP_CHANGED,
    GATEWAY_CHANGED,
    DNS_CHANGED,
    SPEED_THRESHOLD,
    ERROR_OCCURRED
};

class EventManager {
private:
    std::map<NetworkEvent, std::vector<std::function<void(const std::string&)>>> listeners;
    
public:
    void subscribe(NetworkEvent event, std::function<void(const std::string&)> callback);
    void unsubscribe(NetworkEvent event);
    void emit(NetworkEvent event, const std::string& data = "");
    
    static EventManager& getInstance();
};

// Configuration backup and restore
class BackupManager {
public:
    static bool createBackup(const std::string& configPath, const std::string& backupName = "");
    static bool restoreBackup(const std::string& backupName, const std::string& configPath);
    static std::vector<std::string> listBackups();
    static bool deleteBackup(const std::string& backupName);
    
    static void setBackupDirectory(const std::string& backupDir);
    static bool initialize();
    
private:
    static std::string backupDirectory;
    static std::string getBackupDir();
    static std::string getBackupPath(const std::string& backupName);
};

// Network diagnostics
class NetworkDiagnostics {
public:
    struct DiagnosticResult {
        bool success;
        std::string test;
        std::string message;
        std::chrono::milliseconds duration;
    };
    
    static DiagnosticResult testConnectivity(const std::string& host = "8.8.8.8");
    static DiagnosticResult testDnsResolution(const std::string& domain = "google.com");
    static DiagnosticResult testGatewayReachability();
    static DiagnosticResult testInterfaceStatus(const std::string& interface = "eth0");
    static DiagnosticResult testDhcpServer();
    static DiagnosticResult testSpeed();
    
    static std::vector<DiagnosticResult> runFullDiagnostics();
    static std::string generateReport(const std::vector<DiagnosticResult>& results);
};

} // namespace OpenWrtNetwork
