#include "VlanCollector.h"
#include <sstream>
#include <regex>
#include <fstream>
#include <iomanip>

json VlanCollector::collectDataJson() {
    json result;
    result["type"] = "vlan";
    result["timestamp"] = getCurrentTimestamp();
    
    try {
        std::vector<VlanInfo> vlans;
        
        if (CommandExecutor::commandExists("vconfig")) {
            auto configVlans = parseVlanConfig();
            vlans.insert(vlans.end(), configVlans.begin(), configVlans.end());
        }
        
        auto ipLinkVlans = parseVlanFromIpLink();
        vlans.insert(vlans.end(), ipLinkVlans.begin(), ipLinkVlans.end());
        
        auto procVlans = parseVlanFromProc();
        vlans.insert(vlans.end(), procVlans.begin(), procVlans.end());
        
        result["data"] = formatVlanDataJson(vlans);
        result["success"] = true;
        
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Error collecting VLAN data: " + std::string(e.what());
        result["data"] = json::array();
    }
    
    return result;
}

std::vector<NetworkData> VlanCollector::collectData() {
    std::vector<NetworkData> results;
    
    try {
        std::vector<VlanInfo> vlans;
        
        if (CommandExecutor::commandExists("vconfig")) {
            auto configVlans = parseVlanConfig();
            vlans.insert(vlans.end(), configVlans.begin(), configVlans.end());
        }
        
        auto ipLinkVlans = parseVlanFromIpLink();
        vlans.insert(vlans.end(), ipLinkVlans.begin(), ipLinkVlans.end());
        
        auto procVlans = parseVlanFromProc();
        vlans.insert(vlans.end(), procVlans.begin(), procVlans.end());
        
        NetworkData data;
        data.type = "vlan";
        data.data = formatVlanData(vlans);
        data.timestamp = getCurrentTimestamp();
        results.push_back(data);
        
    } catch (const std::exception& e) {
        NetworkData errorData;
        errorData.type = "vlan_error";
        errorData.data = "Error collecting VLAN data: " + std::string(e.what());
        errorData.timestamp = getCurrentTimestamp();
        results.push_back(errorData);
    }
    
    return results;
}

std::vector<VlanInfo> VlanCollector::parseVlanConfig() {
    std::vector<VlanInfo> vlans;
    
    try {
        std::string output = CommandExecutor::executeCommand("vconfig show");
        std::istringstream iss(output);
        std::string line;
        
        std::regex vlanRegex(R"(VLAN\s+(\d+):\s+Name:\s+(\S+),\s+Interfaces?:\s+(.+))");
        
        while (std::getline(iss, line)) {
            std::smatch match;
            if (std::regex_match(line, match, vlanRegex)) {
                VlanInfo vlan;
                vlan.vlanId = std::stoi(match[1].str());
                vlan.name = match[2].str();
                vlan.interface = match[3].str();
                vlan.status = "active";
                
                try {
                    std::string ipOutput = CommandExecutor::executeCommand("ip addr show " + vlan.name + " 2>/dev/null");
                    std::regex ipRegex(R"(inet\s+(\d+\.\d+\.\d+\.\d+)/(\d+))");
                    std::smatch ipMatch;
                    if (std::regex_search(ipOutput, ipMatch, ipRegex)) {
                        vlan.ipAddress = ipMatch[1].str();
                        vlan.netmask = ipMatch[2].str();
                    }
                } catch (...) {
                    vlan.ipAddress = "N/A";
                    vlan.netmask = "N/A";
                }
                
                vlans.push_back(vlan);
            }
        }
    } catch (...) {
        // vconfig failed, continue with other methods
    }
    
    return vlans;
}

