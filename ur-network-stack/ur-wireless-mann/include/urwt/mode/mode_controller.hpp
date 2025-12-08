#ifndef URWT_MODE_MODE_CONTROLLER_HPP
#define URWT_MODE_MODE_CONTROLLER_HPP

#include <memory>
#include <string>
#include <mutex>
#include "urwt/api.hpp"
#include "urwt/mode/sta_mode_manager.hpp"
#include "urwt/mode/ap_mode_manager.hpp"
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/state/wireless_state.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace mode {

using namespace config;
using namespace state;

class ModeController {
public:
    ModeController(std::shared_ptr<WirelessToolsAPI> api,
                  std::shared_ptr<STAModeManager> sta_manager,
                  std::shared_ptr<APModeManager> ap_manager);
    ~ModeController();

    Result<bool, std::string> switchMode(WirelessMode target_mode,
                                        const WirelessConfig& config);
    Result<bool, std::string> switchToSTA(const STAModeConfig& config);
    Result<bool, std::string> switchToAP(const APModeConfig& config);

    Result<bool, std::string> setMode(WirelessMode mode);
    Result<bool, std::string> setMode(WirelessMode mode,
                                      std::optional<config::STAModeConfig> sta_config,
                                      std::optional<config::APModeConfig> ap_config);
    WirelessMode getCurrentMode() const;
    Result<bool, std::string> detectCurrentMode(const std::string& interface);

    Result<bool, std::string> validateModeSwitch(WirelessMode from,
                                                 WirelessMode to);
    bool canSwitchMode() const;

    Result<bool, std::string> stopCurrentMode();
    Result<bool, std::string> bringToNeutralState(const std::string& interface);

private:
    std::shared_ptr<WirelessToolsAPI> api_;
    std::shared_ptr<STAModeManager> sta_manager_;
    std::shared_ptr<APModeManager> ap_manager_;

    mutable std::mutex mutex_;
    WirelessMode current_mode_{WirelessMode::Unknown};

    Result<bool, std::string> cleanupBeforeSwitch(const std::string& interface);
    Result<bool, std::string> validateInterface(const std::string& interface);
};

}
}

#endif