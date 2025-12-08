#ifndef URWT_MODELS_CONNECTION_TEST_RESULT_HPP
#define URWT_MODELS_CONNECTION_TEST_RESULT_HPP

#include <string>
#include <optional>
#include <chrono>
#include "../common/types.hpp"

namespace urwt {

struct ConnectionTestResult {
    bool success;
    std::string ssid;
    std::string interface;
    std::string connection_type;
    std::chrono::milliseconds duration;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::string> error_message;
    bool was_connected;
    std::optional<std::string> original_ssid;
};

void to_json(json& j, const ConnectionTestResult& result);
void from_json(const json& j, ConnectionTestResult& result);

}

#endif
