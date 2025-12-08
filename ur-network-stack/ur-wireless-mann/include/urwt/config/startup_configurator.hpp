
#ifndef URWT_CONFIG_STARTUP_CONFIGURATOR_HPP
#define URWT_CONFIG_STARTUP_CONFIGURATOR_HPP

#include <memory>
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/api.hpp"
#include "urwt/mode/mode_controller.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace config {

class WirelessConfigManager;

class StartupConfigurator {
public:
    StartupConfigurator(std::shared_ptr<WirelessToolsAPI> api,
                       std::shared_ptr<mode::ModeController> modeController,
                       std::shared_ptr<WirelessConfigManager> wirelessConfigManager);
    ~StartupConfigurator();

    Result<bool, std::string> applyConfiguration(const WirelessConfig& config);
    
private:
    std::shared_ptr<WirelessToolsAPI> api_;
    std::shared_ptr<mode::ModeController> mode_controller_;
    std::shared_ptr<WirelessConfigManager> wireless_config_manager_;

    Result<bool, std::string> applyWiFiState(bool enabled);
    Result<bool, std::string> applyMode(WirelessMode mode);
    Result<bool, std::string> applySTAConfiguration(const STAModeConfig& config);
    Result<bool, std::string> applyAPConfiguration(const APModeConfig& config);
    Result<bool, std::string> configureNetworks(const std::vector<NetworkProfile>& networks);
};

}
}

#endif
