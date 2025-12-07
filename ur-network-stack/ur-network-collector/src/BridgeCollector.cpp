#include "BridgeCollector.h"
#include <sstream>
#include <regex>
#include <fstream>
#include <filesystem>
#include <iomanip>

json BridgeCollector::collectDataJson() {
    json result;
    result["type"] = "bridges";
    result["timestamp"] = getCurrentTimestamp();
    
    try {
        std::vector<BridgeInfo> bridges;
        std::vector<BridgePort> allPorts;
        
        if (CommandExecutor::commandExists("brctl")) {
            auto brctlBridges = parseBridgeShow();
            bridges.insert(bridges.end(), brctlBridges.begin(), brctlBridges.end());
            
            for (const auto& bridge : brctlBridges) {
                auto ports = parseBridgePorts(bridge.name);
                allPorts.insert(allPorts.end(), ports.begin(), ports.end());
            }
        }
        
        auto ipLinkBridges = parseIpLinkBridge();
        bridges.insert(bridges.end(), ipLinkBridges.begin(), ipLinkBridges.end());
        
        auto procBridges = parseProcNetBridge();
        bridges.insert(bridges.end(), procBridges.begin(), procBridges.end());
        
        result["data"] = formatBridgeDataJson(bridges, allPorts);
        result["success"] = true;
        
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Error collecting bridge data: " + std::string(e.what());
        result["data"] = json::object();
    }
    
    return result;
}

std::vector<NetworkData> BridgeCollector::collectData() {
    std::vector<NetworkData> results;
    
    try {
        std::vector<BridgeInfo> bridges;
        std::vector<BridgePort> allPorts;
        
        if (CommandExecutor::commandExists("brctl")) {
            auto brctlBridges = parseBridgeShow();
            bridges.insert(bridges.end(), brctlBridges.begin(), brctlBridges.end());
            
            for (const auto& bridge : brctlBridges) {
                auto ports = parseBridgePorts(bridge.name);
                allPorts.insert(allPorts.end(), ports.begin(), ports.end());
            }
        }
        
        auto ipLinkBridges = parseIpLinkBridge();
        bridges.insert(bridges.end(), ipLinkBridges.begin(), ipLinkBridges.end());
        
        auto procBridges = parseProcNetBridge();
        bridges.insert(bridges.end(), procBridges.begin(), procBridges.end());
        
        NetworkData data;
        data.type = "bridges";
        data.data = formatBridgeData(bridges, allPorts);
        data.timestamp = getCurrentTimestamp();
        results.push_back(data);
        
    } catch (const std::exception& e) {
        NetworkData errorData;
        errorData.type = "bridges_error";
        errorData.data = "Error collecting bridge data: " + std::string(e.what());
        errorData.timestamp = getCurrentTimestamp();
        results.push_back(errorData);
    }
    
    return results;
}

