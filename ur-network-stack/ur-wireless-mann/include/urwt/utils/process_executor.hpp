#ifndef URWT_UTILS_PROCESS_EXECUTOR_HPP
#define URWT_UTILS_PROCESS_EXECUTOR_HPP

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include "../utils/result.hpp"

namespace urwt {

struct ProcessResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    std::chrono::milliseconds duration;
    bool timed_out;
};

class ProcessExecutor {
public:
    ProcessExecutor() = default;

    Result<ProcessResult, std::string> execute(
        const std::string& command,
        const std::vector<std::string>& args = {},
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    );

    Result<ProcessResult, std::string> executeShell(
        const std::string& command,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    );

    static std::string escapeArg(const std::string& arg);

private:
    Result<ProcessResult, std::string> executeInternal(
        const std::string& full_command,
        std::optional<std::chrono::milliseconds> timeout
    );
};

}

#endif
