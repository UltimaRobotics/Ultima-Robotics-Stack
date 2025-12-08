#ifndef URWT_API_HPP
#define URWT_API_HPP

#include <memory>
#include <vector>
#include "models/wifi_interface.hpp"
#include "models/scan_result.hpp"
#include "models/connection_test_result.hpp"
#include "utils/result.hpp"

namespace urwt {

class InterfaceManager;
class ScanStrategy;
class ConnectionTester;

class WirelessToolsAPI {
public:
    WirelessToolsAPI();
    ~WirelessToolsAPI();

    Result<std::vector<WifiInterface>, std::string> listInterfaces();
    Result<WifiInterface, std::string> getInterface(const std::string& name);
    Result<ScanResult, std::string> scan(const WifiInterface& interface);
    Result<ConnectionTestResult, std::string> testConnection(
        const WifiInterface& interface,
        const std::string& ssid
    );

    void setScanStrategy(std::shared_ptr<ScanStrategy> strategy);

private:
    std::shared_ptr<InterfaceManager> interface_manager_;
    std::shared_ptr<ScanStrategy> scan_strategy_;
    std::shared_ptr<ConnectionTester> connection_tester_;
};

}

#endif
