#include "urwt/network/saved_network_manager.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace urwt {
namespace network {

// TODO: Integrate with WirelessConfigManager for persistence
SavedNetworkManager::SavedNetworkManager() {
}

SavedNetworkManager::~SavedNetworkManager() {
}

Result<bool, std::string> SavedNetworkManager::addNetwork(const config::NetworkProfile& profile) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!validateProfile(profile)) {
        return Result<bool, std::string>::error("Invalid network profile");
    }

    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&profile](const config::NetworkProfile& p) { return p.ssid == profile.ssid; });

    if (it != networks_.end()) {
        return Result<bool, std::string>::error("Network with SSID '" + profile.ssid + "' already exists");
    }

    networks_.push_back(profile);
    sortNetworks();

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> SavedNetworkManager::removeNetwork(const std::string& ssid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&ssid](const config::NetworkProfile& p) { return p.ssid == ssid; });

    if (it == networks_.end()) {
        return Result<bool, std::string>::error("Network with SSID '" + ssid + "' not found");
    }

    networks_.erase(it);

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> SavedNetworkManager::updateNetwork(const config::NetworkProfile& profile) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!validateProfile(profile)) {
        return Result<bool, std::string>::error("Invalid network profile");
    }

    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&profile](const config::NetworkProfile& p) { return p.ssid == profile.ssid; });

    if (it == networks_.end()) {
        return Result<bool, std::string>::error("Network with SSID '" + profile.ssid + "' not found");
    }

    *it = profile;
    sortNetworks();

    return Result<bool, std::string>::ok(true);
}

std::vector<config::NetworkProfile> SavedNetworkManager::getAllNetworks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return networks_;
}

std::vector<config::NetworkProfile> SavedNetworkManager::getAutoConnectNetworks() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<config::NetworkProfile> auto_connect;
    std::copy_if(networks_.begin(), networks_.end(), std::back_inserter(auto_connect),
        [](const config::NetworkProfile& p) { return p.auto_connect; });

    return auto_connect;
}

std::optional<config::NetworkProfile> SavedNetworkManager::getNetwork(const std::string& ssid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&ssid](const config::NetworkProfile& p) { return p.ssid == ssid; });

    if (it == networks_.end()) {
        return std::nullopt;
    }

    return *it;
}

bool SavedNetworkManager::hasNetwork(const std::string& ssid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    return std::any_of(networks_.begin(), networks_.end(),
        [&ssid](const config::NetworkProfile& p) { return p.ssid == ssid; });
}

size_t SavedNetworkManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return networks_.size();
}

Result<bool, std::string> SavedNetworkManager::setPriority(const std::string& ssid, int priority) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (priority < 0 || priority > 10) {
        return Result<bool, std::string>::error("Priority must be between 0 and 10");
    }

    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&ssid](const config::NetworkProfile& p) { return p.ssid == ssid; });

    if (it == networks_.end()) {
        return Result<bool, std::string>::error("Network with SSID '" + ssid + "' not found");
    }

    it->priority = priority;
    sortNetworks();

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> SavedNetworkManager::moveUp(const std::string& ssid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&ssid](const config::NetworkProfile& p) { return p.ssid == ssid; });

    if (it == networks_.end()) {
        return Result<bool, std::string>::error("Network with SSID '" + ssid + "' not found");
    }

    if (it->priority >= 10) {
        return Result<bool, std::string>::error("Network already at maximum priority");
    }

    it->priority++;
    sortNetworks();

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> SavedNetworkManager::moveDown(const std::string& ssid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&ssid](const config::NetworkProfile& p) { return p.ssid == ssid; });

    if (it == networks_.end()) {
        return Result<bool, std::string>::error("Network with SSID '" + ssid + "' not found");
    }

    if (it->priority <= 0) {
        return Result<bool, std::string>::error("Network already at minimum priority");
    }

    it->priority--;
    sortNetworks();

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> SavedNetworkManager::setAutoConnect(const std::string& ssid, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(networks_.begin(), networks_.end(),
        [&ssid](const config::NetworkProfile& p) { return p.ssid == ssid; });

    if (it == networks_.end()) {
        return Result<bool, std::string>::error("Network with SSID '" + ssid + "' not found");
    }

    it->auto_connect = enabled;

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> SavedNetworkManager::saveToFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        nlohmann::json j;
        j["networks"] = networks_;

        std::ofstream file(filepath);
        if (!file.is_open()) {
            return Result<bool, std::string>::error("Failed to open file: " + filepath);
        }

        file << j.dump(2);
        file.close();

        return Result<bool, std::string>::ok(true);
    } catch (const std::exception& e) {
        return Result<bool, std::string>::error("Failed to save networks: " + std::string(e.what()));
    }
}

