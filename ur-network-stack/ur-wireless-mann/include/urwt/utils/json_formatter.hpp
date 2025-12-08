#ifndef URWT_UTILS_JSON_FORMATTER_HPP
#define URWT_UTILS_JSON_FORMATTER_HPP

#include <string>
#include <vector>
#include "../models/wifi_interface.hpp"
#include "../models/scan_result.hpp"
#include "../models/connection_test_result.hpp"

namespace urwt {

class JSONFormatter {
public:
    static std::string formatInterfaceList(const std::vector<WifiInterface>& interfaces);
    static std::string formatScanResult(const ScanResult& result);
    static std::string formatConnectionTest(const ConnectionTestResult& test);
    static std::string formatError(const std::string& error, const std::string& context = "");
    static std::string formatSuccess(const std::string& message, const json& data = json::object());

private:
    static constexpr int INDENT = 2;
};

}

#endif