std::vector<BridgeInfo> BridgeCollector::parseBridgeShow() {
    std::vector<BridgeInfo> bridges;
    
    try {
        std::string output = CommandExecutor::executeCommand("brctl show");
        std::istringstream iss(output);
        std::string line;
        
        std::string currentBridge = "";
        bool firstLine = true;
        
        while (std::getline(iss, line)) {
            if (firstLine || line.empty()) {
                firstLine = false;
                continue;
            }
            
            std::istringstream lineStream(line);
            std::string token;
            std::vector<std::string> tokens;
            
            while (lineStream >> token) {
                tokens.push_back(token);
            }
            
            if (!tokens.empty()) {
                if (tokens[0] != currentBridge) {
                    BridgeInfo bridge;
                    bridge.name = tokens[0];
                    bridge.id = tokens.size() > 1 ? tokens[1] : "N/A";
                    bridge.stpState = tokens.size() > 2 ? tokens[2] : "N/A";
                    bridge.interfaceCount = tokens.size() > 3 ? tokens[3] : "0";
                    bridge.status = "active";
                    
                    // Get detailed bridge information
                    try {
                        std::string detailOutput = CommandExecutor::executeCommand("brctl showstp " + bridge.name);
                        std::istringstream detailIss(detailOutput);
                        std::string detailLine;
                        
                        while (std::getline(detailIss, detailLine)) {
                            if (detailLine.find("bridge id") != std::string::npos) {
                                std::regex idRegex(R"(bridge id\s+:\s+(.+))");
                                std::smatch match;
                                if (std::regex_search(detailLine, match, idRegex)) {
                                    bridge.id = match[1].str();
                                }
                            } else if (detailLine.find("stp state") != std::string::npos) {
                                std::regex stateRegex(R"(stp state\s+:\s+(.+))");
                                std::smatch match;
                                if (std::regex_search(detailLine, match, stateRegex)) {
                                    bridge.stpState = match[1].str();
                                }
                            } else if (detailLine.find("forward delay") != std::string::npos) {
                                std::regex delayRegex(R"(forward delay\s+:\s+(.+))");
                                std::smatch match;
                                if (std::regex_search(detailLine, match, delayRegex)) {
                                    bridge.stpForwardDelay = match[1].str();
                                }
                            } else if (detailLine.find("hello time") != std::string::npos) {
                                std::regex helloRegex(R"(hello time\s+:\s+(.+))");
                                std::smatch match;
                                if (std::regex_search(detailLine, match, helloRegex)) {
                                    bridge.stpHelloTime = match[1].str();
                                }
                            } else if (detailLine.find("max age") != std::string::npos) {
                                std::regex maxAgeRegex(R"(max age\s+:\s+(.+))");
                                std::smatch match;
                                if (std::regex_search(detailLine, match, maxAgeRegex)) {
                                    bridge.stpMaxAge = match[1].str();
                                }
                            } else if (detailLine.find("ageing time") != std::string::npos) {
                                std::regex ageingRegex(R"(ageing time\s+:\s+(.+))");
                                std::smatch match;
                                if (std::regex_search(detailLine, match, ageingRegex)) {
                                    bridge.ageingTime = match[1].str();
                                }
                            }
                        }
                    } catch (...) {
                        // Failed to get detailed info, use defaults
                    }
                    
                    bridges.push_back(bridge);
                    currentBridge = bridge.name;
                }
            }
        }
    } catch (...) {
        // brctl failed
    }
    
    return bridges;
}

std::vector<BridgeInfo> BridgeCollector::parseIpLinkBridge() {
    std::vector<BridgeInfo> bridges;
    
    try {
        std::string output = CommandExecutor::executeCommand("ip -d link show type bridge");
        std::istringstream iss(output);
        std::string line;
        
        std::regex bridgeRegex(R"(\d+:\s+(\S+):\s+.*bridge)");
        
        while (std::getline(iss, line)) {
            std::smatch match;
            if (std::regex_search(line, match, bridgeRegex)) {
                BridgeInfo bridge;
                bridge.name = match[1].str();
                bridge.status = "active";
                bridge.stpState = "N/A";
                bridge.id = "N/A";
                bridge.interfaceCount = "0";
                
                // Get bridge details
                try {
                    std::string detailOutput = CommandExecutor::executeCommand("bridge link show dev " + bridge.name);
                    if (!detailOutput.empty()) {
                        bridge.interfaceCount = std::to_string(std::count(detailOutput.begin(), detailOutput.end(), '\n'));
                    }
                } catch (...) {
                    // Failed to get interface count
                }
                
                bridges.push_back(bridge);
            }
        }
    } catch (...) {
        // ip link failed
    }
    
    return bridges;
}

