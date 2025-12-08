
#ifndef URWT_CONFIG_CONFIGURATION_PERSISTER_HPP
#define URWT_CONFIG_CONFIGURATION_PERSISTER_HPP

#include <string>
#include <vector>
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace config {

class ConfigurationPersister {
public:
    ConfigurationPersister();
    ~ConfigurationPersister();

    Result<bool, std::string> saveConfig(const WirelessConfig& config, 
                                         const std::string& path);
    Result<WirelessConfig, std::string> loadConfig(const std::string& path);
    Result<bool, std::string> createBackup(const std::string& path);
    Result<bool, std::string> restoreBackup(const std::string& path, int backupIndex);
    
private:
    Result<bool, std::string> atomicWrite(const std::string& path, 
                                          const std::string& content);
    std::vector<std::string> listBackups(const std::string& path);
    void rotateBackups(const std::string& path, int max_backups);
};

}
}

#endif
