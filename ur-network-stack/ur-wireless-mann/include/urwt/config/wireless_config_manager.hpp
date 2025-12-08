#ifndef URWT_CONFIG_WIRELESS_CONFIG_MANAGER_HPP
#define URWT_CONFIG_WIRELESS_CONFIG_MANAGER_HPP

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <mutex>
#include <json.hpp>
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/config/configuration_persister.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace config {

using json = nlohmann::json;

class WirelessConfigManager {
public:
    WirelessConfigManager();
    ~WirelessConfigManager();

    Result<bool, std::string> loadFromFile(const std::string& configPath);
    Result<bool, std::string> loadFromJSON(const json& configJson);
    Result<bool, std::string> saveToFile(const std::string& configPath);

    WirelessConfig getConfig() const;
    bool isWirelessEnabled() const;
    WirelessMode getMode() const;
    AutomationConfig getAutomationConfig() const;
    STAModeConfig getSTAConfig() const;
    APModeConfig getAPConfig() const;
    MonitoringConfig getMonitoringConfig() const;

    Result<bool, std::string> setWirelessEnabled(bool enabled);
    Result<bool, std::string> setMode(WirelessMode mode);
    Result<bool, std::string> setAutomationEnabled(bool enabled);
    Result<bool, std::string> updateSTAConfig(const STAModeConfig& config);
    Result<bool, std::string> updateAPConfig(const APModeConfig& config);

    Result<bool, std::string> addSavedNetwork(const NetworkProfile& profile);
    Result<bool, std::string> removeSavedNetwork(const std::string& ssid);
    Result<bool, std::string> updateSavedNetwork(const NetworkProfile& profile);
    std::vector<NetworkProfile> getSavedNetworks() const;
    std::optional<NetworkProfile> getSavedNetwork(const std::string& ssid) const;

    Result<bool, std::string> persist();
    Result<bool, std::string> reload();
    Result<bool, std::string> createBackup();
    Result<bool, std::string> restoreFromBackup(int backupIndex);

    Result<bool, std::string> validate() const;
    Result<bool, std::string> validateNetworkProfile(const NetworkProfile& profile) const;

    Result<bool, std::string> setConfigFilePath(const std::string& path);
    std::string getConfigFilePath() const;
    Result<bool, std::string> enableAutoPersist(bool enable);

private:
    mutable std::mutex mutex_;
    WirelessConfig config_;
    std::string config_file_path_;
    bool auto_persist_{false};
    std::shared_ptr<ConfigurationPersister> persister_;

    Result<bool, std::string> verifyAndCorrectInterfaces();
    Result<WirelessConfig, std::string> parseWirelessConfig(const json& j);
    Result<AutomationConfig, std::string> parseAutomationConfig(const json& j);
    Result<STAModeConfig, std::string> parseSTAConfig(const json& j);
    Result<APModeConfig, std::string> parseAPConfig(const json& j);
    Result<MonitoringConfig, std::string> parseMonitoringConfig(const json& j);
    Result<NetworkProfile, std::string> parseNetworkProfile(const json& j);

    json serializeConfig() const;
    json serializeNetworkProfile(const NetworkProfile& profile) const;

    std::string encryptPassword(const std::string& password) const;
    std::string decryptPassword(const std::string& encrypted) const;

    struct FileContent { std::string content; };
    Result<FileContent, std::string> readFile(const std::string& path) const;
    Result<bool, std::string> writeFile(const std::string& path, 
                                        const std::string& content) const;
};

}
}

#endif
