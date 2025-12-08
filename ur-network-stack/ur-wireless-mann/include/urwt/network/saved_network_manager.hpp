#ifndef URWT_NETWORK_SAVED_NETWORK_MANAGER_HPP
#define URWT_NETWORK_SAVED_NETWORK_MANAGER_HPP

#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <memory>
#include <json.hpp>
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace network {

class SavedNetworkManager {
public:
    SavedNetworkManager();
    ~SavedNetworkManager();

    Result<bool, std::string> addNetwork(const config::NetworkProfile& profile);
    Result<bool, std::string> removeNetwork(const std::string& ssid);
    Result<bool, std::string> updateNetwork(const config::NetworkProfile& profile);

    std::vector<config::NetworkProfile> getAllNetworks() const;
    std::vector<config::NetworkProfile> getAutoConnectNetworks() const;
    std::optional<config::NetworkProfile> getNetwork(const std::string& ssid) const;

    bool hasNetwork(const std::string& ssid) const;
    size_t count() const;

    Result<bool, std::string> setPriority(const std::string& ssid, int priority);
    Result<bool, std::string> moveUp(const std::string& ssid);
    Result<bool, std::string> moveDown(const std::string& ssid);

    Result<bool, std::string> setAutoConnect(const std::string& ssid, bool enabled);

    Result<bool, std::string> saveToFile(const std::string& filepath);
    Result<bool, std::string> loadFromFile(const std::string& filepath);

    Result<bool, std::string> importNetworks(const std::vector<config::NetworkProfile>& profiles);
    Result<bool, std::string> clear();

    std::vector<config::NetworkProfile> findBySSID(const std::string& pattern) const;
    std::vector<config::NetworkProfile> findBySecurity(config::SecurityType security) const;
    std::vector<config::NetworkProfile> getSortedByPriority() const;

private:
    mutable std::mutex mutex_;
    std::vector<config::NetworkProfile> networks_;

    void sortNetworks();
    bool validateProfile(const config::NetworkProfile& profile) const;
    void deduplicateNetworks();
};

} // namespace network
} // namespace urwt

#endif // URWT_NETWORK_SAVED_NETWORK_MANAGER_HPP
