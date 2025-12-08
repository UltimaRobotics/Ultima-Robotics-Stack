#include "urwt/managers/connection_tester.hpp"

namespace urwt {

ConnectionTester::ConnectionTester(std::shared_ptr<ProcessExecutor> executor)
    : executor_(executor ? executor : std::make_shared<ProcessExecutor>()) {}

Result<ConnectionTestResult, std::string> ConnectionTester::testConnection(
    const WifiInterface& interface,
    const std::string& ssid
) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::optional<std::string> original_ssid = getCurrentSSID(interface.name());
    bool was_connected = original_ssid.has_value();
    
    auto connect_result = connect(interface.name(), ssid);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    ConnectionTestResult test_result{
        connect_result.isOk() && connect_result.value(),
        ssid,
        interface.name(),
        "test",
        duration,
        std::chrono::system_clock::now(),
        std::nullopt,
        was_connected,
        original_ssid
    };
    
    if (!test_result.success) {
        test_result.error_message = connect_result.isError() ? 
            connect_result.error() : "Connection failed";
    }
    
    if (was_connected && original_ssid) {
        disconnect(interface.name());
        connect(interface.name(), *original_ssid);
    } else if (!was_connected) {
        disconnect(interface.name());
    }
    
    return Result<ConnectionTestResult, std::string>::ok(test_result);
}

Result<bool, std::string> ConnectionTester::connect(const std::string& interface, const std::string& ssid) {
    auto result = executor_->execute("nmcli", {"dev", "wifi", "connect", ssid, "ifname", interface});
    
    if (result.isError()) {
        return Result<bool, std::string>::error(result.error());
    }
    
    return Result<bool, std::string>::ok(result.value().exit_code == 0);
}

Result<bool, std::string> ConnectionTester::disconnect(const std::string& interface) {
    auto result = executor_->execute("nmcli", {"dev", "disconnect", interface});
    
    if (result.isError()) {
        return Result<bool, std::string>::error(result.error());
    }
    
    return Result<bool, std::string>::ok(result.value().exit_code == 0);
}

std::optional<std::string> ConnectionTester::getCurrentSSID(const std::string& interface) {
    auto result = executor_->execute("nmcli", {"-t", "-f", "active,ssid", "dev", "wifi"});
    
    if (result.isError() || result.value().exit_code != 0) {
        return std::nullopt;
    }
    
    return std::nullopt;
}

}
