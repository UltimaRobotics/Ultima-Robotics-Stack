#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>

namespace ControlledLog {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

struct LoggingConfig {
    bool websockets_enabled = true;
    bool database_enabled = true;
    bool licence_cron_enabled = true;
    bool rpc_enabled = true;
    bool utility_enabled = true;
    LogLevel level = LogLevel::DEBUG; // Enable all levels by default
    
    // Load from JSON configuration
    static LoggingConfig fromJson(const nlohmann::json& config);
    
    // Check if logging is enabled for a specific component
    bool isComponentEnabled(const std::string& component) const;
    
    // Check if log level should be output (always true for terminal logging)
    bool shouldLog(LogLevel /* level */) const { return true; }
};

class Logger {
private:
    static LoggingConfig config_;
    static std::mutex mutex_;
    static bool initialized_;
    
    // Initialize the logger
    static void initialize();
    
    // Get current timestamp string
    static std::string getTimestamp();
    
    // Convert log level to string
    static std::string levelToString(LogLevel level);
    
    // Write log entry
    static void writeLog(LogLevel level, const std::string& component, const std::string& message);

public:
    // Set configuration
    static void setConfig(const LoggingConfig& config);
    
    // Get configuration
    static const LoggingConfig& getConfig();
    
    // Main logging function
    static void log(LogLevel level, const std::string& component, const std::string& message);
    
    // Convenience methods for different components
    static void logWebsockets(LogLevel level, const std::string& message);
    static void logDatabase(LogLevel level, const std::string& message);
    static void logLicenseCron(LogLevel level, const std::string& message);
    static void logRpc(LogLevel level, const std::string& message);
    static void logUtility(LogLevel level, const std::string& message);
    
    // Flush and cleanup
    static void flush();
    static void shutdown();
};

} // namespace ControlledLog

// Logging macros for different components
#define LOG_WEBSOCKETS_INFO(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("websockets")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::INFO, "websockets", message); \
        } \
    } while(0)

#define LOG_WEBSOCKETS_ERROR(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("websockets")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::ERROR, "websockets", message); \
        } \
    } while(0)

#define LOG_DATABASE_INFO(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("database")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::INFO, "database", message); \
        } \
    } while(0)

#define LOG_DATABASE_ERROR(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("database")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::ERROR, "database", message); \
        } \
    } while(0)

#define LOG_LICENSE_CRON_INFO(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("licence-cron")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::INFO, "licence-cron", message); \
        } \
    } while(0)

#define LOG_LICENSE_CRON_WARN(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("licence-cron")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::WARN, "licence-cron", message); \
        } \
    } while(0)

#define LOG_LICENSE_CRON_ERROR(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("licence-cron")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::ERROR, "licence-cron", message); \
        } \
    } while(0)

#define LOG_LICENSE_CRON_DEBUG(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("licence-cron")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::DEBUG, "licence-cron", message); \
        } \
    } while(0)

#define LOG_RPC_INFO(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("rpc")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::INFO, "rpc", message); \
        } \
    } while(0)

#define LOG_RPC_WARN(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("rpc")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::WARN, "rpc", message); \
        } \
    } while(0)

#define LOG_RPC_ERROR(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("rpc")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::ERROR, "rpc", message); \
        } \
    } while(0)

#define LOG_RPC_DEBUG(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("rpc")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::DEBUG, "rpc", message); \
        } \
    } while(0)

#define LOG_UTILITY_INFO(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("utility")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::INFO, "utility", message); \
        } \
    } while(0)

#define LOG_UTILITY_ERROR(message) \
    do { \
        if (ControlledLog::Logger::getConfig().isComponentEnabled("utility")) { \
            ControlledLog::Logger::log(ControlledLog::LogLevel::ERROR, "utility", message); \
        } \
    } while(0)
