#include "../include/ProfileManager.h"
#include "../include/Utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <map>

namespace OpenWrtNetwork {

ProfileManager::ProfileManager() {
}

ProfileManager::~ProfileManager() {
}

bool ProfileManager::initialize(const std::string& profileDir) {
    profilesDirectory = profileDir;
    activeProfileFile = profileDir + "/active";
    
    // Create profiles directory if it doesn't exist
    if (!Utils::createDirectory(profilesDirectory)) {
        std::cerr << "Failed to create profiles directory: " << profileDir << std::endl;
        return false;
    }
    
    return loadProfiles();
}

bool ProfileManager::createProfile(const ConnectionProfile& profile) {
    if (!validateProfile(profile)) {
        return false;
    }
    
    if (profiles.find(profile.name) != profiles.end()) {
        std::cerr << "Profile already exists: " << profile.name << std::endl;
        return false;
    }
    
    profiles[profile.name] = profile;
    return saveProfile(profile);
}

bool ProfileManager::updateProfile(const std::string& name, const ConnectionProfile& profile) {
    if (profiles.find(name) == profiles.end()) {
        std::cerr << "Profile not found: " << name << std::endl;
        return false;
    }
    
    if (!validateProfile(profile)) {
        return false;
    }
    
    profiles[name] = profile;
    return saveProfile(profile);
}

bool ProfileManager::deleteProfile(const std::string& name) {
    if (profiles.find(name) == profiles.end()) {
        std::cerr << "Profile not found: " << name << std::endl;
        return false;
    }
    
    profiles.erase(name);
    return deleteProfileFile(name);
}

bool ProfileManager::activateProfile(const std::string& name, NetworkConfigAPI& configAPI) {
    if (profiles.find(name) == profiles.end()) {
        std::cerr << "Profile not found: " << name << std::endl;
        return false;
    }
    
    const auto& profile = profiles[name];
    
    // Apply basic settings
    if (!configAPI.setConnectionMode(profile.basicSettings.connectionMode)) {
        std::cerr << "Failed to set connection mode" << std::endl;
        return false;
    }
    
    if (profile.basicSettings.connectionMode == "static") {
        if (!configAPI.setStaticIP(
            profile.basicSettings.ipv4Address,
            profile.basicSettings.subnetMask,
            profile.basicSettings.defaultGateway)) {
            std::cerr << "Failed to set static IP" << std::endl;
            return false;
        }
    }
    
    if (!configAPI.setDnsServers(profile.basicSettings.dnsServers)) {
        std::cerr << "Failed to set DNS servers" << std::endl;
        return false;
    }
    
    // Apply advanced settings
    if (!configAPI.setMtuSize(profile.advancedSettings.mtuSize)) {
        std::cerr << "Failed to set MTU size" << std::endl;
        return false;
    }
    
    if (!configAPI.setHardwareOffload("checksum", profile.advancedSettings.checksumOffload)) {
        std::cerr << "Failed to set checksum offload" << std::endl;
        return false;
    }
    
    if (!configAPI.setHardwareOffload("tso", profile.advancedSettings.tcpSegmentationOffload)) {
        std::cerr << "Failed to set TSO" << std::endl;
        return false;
    }
    
    if (!configAPI.setHardwareOffload("rss", profile.advancedSettings.rssOffload)) {
        std::cerr << "Failed to set RSS" << std::endl;
        return false;
    }
    
    if (!configAPI.setHardwareOffload("lso", profile.advancedSettings.lsoOffload)) {
        std::cerr << "Failed to set LSO" << std::endl;
        return false;
    }
    
    if (!configAPI.setLinkNegotiation(
        profile.advancedSettings.linkNegotiationMode,
        profile.advancedSettings.forcedSpeed,
        profile.advancedSettings.forcedDuplex)) {
        std::cerr << "Failed to set link negotiation" << std::endl;
        return false;
    }
    
    if (!configAPI.setEnergyEfficientEthernet(profile.advancedSettings.energyEfficientEthernet)) {
        std::cerr << "Failed to set EEE" << std::endl;
        return false;
    }
    
    // Update last used timestamp
    profiles[name].lastUsed = std::chrono::system_clock::now();
    saveProfile(profiles[name]);
    
    // Save active profile
    std::ofstream activeFile(activeProfileFile);
    if (activeFile.is_open()) {
        activeFile << name;
        activeFile.close();
    }
    
    // Restart interface to apply changes
    configAPI.restartInterface("eth0");
    
    return true;
}

std::vector<ConnectionProfile> ProfileManager::getAllProfiles() {
    std::vector<ConnectionProfile> profileList;
    for (const auto& pair : profiles) {
        profileList.push_back(pair.second);
    }
    return profileList;
}

ConnectionProfile ProfileManager::getProfile(const std::string& name) {
    auto it = profiles.find(name);
    if (it != profiles.end()) {
        return it->second;
    }
    return ConnectionProfile{};
}

std::string ProfileManager::getActiveProfile() {
    if (!Utils::fileExists(activeProfileFile)) {
        return "";
    }
    
    std::ifstream activeFile(activeProfileFile);
    if (activeFile.is_open()) {
        std::string activeProfile;
        std::getline(activeFile, activeProfile);
        activeFile.close();
        return Utils::trim(activeProfile);
    }
    
    return "";
}

bool ProfileManager::setAutoSwitchConditions(const std::string& profileName, const std::vector<std::string>& conditions) {
    if (profiles.find(profileName) == profiles.end()) {
        return false;
    }
    
    profiles[profileName].autoApplyConditions = conditions;
    return saveProfile(profiles[profileName]);
}

std::vector<std::string> ProfileManager::getAutoSwitchConditions(const std::string& profileName) {
    auto it = profiles.find(profileName);
    if (it != profiles.end()) {
        return it->second.autoApplyConditions;
    }
    return std::vector<std::string>();
}

bool ProfileManager::evaluateAutoSwitchConditions(const std::string& profileName) {
    auto it = profiles.find(profileName);
    if (it == profiles.end()) {
        return false;
    }
    
    for (const auto& condition : it->second.autoApplyConditions) {
        if (evaluateCondition(condition)) {
            return true;
        }
    }
    
    return false;
}

bool ProfileManager::validateProfile(const ConnectionProfile& profile) {
    auto errors = getProfileErrors(profile);
    return errors.empty();
}

std::vector<std::string> ProfileManager::getProfileErrors(const ConnectionProfile& profile) {
    std::vector<std::string> errors;
    
    if (profile.name.empty()) {
        errors.push_back("Profile name cannot be empty");
    }
    
    if (profile.basicSettings.connectionMode != "dhcp" && profile.basicSettings.connectionMode != "manual") {
        errors.push_back("Connection mode must be 'dhcp' or 'manual'");
    }
    
    if (profile.basicSettings.connectionMode == "manual") {
        if (!Utils::isValidIPv4(profile.basicSettings.ipv4Address)) {
            errors.push_back("Invalid IPv4 address");
        }
        
        if (!Utils::isValidIPv4(profile.basicSettings.subnetMask)) {
            errors.push_back("Invalid subnet mask");
        }
        
        if (!Utils::isValidIPv4(profile.basicSettings.defaultGateway)) {
            errors.push_back("Invalid gateway address");
        }
    }
    
    for (const auto& dns : profile.basicSettings.dnsServers) {
        if (!Utils::isValidIPv4(dns)) {
            errors.push_back("Invalid DNS server: " + dns);
        }
    }
    
    if (profile.advancedSettings.mtuSize < 576 || profile.advancedSettings.mtuSize > 9000) {
        errors.push_back("MTU size must be between 576 and 9000");
    }
    
    return errors;
}

bool ProfileManager::loadProfiles() {
    if (!Utils::fileExists(profilesDirectory)) {
        return true;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(profilesDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".profile") {
            std::string content = Utils::readFile(entry.path().string());
            if (!content.empty()) {
                ConnectionProfile profile = deserializeProfile(content);
                if (!profile.name.empty()) {
                    profiles[profile.name] = profile;
                }
            }
        }
    }
    
    return true;
}

bool ProfileManager::saveProfile(const ConnectionProfile& profile) {
    std::string content = serializeProfile(profile);
    return Utils::writeFile(profileFilePath(profile.name), content);
}

bool ProfileManager::deleteProfileFile(const std::string& name) {
    std::string path = profileFilePath(name);
    if (Utils::fileExists(path)) {
        return std::filesystem::remove(path);
    }
    return true;
}

std::string ProfileManager::profileFilePath(const std::string& name) {
    return profilesDirectory + "/" + name + ".profile";
}

std::string ProfileManager::serializeProfile(const ConnectionProfile& profile) {
    std::ostringstream oss;
    
    oss << "name=" << profile.name << "\n";
    oss << "description=" << profile.description << "\n";
    
    // Basic settings
    oss << "[basic]\n";
    oss << "connectionMode=" << profile.basicSettings.connectionMode << "\n";
    oss << "ipv4Address=" << profile.basicSettings.ipv4Address << "\n";
    oss << "subnetMask=" << profile.basicSettings.subnetMask << "\n";
    oss << "defaultGateway=" << profile.basicSettings.defaultGateway << "\n";
    oss << "dhcpLeaseTimeRemaining=" << profile.basicSettings.dhcpLeaseTimeRemaining << "\n";
    oss << "dnsServers=" << Utils::join(profile.basicSettings.dnsServers, ",") << "\n";
    oss << "searchDomains=" << Utils::join(profile.basicSettings.searchDomains, ",") << "\n";
    oss << "interfacePriority=" << profile.basicSettings.interfacePriority << "\n";
    
    // Advanced settings
    oss << "[advanced]\n";
    oss << "mtuSize=" << profile.advancedSettings.mtuSize << "\n";
    oss << "checksumOffload=" << (profile.advancedSettings.checksumOffload ? "1" : "0") << "\n";
    oss << "tcpSegmentationOffload=" << (profile.advancedSettings.tcpSegmentationOffload ? "1" : "0") << "\n";
    oss << "rssOffload=" << (profile.advancedSettings.rssOffload ? "1" : "0") << "\n";
    oss << "lsoOffload=" << (profile.advancedSettings.lsoOffload ? "1" : "0") << "\n";
    oss << "linkNegotiationMode=" << profile.advancedSettings.linkNegotiationMode << "\n";
    oss << "forcedSpeed=" << profile.advancedSettings.forcedSpeed << "\n";
    oss << "forcedDuplex=" << profile.advancedSettings.forcedDuplex << "\n";
    oss << "energyEfficientEthernet=" << (profile.advancedSettings.energyEfficientEthernet ? "1" : "0") << "\n";
    
    // Metadata
    oss << "[metadata]\n";
    auto timeT = std::chrono::system_clock::to_time_t(profile.lastUsed);
    oss << "lastUsed=" << timeT << "\n";
    oss << "gatewayInfo=" << profile.gatewayInfo << "\n";
    oss << "autoApplyConditions=" << Utils::join(profile.autoApplyConditions, ";") << "\n";
    
    return oss.str();
}

ConnectionProfile ProfileManager::deserializeProfile(const std::string& data) {
    ConnectionProfile profile;
    std::istringstream iss(data);
    std::string line;
    std::string currentSection;
    
    while (std::getline(iss, line)) {
        line = Utils::trim(line);
        
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        size_t equalPos = line.find('=');
        if (equalPos != std::string::npos) {
            std::string key = Utils::trim(line.substr(0, equalPos));
            std::string value = Utils::trim(line.substr(equalPos + 1));
            
            if (currentSection.empty()) {
                if (key == "name") profile.name = value;
                else if (key == "description") profile.description = value;
            } else if (currentSection == "basic") {
                if (key == "connectionMode") profile.basicSettings.connectionMode = value;
                else if (key == "ipv4Address") profile.basicSettings.ipv4Address = value;
                else if (key == "subnetMask") profile.basicSettings.subnetMask = value;
                else if (key == "defaultGateway") profile.basicSettings.defaultGateway = value;
                else if (key == "dhcpLeaseTimeRemaining") profile.basicSettings.dhcpLeaseTimeRemaining = std::stoi(value);
                else if (key == "dnsServers") profile.basicSettings.dnsServers = Utils::split(value, ',');
                else if (key == "searchDomains") profile.basicSettings.searchDomains = Utils::split(value, ',');
                else if (key == "interfacePriority") profile.basicSettings.interfacePriority = std::stoi(value);
            } else if (currentSection == "advanced") {
                if (key == "mtuSize") profile.advancedSettings.mtuSize = std::stoi(value);
                else if (key == "checksumOffload") profile.advancedSettings.checksumOffload = (value == "1");
                else if (key == "tcpSegmentationOffload") profile.advancedSettings.tcpSegmentationOffload = (value == "1");
                else if (key == "rssOffload") profile.advancedSettings.rssOffload = (value == "1");
                else if (key == "lsoOffload") profile.advancedSettings.lsoOffload = (value == "1");
                else if (key == "linkNegotiationMode") profile.advancedSettings.linkNegotiationMode = value;
                else if (key == "forcedSpeed") profile.advancedSettings.forcedSpeed = std::stoi(value);
                else if (key == "forcedDuplex") profile.advancedSettings.forcedDuplex = value;
                else if (key == "energyEfficientEthernet") profile.advancedSettings.energyEfficientEthernet = (value == "1");
            } else if (currentSection == "metadata") {
                if (key == "lastUsed") {
                    time_t timeT = std::stoll(value);
                    profile.lastUsed = std::chrono::system_clock::from_time_t(timeT);
                } else if (key == "gatewayInfo") profile.gatewayInfo = value;
                else if (key == "autoApplyConditions") profile.autoApplyConditions = Utils::split(value, ';');
            }
        }
    }
    
    return profile;
}

bool ProfileManager::evaluateCondition(const std::string& condition) {
    if (Utils::startsWith(condition, "port:")) {
        return checkPortCondition(condition.substr(5));
    } else if (Utils::startsWith(condition, "time:")) {
        return checkTimeCondition(condition.substr(5));
    } else if (Utils::startsWith(condition, "network:")) {
        return checkNetworkCondition(condition.substr(8));
    }
    
    return false;
}

bool ProfileManager::checkPortCondition(const std::string& port) {
    // Check if specific port is connected
    std::string output = Utils::executeCommand("ethtool eth0");
    return output.find("Link detected: yes") != std::string::npos;
}

bool ProfileManager::checkTimeCondition(const std::string& timeCondition) {
    // Simple time-based condition evaluation
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&timeT);
    
