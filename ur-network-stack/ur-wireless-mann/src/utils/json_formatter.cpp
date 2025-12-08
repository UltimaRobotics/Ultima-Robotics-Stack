#include "urwt/utils/json_formatter.hpp"

namespace urwt {

std::string JSONFormatter::formatInterfaceList(const std::vector<WifiInterface>& interfaces) {
    json j = json{
        {"wifi_interfaces", interfaces},
        {"count", interfaces.size()},
        {"status", "success"}
    };
    return j.dump(INDENT);
}

std::string JSONFormatter::formatScanResult(const ScanResult& result) {
    json j = json{
        {"scan_result", result},
        {"status", "success"}
    };
    return j.dump(INDENT);
}

std::string JSONFormatter::formatConnectionTest(const ConnectionTestResult& test) {
    json j = json{
        {"connection_test", test},
        {"status", test.success ? "success" : "failed"}
    };
    return j.dump(INDENT);
}

std::string JSONFormatter::formatError(const std::string& error, const std::string& context) {
    json j = json{
        {"error", error},
        {"status", "failed"}
    };

    if (!context.empty()) {
        j["context"] = context;
    }

    return j.dump(INDENT);
}

std::string JSONFormatter::formatSuccess(const std::string& message, const json& data) {
    json j = json{
        {"message", message},
        {"status", "success"}
    };
    
    if (!data.empty()) {
        j["data"] = data;
    }

    return j.dump(INDENT);
}

}