std::vector<BridgeInfo> BridgeCollector::parseProcNetBridge() {
    std::vector<BridgeInfo> bridges;
    
    std::ifstream procFile("/proc/net/bridge");
    if (!procFile.is_open()) {
        return bridges;
    }
    
    std::string line;
    while (std::getline(procFile, line)) {
        if (line.find("bridge name") == 0 || line.empty()) {
            continue;
        }
        
        std::istringstream iss(line);
        BridgeInfo bridge;
        
        iss >> bridge.name >> bridge.id >> bridge.stpState 
            >> bridge.stpForwardDelay >> bridge.stpHelloTime 
            >> bridge.stpMaxAge >> bridge.ageingTime 
            >> bridge.helloTimer >> bridge.tcnTimer 
            >> bridge.topologyChangeTimer >> bridge.gcTimer;
        
        bridge.status = "active";
        bridge.interfaceCount = "0";
        
        bridges.push_back(bridge);
    }
    
    return bridges;
}

std::vector<BridgePort> BridgeCollector::parseBridgePorts(const std::string& bridgeName) {
    std::vector<BridgePort> ports;
    
    try {
        std::string output = CommandExecutor::executeCommand("brctl showstp " + bridgeName);
        std::istringstream iss(output);
        std::string line;
        
        std::regex portRegex(R"((\S+)\s+\((\d+)\))");
        std::string currentPort = "";
        
        while (std::getline(iss, line)) {
            std::smatch match;
            if (std::regex_search(line, match, portRegex)) {
                BridgePort port;
                port.bridgeName = bridgeName;
                port.portName = match[1].str();
                port.portId = match[2].str();
                
                currentPort = port.portName;
                ports.push_back(port);
            } else if (!currentPort.empty() && !ports.empty()) {
                BridgePort& currentPortInfo = ports.back();
                
                if (line.find("state") != std::string::npos) {
                    std::regex stateRegex(R"(state\s+:\s+(.+))");
                    if (std::regex_search(line, match, stateRegex)) {
                        currentPortInfo.state = match[1].str();
                    }
                } else if (line.find("port id") != std::string::npos) {
                    std::regex portIdRegex(R"(port id\s+:\s+(.+))");
                    if (std::regex_search(line, match, portIdRegex)) {
                        currentPortInfo.portId = match[1].str();
                    }
                } else if (line.find("priority") != std::string::npos) {
                    std::regex priorityRegex(R"(priority\s+:\s+(.+))");
                    if (std::regex_search(line, match, priorityRegex)) {
                        currentPortInfo.priority = match[1].str();
                    }
                } else if (line.find("path cost") != std::string::npos) {
                    std::regex costRegex(R"(path cost\s+:\s+(.+))");
                    if (std::regex_search(line, match, costRegex)) {
                        currentPortInfo.pathCost = match[1].str();
                    }
                }
            }
        }
    } catch (...) {
        // Failed to parse bridge ports
    }
    
    return ports;
}

std::string BridgeCollector::formatBridgeData(const std::vector<BridgeInfo>& bridges, const std::vector<BridgePort>& ports) {
    std::ostringstream oss;
    oss << "Network Bridges Configuration:\n";
    oss << "==============================\n";
    
    if (bridges.empty()) {
        oss << "No bridges configured or unable to access bridge information.\n";
        return oss.str();
    }
    
    oss << "Bridge Information:\n";
    oss << "Name       | Bridge ID       | STP State | Forward Delay | Hello Time | Max Age | Ageing Time | Interfaces | Status\n";
    oss << "-----------|-----------------|-----------|---------------|------------|---------|-------------|------------|--------\n";
    
    for (const auto& bridge : bridges) {
        oss << std::setw(10) << bridge.name << " | "
            << std::setw(15) << bridge.id << " | "
            << std::setw(9) << bridge.stpState << " | "
            << std::setw(13) << bridge.stpForwardDelay << " | "
            << std::setw(10) << bridge.stpHelloTime << " | "
            << std::setw(7) << bridge.stpMaxAge << " | "
            << std::setw(11) << bridge.ageingTime << " | "
            << std::setw(10) << bridge.interfaceCount << " | "
            << std::setw(6) << bridge.status << "\n";
    }
    
    if (!ports.empty()) {
        oss << "\nBridge Ports Information:\n";
        oss << "Bridge    | Port      | Port ID | State | Priority | Path Cost\n";
        oss << "----------|-----------|---------|-------|----------|----------\n";
        
        for (const auto& port : ports) {
            oss << std::setw(9) << port.bridgeName << " | "
                << std::setw(9) << port.portName << " | "
                << std::setw(7) << port.portId << " | "
                << std::setw(5) << port.state << " | "
                << std::setw(8) << port.priority << " | "
                << std::setw(8) << port.pathCost << "\n";
        }
    }
    
    return oss.str();
}

