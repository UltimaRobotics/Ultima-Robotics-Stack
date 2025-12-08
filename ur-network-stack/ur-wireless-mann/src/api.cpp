#include "urwt/api.hpp"
#include "urwt/managers/interface_manager.hpp"
#include "urwt/managers/connection_tester.hpp"
#include "urwt/strategies/forked_scan_strategy.hpp"

namespace urwt {

WirelessToolsAPI::WirelessToolsAPI()
    : interface_manager_(std::make_shared<InterfaceManager>())
    , scan_strategy_(std::make_shared<ForkedScanStrategy>())
    , connection_tester_(std::make_shared<ConnectionTester>()) {}

WirelessToolsAPI::~WirelessToolsAPI() = default;

Result<std::vector<WifiInterface>, std::string> WirelessToolsAPI::listInterfaces() {
    return interface_manager_->listInterfaces();
}

Result<WifiInterface, std::string> WirelessToolsAPI::getInterface(const std::string& name) {
    return interface_manager_->getInterface(name);
}

Result<ScanResult, std::string> WirelessToolsAPI::scan(const WifiInterface& interface) {
    if (!scan_strategy_) {
        return Result<ScanResult, std::string>::error("No scan strategy set");
    }
    
    return scan_strategy_->execute(interface);
}

Result<ConnectionTestResult, std::string> WirelessToolsAPI::testConnection(
    const WifiInterface& interface,
    const std::string& ssid
) {
    return connection_tester_->testConnection(interface, ssid);
}

void WirelessToolsAPI::setScanStrategy(std::shared_ptr<ScanStrategy> strategy) {
    scan_strategy_ = strategy;
}

}