Result<bool, std::string> SavedNetworkManager::loadFromFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return Result<bool, std::string>::error("Failed to open file: " + filepath);
        }

        nlohmann::json j;
        file >> j;
        file.close();

        if (!j.contains("networks")) {
            return Result<bool, std::string>::error("Invalid file format: missing 'networks' field");
        }

        networks_ = j["networks"].get<std::vector<config::NetworkProfile>>();
        sortNetworks();
        deduplicateNetworks();

        return Result<bool, std::string>::ok(true);
    } catch (const std::exception& e) {
        return Result<bool, std::string>::error("Failed to load networks: " + std::string(e.what()));
    }
}

Result<bool, std::string> SavedNetworkManager::importNetworks(const std::vector<config::NetworkProfile>& profiles) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& profile : profiles) {
        if (!validateProfile(profile)) {
            continue;
        }

        auto it = std::find_if(networks_.begin(), networks_.end(),
            [&profile](const config::NetworkProfile& p) { return p.ssid == profile.ssid; });

        if (it == networks_.end()) {
            networks_.push_back(profile);
        }
    }

    sortNetworks();
    deduplicateNetworks();

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> SavedNetworkManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    networks_.clear();
    return Result<bool, std::string>::ok(true);
}

std::vector<config::NetworkProfile> SavedNetworkManager::findBySSID(const std::string& pattern) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<config::NetworkProfile> matches;
    std::copy_if(networks_.begin(), networks_.end(), std::back_inserter(matches),
        [&pattern](const config::NetworkProfile& p) { 
            return p.ssid.find(pattern) != std::string::npos; 
        });

    return matches;
}

std::vector<config::NetworkProfile> SavedNetworkManager::findBySecurity(config::SecurityType security) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<config::NetworkProfile> matches;
    std::copy_if(networks_.begin(), networks_.end(), std::back_inserter(matches),
        [security](const config::NetworkProfile& p) { return p.security == security; });

    return matches;
}

std::vector<config::NetworkProfile> SavedNetworkManager::getSortedByPriority() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<config::NetworkProfile> sorted = networks_;
    std::sort(sorted.begin(), sorted.end(),
        [](const config::NetworkProfile& a, const config::NetworkProfile& b) {
            return a.priority > b.priority;
        });

    return sorted;
}

void SavedNetworkManager::sortNetworks() {
    std::sort(networks_.begin(), networks_.end(),
        [](const config::NetworkProfile& a, const config::NetworkProfile& b) {
            return a.priority > b.priority;
        });
}

bool SavedNetworkManager::validateProfile(const config::NetworkProfile& profile) const {
    if (profile.ssid.empty()) {
        return false;
    }

    if (profile.priority < 0 || profile.priority > 10) {
        return false;
    }

    if (profile.security != config::SecurityType::Open && profile.password.empty()) {
        return false;
    }

    if (profile.security == config::SecurityType::WPA2Enterprise) {
        if (!profile.identity.has_value() || profile.identity->empty()) {
            return false;
        }
    }

    return true;
}

void SavedNetworkManager::deduplicateNetworks() {
    auto it = std::unique(networks_.begin(), networks_.end(),
        [](const config::NetworkProfile& a, const config::NetworkProfile& b) {
            return a.ssid == b.ssid;
        });
    networks_.erase(it, networks_.end());
}

} // namespace network
} // namespace urwt