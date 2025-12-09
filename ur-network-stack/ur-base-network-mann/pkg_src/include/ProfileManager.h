#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "NetworkConfigAPI.h"

namespace OpenWrtNetwork {

class ProfileManager {
public:
    ProfileManager();
    ~ProfileManager();

    bool initialize(const std::string& profileDir = "/etc/Ultima-Config/ur-base-network-mann/network-profiles");
    
    // Profile operations
    bool createProfile(const ConnectionProfile& profile);
    bool updateProfile(const std::string& name, const ConnectionProfile& profile);
    bool deleteProfile(const std::string& name);
    bool activateProfile(const std::string& name, NetworkConfigAPI& configAPI);
    
    // Profile queries
    std::vector<ConnectionProfile> getAllProfiles();
    ConnectionProfile getProfile(const std::string& name);
    std::string getActiveProfile();
    
    // Auto-switch conditions
    bool setAutoSwitchConditions(const std::string& profileName, const std::vector<std::string>& conditions);
    std::vector<std::string> getAutoSwitchConditions(const std::string& profileName);
    bool evaluateAutoSwitchConditions(const std::string& profileName);
    
    // Profile validation
    bool validateProfile(const ConnectionProfile& profile);
    std::vector<std::string> getProfileErrors(const ConnectionProfile& profile);

private:
    std::string profilesDirectory;
    std::string activeProfileFile;
    std::map<std::string, ConnectionProfile> profiles;
    
    bool loadProfiles();
    bool saveProfile(const ConnectionProfile& profile);
    bool deleteProfileFile(const std::string& name);
    std::string profileFilePath(const std::string& name);
    
    // Serialization
    std::string serializeProfile(const ConnectionProfile& profile);
    ConnectionProfile deserializeProfile(const std::string& data);
    
    // Condition evaluation
    bool evaluateCondition(const std::string& condition);
    bool checkPortCondition(const std::string& port);
    bool checkTimeCondition(const std::string& timeCondition);
    bool checkNetworkCondition(const std::string& networkCondition);
};

} // namespace OpenWrtNetwork
