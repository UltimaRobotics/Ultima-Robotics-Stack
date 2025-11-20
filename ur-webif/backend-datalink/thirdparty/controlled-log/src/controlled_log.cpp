#include "controlled_log.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>

namespace ControlledLog {

// Static member definitions
LoggingConfig Logger::config_ = LoggingConfig();
std::mutex Logger::mutex_;
bool Logger::initialized_ = false;

// LoggingConfig implementation
LoggingConfig LoggingConfig::fromJson(const nlohmann::json& config) {
    LoggingConfig loggingConfig;
    
    if (config.contains("logging")) {
        const auto& logging = config["logging"];
        
        if (logging.contains("websockets")) {
            loggingConfig.websockets_enabled = logging["websockets"].get<bool>();
        }
        
        if (logging.contains("database")) {
            loggingConfig.database_enabled = logging["database"].get<bool>();
        }
        
        if (logging.contains("licence-cron")) {
            loggingConfig.licence_cron_enabled = logging["licence-cron"].get<bool>();
        }
        
        if (logging.contains("rpc")) {
            loggingConfig.rpc_enabled = logging["rpc"].get<bool>();
        }
        
        if (logging.contains("utility")) {
            loggingConfig.utility_enabled = logging["utility"].get<bool>();
        }
        
        // Always enable all levels for terminal logging
        loggingConfig.level = LogLevel::DEBUG;
    }
    
    return loggingConfig;
}

bool LoggingConfig::isComponentEnabled(const std::string& component) const {
    if (component == "websockets") return websockets_enabled;
    if (component == "database") return database_enabled;
    if (component == "licence-cron") return licence_cron_enabled;
    if (component == "rpc") return rpc_enabled;
    if (component == "utility") return utility_enabled;
    return false;
}

// Logger implementation
void Logger::initialize() {
    if (initialized_) {
        return;
    }
    
    // For terminal output, we don't need to create directories or open files
    // Just mark as initialized
    initialized_ = true;
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::writeLog(LogLevel level, const std::string& component, const std::string& message) {
    if (!initialized_) {
        Logger::initialize();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Output to terminal with colors and formatting
    std::string timestamp = Logger::getTimestamp();
    std::string levelStr = Logger::levelToString(level);
    
    // Color codes for terminal output
    const char* color_reset = "\033[0m";
    const char* color_debug = "\033[90m";    // Gray
    const char* color_info = "\033[32m";     // Green
    const char* color_warn = "\033[33m";     // Yellow
    const char* color_error = "\033[31m";    // Red
    const char* color_component = "\033[36m"; // Cyan
    
    const char* level_color = color_reset;
    switch (level) {
        case LogLevel::DEBUG: level_color = color_debug; break;
        case LogLevel::INFO: level_color = color_info; break;
        case LogLevel::WARN: level_color = color_warn; break;
        case LogLevel::ERROR: level_color = color_error; break;
    }
    
    std::cout << color_component << "[" << timestamp << "] "
              << level_color << "[" << levelStr << "] "
              << color_component << "[" << component << "] "
              << color_reset << message << std::endl;
}

void Logger::setConfig(const LoggingConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reset initialization for terminal output
    if (initialized_) {
        initialized_ = false;
    }
    
    config_ = config;
    Logger::initialize();
}

const LoggingConfig& Logger::getConfig() {
    return config_;
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    if (!config_.isComponentEnabled(component) || !config_.shouldLog(level)) {
        return;
    }
    
    writeLog(level, component, message);
}

void Logger::logWebsockets(LogLevel level, const std::string& message) {
    Logger::log(level, "websockets", message);
}

void Logger::logDatabase(LogLevel level, const std::string& message) {
    Logger::log(level, "database", message);
}

void Logger::logLicenseCron(LogLevel level, const std::string& message) {
    Logger::log(level, "licence-cron", message);
}

void Logger::logRpc(LogLevel level, const std::string& message) {
    Logger::log(level, "rpc", message);
}

void Logger::logUtility(LogLevel level, const std::string& message) {
    Logger::log(level, "utility", message);
}

void Logger::flush() {
    // For terminal output, no flush needed
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // For terminal output, no cleanup needed
    initialized_ = false;
}

} // namespace ControlledLog