    // Parse time condition (e.g., "09:00-17:00")
    size_t dashPos = timeCondition.find('-');
    if (dashPos != std::string::npos) {
        std::string startTime = timeCondition.substr(0, dashPos);
        std::string endTime = timeCondition.substr(dashPos + 1);
        
        int currentHour = tm.tm_hour;
        int currentMin = tm.tm_min;
        int currentTime = currentHour * 60 + currentMin;
        
        size_t startColon = startTime.find(':');
        int startHour = std::stoi(startTime.substr(0, startColon));
        int startMin = std::stoi(startTime.substr(startColon + 1));
        int startTimeMin = startHour * 60 + startMin;
        
        size_t endColon = endTime.find(':');
        int endHour = std::stoi(endTime.substr(0, endColon));
        int endMin = std::stoi(endTime.substr(endColon + 1));
        int endTimeMin = endHour * 60 + endMin;
        
        return currentTime >= startTimeMin && currentTime <= endTimeMin;
    }
    
    return false;
}

bool ProfileManager::checkNetworkCondition(const std::string& networkCondition) {
    // Check network-specific conditions
    if (networkCondition == "internet") {
        std::string result = Utils::executeCommand("ping -c 1 8.8.8.8");
        return result.find("1 received") != std::string::npos;
    }
    
    return false;
}

} // namespace OpenWrtNetwork
