#ifndef URWT_MANAGERS_CONNECTION_TESTER_HPP
#define URWT_MANAGERS_CONNECTION_TESTER_HPP

#include <memory>
#include <string>
#include "../models/connection_test_result.hpp"
#include "../models/wifi_interface.hpp"
#include "../utils/result.hpp"
#include "../utils/process_executor.hpp"

namespace urwt {

class ConnectionTester {
public:
    explicit ConnectionTester(std::shared_ptr<ProcessExecutor> executor = nullptr);

    Result<ConnectionTestResult, std::string> testConnection(
        const WifiInterface& interface,
        const std::string& ssid
    );

private:
    std::shared_ptr<ProcessExecutor> executor_;
    
    Result<bool, std::string> connect(const std::string& interface, const std::string& ssid);
    Result<bool, std::string> disconnect(const std::string& interface);
    std::optional<std::string> getCurrentSSID(const std::string& interface);
};

}

#endif
