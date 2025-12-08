#ifndef URWT_MODE_AP_MODE_MANAGER_HPP
#define URWT_MODE_AP_MODE_MANAGER_HPP

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include "urwt/api.hpp"
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/state/wireless_state.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace mode {

using namespace config;
using namespace state;

struct APClient {
    std::string mac_address;
    std::string ip_address;
    std::string hostname;
    std::chrono::system_clock::time_point connected_at;
    
    APClient() : connected_at(std::chrono::system_clock::now()) {}
};

class APModeManager {
public:
    explicit APModeManager(std::shared_ptr<WirelessToolsAPI> api);
    ~APModeManager();

    Result<bool, std::string> startAP(const APModeConfig& config);
    Result<bool, std::string> stopAP();
    bool isAPRunning() const;

    Result<bool, std::string> updateAPConfig(const APModeConfig& config);
    Result<std::string, std::string> generateHostapdConfig(
        const APModeConfig& config);
    Result<std::string, std::string> generateDnsmasqConfig(
        const APModeConfig& config);

    std::vector<APClient> getConnectedClients() const;
    size_t getClientCount() const;
    Result<bool, std::string> disconnectClient(const std::string& mac);

    Result<APModeState, std::string> getAPStatus() const;

private:
    std::shared_ptr<WirelessToolsAPI> api_;

    std::string hostapd_config_path_;
    std::string dnsmasq_config_path_;
    std::string hostapd_pid_file_;
    std::string dnsmasq_pid_file_;
    std::string interface_;

    Result<bool, std::string> configureIPForwarding();
    Result<bool, std::string> configureNAT(const std::string& interface);
    Result<bool, std::string> startHostapd(const std::string& config_path);
    Result<bool, std::string> startDnsmasq(const std::string& config_path);
    Result<bool, std::string> stopHostapd();
    Result<bool, std::string> stopDnsmasq();

    Result<std::vector<APClient>, std::string> parseConnectedClients(
        const std::string& output) const;
    Result<std::string, std::string> executeCommand(const std::string& cmd) const;
};

}
}

#endif
