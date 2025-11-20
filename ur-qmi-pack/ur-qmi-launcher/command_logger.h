#ifndef COMMAND_LOGGER_H
#define COMMAND_LOGGER_H

#include <string>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>

class CommandLogger {
public:
    static void setVerboseEnabled(bool enabled);
    static bool isVerboseEnabled();
    static void logCommand(const std::string& command);
    static void logCommandResult(const std::string& command, const std::string& result, int exit_code = 0);
    static void logCommandWithResult(const std::string& command, const std::string& result, int exit_code = 0);

private:
    static bool m_verbose_enabled;
    static std::string getCurrentTimestamp();
    static void printSeparator();
};

#endif // COMMAND_LOGGER_H