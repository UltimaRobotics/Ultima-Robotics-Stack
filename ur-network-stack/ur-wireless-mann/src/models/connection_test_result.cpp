#include "urwt/models/connection_test_result.hpp"

namespace urwt {

void to_json(json& j, const ConnectionTestResult& result) {
    j = json{
        {"ssid", result.ssid},
        {"interface", result.interface},
        {"connection_type", result.connection_type},
        {"success", result.success},
        {"test_time", std::chrono::system_clock::to_time_t(result.timestamp)},
        {"test_duration_ms", result.duration.count()},
        {"was_previously_connected", result.was_connected}
    };

    if (result.original_ssid) {
        j["original_ssid"] = *result.original_ssid;
    }

    if (!result.success && result.error_message) {
        j["error_message"] = *result.error_message;
    }
}

void from_json(const json& j, ConnectionTestResult& result) {
    result.ssid = j.at("ssid").get<std::string>();
    result.interface = j.at("interface").get<std::string>();
    result.connection_type = j.at("connection_type").get<std::string>();
    result.success = j.at("success").get<bool>();
    result.was_connected = j.at("was_previously_connected").get<bool>();
    
    if (j.contains("original_ssid")) {
        result.original_ssid = j.at("original_ssid").get<std::string>();
    }
    
    if (j.contains("error_message")) {
        result.error_message = j.at("error_message").get<std::string>();
    }
}

}
