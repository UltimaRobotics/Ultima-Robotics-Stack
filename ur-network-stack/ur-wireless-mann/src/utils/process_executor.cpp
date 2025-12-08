#include "urwt/utils/process_executor.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace urwt {

Result<ProcessResult, std::string> ProcessExecutor::execute(
    const std::string& command,
    const std::vector<std::string>& args,
    std::optional<std::chrono::milliseconds> timeout
) {
    std::string full_command = command;
    for (const auto& arg : args) {
        full_command += " " + escapeArg(arg);
    }
    
    return executeInternal(full_command, timeout);
}

Result<ProcessResult, std::string> ProcessExecutor::executeShell(
    const std::string& command,
    std::optional<std::chrono::milliseconds> timeout
) {
    return executeInternal(command, timeout);
}

std::string ProcessExecutor::escapeArg(const std::string& arg) {
    if (arg.find(' ') == std::string::npos && 
        arg.find('"') == std::string::npos &&
        arg.find('\'') == std::string::npos) {
        return arg;
    }
    
    std::string escaped = "'";
    for (char c : arg) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

Result<ProcessResult, std::string> ProcessExecutor::executeInternal(
    const std::string& full_command,
    std::optional<std::chrono::milliseconds> timeout
) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::array<char, 128> buffer;
    std::string result_stdout;
    std::string result_stderr;
    
    std::string cmd = full_command + " 2>&1";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    
    if (!pipe) {
        return Result<ProcessResult, std::string>::error("Failed to execute command: " + full_command);
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result_stdout += buffer.data();
    }
    
    int status = pclose(pipe.release());
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    ProcessResult result{
        exit_code,
        result_stdout,
        result_stderr,
        duration,
        false
    };
    
    return Result<ProcessResult, std::string>::ok(result);
}

}
