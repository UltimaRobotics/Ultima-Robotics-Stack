#include "command_logger.h"
#include <mutex>

bool CommandLogger::m_verbose_enabled = false;
static std::mutex logging_mutex;

void CommandLogger::setVerboseEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(logging_mutex);
    m_verbose_enabled = enabled;
    if (enabled) {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "VERBOSE COMMAND LOGGING ENABLED" << std::endl;
        std::cout << std::string(80, '=') << "\n" << std::endl;
    }
}

bool CommandLogger::isVerboseEnabled() {
    std::lock_guard<std::mutex> lock(logging_mutex);
    return m_verbose_enabled;
}

void CommandLogger::logCommand(const std::string& command) {
    if (!isVerboseEnabled()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(logging_mutex);
    std::cout << "[" << getCurrentTimestamp() << "] EXECUTING COMMAND:" << std::endl;
    std::cout << "  > " << command << std::endl;
}

void CommandLogger::logCommandResult(const std::string& command, const std::string& result, int exit_code) {
    (void)command;  // Suppress unused parameter warning
    if (!isVerboseEnabled()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(logging_mutex);
    std::cout << "[" << getCurrentTimestamp() << "] COMMAND RESULT (exit code: " << exit_code << "):" << std::endl;
    
    if (!result.empty()) {
        std::cout << "  OUTPUT:" << std::endl;
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line)) {
            std::cout << "    | " << line << std::endl;
        }
    } else {
        std::cout << "  OUTPUT: (no output)" << std::endl;
    }
    printSeparator();
}

void CommandLogger::logCommandWithResult(const std::string& command, const std::string& result, int exit_code) {
    if (!isVerboseEnabled()) {
        return;
    }
    
    logCommand(command);
    logCommandResult(command, result, exit_code);
}

std::string CommandLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void CommandLogger::printSeparator() {
    std::cout << std::string(60, '-') << std::endl << std::endl;
}