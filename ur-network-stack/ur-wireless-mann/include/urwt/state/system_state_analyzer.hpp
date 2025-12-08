#ifndef URWT_STATE_SYSTEM_STATE_ANALYZER_HPP
#define URWT_STATE_SYSTEM_STATE_ANALYZER_HPP

#include <memory>
#include <string>
#include <vector>
#include "urwt/state/wireless_state.hpp"
#include "urwt/state/state_transition.hpp"
#include "urwt/utils/result.hpp"
#include "urwt/api.hpp"

namespace urwt {
namespace state {

class SystemStateAnalyzer {
public:
    explicit SystemStateAnalyzer(std::shared_ptr<WirelessToolsAPI> api);
    ~SystemStateAnalyzer();

    Result<SystemWirelessState, std::string> analyzeCurrentState();
    Result<SystemWirelessState, std::string> analyzeInterface(const std::string& iface);

    Result<WirelessHardwareState, std::string> detectHardwareState();
    Result<InterfaceState, std::string> detectInterfaceState(const std::string& iface);
    Result<ConnectionState, std::string> detectConnectionState(const std::string& iface);
    Result<WirelessMode, std::string> detectCurrentMode(const std::string& iface);

    Result<CurrentConnection, std::string> getCurrentConnection(const std::string& iface);
    Result<APModeState, std::string> getAPState(const std::string& iface);

    StateTransitionPlan compareStates(const SystemWirelessState& current,
                                     const config::WirelessConfig& desired);
    bool requiresStateChange(const SystemWirelessState& current,
                           const config::WirelessConfig& desired);

    Result<bool, std::string> validateStateTransition(
        const SystemWirelessState& from,
        const SystemWirelessState& to);

private:
    std::shared_ptr<WirelessToolsAPI> api_;

    Result<std::string, std::string> executeCommand(const std::string& cmd);
    Result<bool, std::string> isRfKilled();
    Result<bool, std::string> isInterfaceInAPMode(const std::string& iface);
    Result<std::vector<std::string>, std::string> getConnectedAPClients(
        const std::string& iface);

    std::optional<CurrentConnection> parseConnectionInfo(const std::string& output);
    std::optional<APModeState> parseAPStatus(const std::string& output);
};

}
}

#endif