std::vector<VlanInfo> VlanCollector::parseVlanFromProc() {
    std::vector<VlanInfo> vlans;
    
    std::ifstream procFile("/proc/net/vlan/config");
    if (!procFile.is_open()) {
        return vlans;
    }
    
    std::string line;
    while (std::getline(procFile, line)) {
        if (line.find("VLAN") == 0 || line.empty()) {
            continue;
        }
        
        std::istringstream iss(line);
        VlanInfo vlan;
        std::string dummy;
        
        if (iss >> vlan.name >> vlan.vlanId >> dummy >> vlan.interface) {
            vlan.status = "active";
            
            try {
                std::string ipOutput = CommandExecutor::executeCommand("ip addr show " + vlan.name + " 2>/dev/null");
                std::regex ipRegex(R"(inet\s+(\d+\.\d+\.\d+\.\d+)/(\d+))");
                std::smatch ipMatch;
                if (std::regex_search(ipOutput, ipMatch, ipRegex)) {
                    vlan.ipAddress = ipMatch[1].str();
                    vlan.netmask = ipMatch[2].str();
                }
            } catch (...) {
                vlan.ipAddress = "N/A";
                vlan.netmask = "N/A";
            }
            
            vlans.push_back(vlan);
        }
    }
    
    return vlans;
}

std::vector<VlanInfo> VlanCollector::parseVlanFromIpLink() {
    std::vector<VlanInfo> vlans;
    
    try {
        std::string output = CommandExecutor::executeCommand("ip -d link show type vlan");
        std::istringstream iss(output);
        std::string line;
        std::regex vlanRegex(R"(\d+:\s+(\S+):\s+.*vlan\s+id\s+(\d+))");
        
        while (std::getline(iss, line)) {
            std::smatch match;
            if (std::regex_search(line, match, vlanRegex)) {
                VlanInfo vlan;
                vlan.name = match[1].str();
                vlan.vlanId = std::stoi(match[2].str());
                vlan.status = "active";
                
                try {
                    std::string ipOutput = CommandExecutor::executeCommand("ip addr show " + vlan.name + " 2>/dev/null");
                    std::regex ipRegex(R"(inet\s+(\d+\.\d+\.\d+\.\d+)/(\d+))");
                    std::smatch ipMatch;
                    if (std::regex_search(ipOutput, ipMatch, ipRegex)) {
                        vlan.ipAddress = ipMatch[1].str();
                        vlan.netmask = ipMatch[2].str();
                    }
                    
                    std::regex parentRegex(R"(link/\S+\s+(\S+))");
                    if (std::regex_search(ipOutput, ipMatch, parentRegex)) {
                        vlan.interface = ipMatch[1].str();
                    }
                } catch (...) {
                    vlan.ipAddress = "N/A";
                    vlan.netmask = "N/A";
                    vlan.interface = "N/A";
                }
                
                vlans.push_back(vlan);
            }
        }
    } catch (...) {
        // ip command failed
    }
    
    return vlans;
}

std::string VlanCollector::formatVlanData(const std::vector<VlanInfo>& vlans) {
    std::ostringstream oss;
    oss << "VLAN Configuration:\n";
    oss << "==================\n";
    
    if (vlans.empty()) {
        oss << "No VLANs configured or unable to access VLAN information.\n";
        return oss.str();
    }
    
    oss << "VLAN ID | Name        | Interface   | IP Address    | Netmask | Status\n";
    oss << "--------|-------------|-------------|---------------|---------|--------\n";
    
    for (const auto& vlan : vlans) {
        oss << std::setw(7) << vlan.vlanId << " | "
            << std::setw(11) << vlan.name << " | "
            << std::setw(11) << vlan.interface << " | "
            << std::setw(13) << vlan.ipAddress << " | "
            << std::setw(7) << vlan.netmask << " | "
            << std::setw(6) << vlan.status << "\n";
    }
    
    return oss.str();
}

json VlanCollector::formatVlanDataJson(const std::vector<VlanInfo>& vlans) {
    json result = json::array();
    
    for (const auto& vlan : vlans) {
        json vlanObj;
        vlanObj["vlanId"] = vlan.vlanId;
        vlanObj["name"] = vlan.name;
        vlanObj["interface"] = vlan.interface;
        vlanObj["status"] = vlan.status;
        vlanObj["ipAddress"] = vlan.ipAddress;
        vlanObj["netmask"] = vlan.netmask;
        result.push_back(vlanObj);
    }
    
    return result;
}
