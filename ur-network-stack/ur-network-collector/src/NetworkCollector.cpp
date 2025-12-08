#include "NetworkCollector.h"
#include <cstdlib>
#include <cstdio>
#include <array>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <unistd.h>

std::string CommandExecutor::executeCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;
    
    std::string cmd = command + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to execute command: " + command);
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    int exit_code = pclose(pipe);
    if (exit_code != 0) {
        // Check for common permission-related errors
        if (result.find("Permission denied") != std::string::npos || 
            result.find("Operation not permitted") != std::string::npos ||
            exit_code == 4) {  // Permission denied exit code
            throw std::runtime_error("Permission denied: " + command + " (requires root privileges)");
        }
        throw std::runtime_error("Command failed with exit code " + std::to_string(exit_code) + ": " + command + "\nOutput: " + result);
    }
    
    return result;
}

std::vector<std::string> CommandExecutor::executeCommandLines(const std::string& command) {
    std::string output = executeCommand(command);
    std::vector<std::string> lines;
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    return lines;
}

bool CommandExecutor::commandExists(const std::string& command) {
    std::string check_cmd = "which " + command + " > /dev/null 2>&1";
    return system(check_cmd.c_str()) == 0;
}

std::string NetworkCollector::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

bool NetworkCollector::hasRootPrivileges() const {
    return geteuid() == 0;
}