json BridgeCollector::formatBridgeDataJson(const std::vector<BridgeInfo>& bridges, const std::vector<BridgePort>& ports) {
    json result;
    
    json bridgesArray = json::array();
    for (const auto& bridge : bridges) {
        json bridgeObj;
        bridgeObj["name"] = bridge.name;
        bridgeObj["id"] = bridge.id;
        bridgeObj["stpState"] = bridge.stpState;
        bridgeObj["stpForwardDelay"] = bridge.stpForwardDelay;
        bridgeObj["stpHelloTime"] = bridge.stpHelloTime;
        bridgeObj["stpMaxAge"] = bridge.stpMaxAge;
        bridgeObj["ageingTime"] = bridge.ageingTime;
        bridgeObj["helloTimer"] = bridge.helloTimer;
        bridgeObj["tcnTimer"] = bridge.tcnTimer;
        bridgeObj["topologyChangeTimer"] = bridge.topologyChangeTimer;
        bridgeObj["gcTimer"] = bridge.gcTimer;
        bridgeObj["interfaceCount"] = bridge.interfaceCount;
        bridgeObj["status"] = bridge.status;
        bridgesArray.push_back(bridgeObj);
    }
    result["bridges"] = bridgesArray;
    
    json portsArray = json::array();
    for (const auto& port : ports) {
        json portObj;
        portObj["bridgeName"] = port.bridgeName;
        portObj["portName"] = port.portName;
        portObj["portId"] = port.portId;
        portObj["state"] = port.state;
        portObj["priority"] = port.priority;
        portObj["pathCost"] = port.pathCost;
        portObj["designatedBridge"] = port.designatedBridge;
        portObj["designatedPort"] = port.designatedPort;
        portObj["designatedRoot"] = port.designatedRoot;
        portObj["hairpinMode"] = port.hairpinMode;
        portObj["proxyarp"] = port.proxyarp;
        portObj["proxyarpWiFi"] = port.proxyarpWiFi;
        portObj["fastLeave"] = port.fastLeave;
        portObj["learning"] = port.learning;
        portObj["flooding"] = port.flooding;
        portObj["costMode"] = port.costMode;
        portObj["mcastSnooping"] = port.mcastSnooping;
        portObj["mcastQuerier"] = port.mcastQuerier;
        portObj["mcastRouter"] = port.mcastRouter;
        portObj["mcastFastLeave"] = port.mcastFastLeave;
        portObj["mcastStartupQueryCount"] = port.mcastStartupQueryCount;
        portObj["mcastStartupQueryInterval"] = port.mcastStartupQueryInterval;
        portObj["mcastQueryInterval"] = port.mcastQueryInterval;
        portObj["mcastQueryResponseInterval"] = port.mcastQueryResponseInterval;
        portObj["mcastLastMemberCount"] = port.mcastLastMemberCount;
        portObj["mcastLastMemberInterval"] = port.mcastLastMemberInterval;
        portObj["mcastMembershipInterval"] = port.mcastMembershipInterval;
        portObj["mcastQuerierInterval"] = port.mcastQuerierInterval;
        portsArray.push_back(portObj);
    }
    result["ports"] = portsArray;
    
    return result;
}
