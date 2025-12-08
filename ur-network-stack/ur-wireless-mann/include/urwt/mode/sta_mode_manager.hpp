#ifndef URWT_MODE_STA_MODE_MANAGER_HPP
#define URWT_MODE_STA_MODE_MANAGER_HPP

#include <memory>
#include <string>
#include "urwt/api.hpp"
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace mode {

using namespace config;

class STAModeManager {
public:
    explicit STAModeManager(std::shared_ptr<WirelessToolsAPI> api);
    ~STAModeManager();

    Result<bool, std::string> enableSTAMode(const STAModeConfig& config);
    Result<bool, std::string> disableSTAMode(const std::string& interface);

    Result<bool, std::string> configureInterface(const STAModeConfig& config);
    Result<bool, std::string> setStaticIP(const StaticIPConfig& ip_config,
                                         const std::string& interface);
    Result<bool, std::string> enableDHCP(const std::string& interface);

    Result<bool, std::string> setPowerSave(bool enabled, 
                                          const std::string& interface);
    Result<bool, std::string> setRegulatoryDomain(const std::string& domain);

    bool isSTAModeActive(const std::string& interface) const;

private:
    std::shared_ptr<WirelessToolsAPI> api_;

    Result<std::string, std::string> executeCommand(const std::string& cmd);
};

}
}

#endif
