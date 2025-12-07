#include "FirewallCollector.h"
#include <sstream>
#include <regex>
#include <iomanip>

json FirewallCollector::collectDataJson() {
    json result;
    result["type"] = "firewall";
    result["timestamp"] = getCurrentTimestamp();
    
    try {
        std::vector<FirewallRule> rules;
        
        if (CommandExecutor::commandExists("iptables")) {
            auto filterRules = parseIptablesRules("filter");
            rules.insert(rules.end(), filterRules.begin(), filterRules.end());
            
            auto natRules = parseIptablesRules("nat");
            rules.insert(rules.end(), natRules.begin(), natRules.end());
            
            auto mangleRules = parseIptablesRules("mangle");
            rules.insert(rules.end(), mangleRules.begin(), mangleRules.end());
        }
        
        if (CommandExecutor::commandExists("nft")) {
            auto nftablesRules = parseNftablesRules();
            rules.insert(rules.end(), nftablesRules.begin(), nftablesRules.end());
        }
        
        if (CommandExecutor::commandExists("ufw")) {
            auto ufwRules = parseUfwRules();
            rules.insert(rules.end(), ufwRules.begin(), ufwRules.end());
        }
        
        if (CommandExecutor::commandExists("firewall-cmd")) {
            auto firewalldRules = parseFirewalldRules();
            rules.insert(rules.end(), firewalldRules.begin(), firewalldRules.end());
        }
        
        result["data"] = formatFirewallDataJson(rules);
        result["success"] = true;
        
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Error collecting firewall data: " + std::string(e.what());
        result["data"] = json::array();
    }
    
    return result;
}

std::vector<NetworkData> FirewallCollector::collectData() {
    std::vector<NetworkData> results;
    
    try {
        std::vector<FirewallRule> rules;
        
        if (CommandExecutor::commandExists("iptables")) {
            auto filterRules = parseIptablesRules("filter");
            rules.insert(rules.end(), filterRules.begin(), filterRules.end());
            
            auto natRules = parseIptablesRules("nat");
            rules.insert(rules.end(), natRules.begin(), natRules.end());
            
            auto mangleRules = parseIptablesRules("mangle");
            rules.insert(rules.end(), mangleRules.begin(), mangleRules.end());
        }
        
        if (CommandExecutor::commandExists("nft")) {
            auto nftablesRules = parseNftablesRules();
            rules.insert(rules.end(), nftablesRules.begin(), nftablesRules.end());
        }
        
        if (CommandExecutor::commandExists("ufw")) {
            auto ufwRules = parseUfwRules();
            rules.insert(rules.end(), ufwRules.begin(), ufwRules.end());
        }
        
        if (CommandExecutor::commandExists("firewall-cmd")) {
            auto firewalldRules = parseFirewalldRules();
            rules.insert(rules.end(), firewalldRules.begin(), firewalldRules.end());
        }
        
        NetworkData data;
        data.type = "firewall";
        data.data = formatFirewallData(rules);
        data.timestamp = getCurrentTimestamp();
        results.push_back(data);
        
    } catch (const std::exception& e) {
        NetworkData errorData;
        errorData.type = "firewall_error";
        errorData.data = "Error collecting firewall data: " + std::string(e.what());
        errorData.timestamp = getCurrentTimestamp();
        results.push_back(errorData);
    }
    
    return results;
}

std::vector<FirewallRule> FirewallCollector::parseIptablesRules(const std::string& table) {
    std::vector<FirewallRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("iptables -t " + table + " -L -n -v --line-numbers");
        std::istringstream iss(output);
        std::string line;
        
        std::regex ruleRegex(R"((\d+)\s+(\d+)\s+(\d+)\s+(\S+)\s+(\S+)\s+(.+)\s+(\S+))");
        std::regex chainRegex(R"(Chain\s+(\S+))");
        
        std::string currentChain = "";
        
        while (std::getline(iss, line)) {
            std::smatch chainMatch;
            if (std::regex_match(line, chainMatch, chainRegex)) {
                currentChain = chainMatch[1].str();
                continue;
            }
            
            if (line.find("pkts") != std::string::npos || line.find("target") != std::string::npos) {
                continue;
            }
            
            std::smatch ruleMatch;
            if (std::regex_search(line, ruleMatch, ruleRegex)) {
                FirewallRule rule;
                rule.type = "iptables";
                rule.table = table;
                rule.chain = currentChain;
                rule.lineNumber = std::stoi(ruleMatch[1].str());
                rule.packets = std::stol(ruleMatch[2].str());
                rule.bytes = std::stol(ruleMatch[3].str());
                rule.target = ruleMatch[4].str();
                rule.protocol = ruleMatch[5].str();
                
                std::string details = ruleMatch[6].str() + " " + ruleMatch[7].str();
                
                std::regex srcRegex(R"(s=(\S+))");
                std::regex dstRegex(R"(d=(\S+))");
                std::regex sportRegex(R"(spt:(\d+))");
                std::regex dportRegex(R"(dpt:(\d+))");
                std::regex inRegex(R"(in:(\S+))");
                std::regex outRegex(R"(out:(\S+))");
                std::regex stateRegex(R"(state\s+(\S+))");
                
                std::smatch match;
                if (std::regex_search(details, match, srcRegex)) {
                    rule.source = match[1].str();
                }
                if (std::regex_search(details, match, dstRegex)) {
                    rule.destination = match[1].str();
                }
                if (std::regex_search(details, match, sportRegex)) {
                    rule.sourcePort = match[1].str();
                }
                if (std::regex_search(details, match, dportRegex)) {
                    rule.destPort = match[1].str();
                }
                if (std::regex_search(details, match, inRegex)) {
                    rule.inputInterface = match[1].str();
                }
                if (std::regex_search(details, match, outRegex)) {
                    rule.outputInterface = match[1].str();
                }
                if (std::regex_search(details, match, stateRegex)) {
                    rule.state = match[1].str();
                }
                
                rule.additional = details;
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // iptables failed
    }
    
    return rules;
}

std::vector<FirewallRule> FirewallCollector::parseNftablesRules() {
    std::vector<FirewallRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("nft list ruleset");
        std::istringstream iss(output);
        std::string line;
        
        std::regex tableRegex(R"(table\s+(\w+)\s+(\w+)\s+\{)");
        std::regex chainRegex(R"(chain\s+(\S+)\s+\{)");
        std::regex ruleRegex(R"(\s+(.+))");
        
        std::string currentTable = "";
        std::string currentChain = "";
        int ruleCounter = 0;
        
        while (std::getline(iss, line)) {
            std::smatch tableMatch;
            if (std::regex_search(line, tableMatch, tableRegex)) {
                currentTable = tableMatch[2].str();
                continue;
            }
            
            std::smatch chainMatch;
            if (std::regex_search(line, chainMatch, chainRegex)) {
                currentChain = chainMatch[1].str();
                ruleCounter = 0;
                continue;
            }
            
            if (line.find("type filter hook") != std::string::npos) {
                continue;
            }
            
            if (line.find("ip") != std::string::npos && line.find("accept") != std::string::npos) {
                FirewallRule rule;
                rule.type = "nftables";
                rule.table = currentTable;
                rule.chain = currentChain;
                rule.lineNumber = ++ruleCounter;
                rule.target = "ACCEPT";
                rule.additional = line;
                
                std::regex saddrRegex(R"(saddr\s+(\S+))");
                std::regex daddrRegex(R"(daddr\s+(\S+))");
                std::regex dportRegex(R"(dport\s+(\S+))");
                std::regex protoRegex(R"(ip protocol\s+(\S+))");
                
                std::smatch match;
                if (std::regex_search(line, match, saddrRegex)) {
                    rule.source = match[1].str();
                }
                if (std::regex_search(line, match, daddrRegex)) {
                    rule.destination = match[1].str();
                }
                if (std::regex_search(line, match, dportRegex)) {
                    rule.destPort = match[1].str();
                }
                if (std::regex_search(line, match, protoRegex)) {
                    rule.protocol = match[1].str();
                }
                
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // nftables failed
    }
    
    return rules;
}

std::vector<FirewallRule> FirewallCollector::parseUfwRules() {
    std::vector<FirewallRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("ufw status numbered");
        std::istringstream iss(output);
        std::string line;
        
        std::regex ruleRegex(R"(\[(\d+)\]\s+(\S+)\s+(.+))");
        
        while (std::getline(iss, line)) {
            std::smatch match;
            if (std::regex_match(line, match, ruleRegex)) {
                FirewallRule rule;
                rule.type = "ufw";
                rule.table = "filter";
                rule.lineNumber = std::stoi(match[1].str());
                rule.target = match[2].str();
                rule.additional = match[3].str();
                
                std::string details = match[3].str();
                std::regex protoRegex(R"((\w+)\s+)");
                std::regex portRegex(R"(\s+(\d+))");
                
                std::smatch protoMatch;
                if (std::regex_search(details, protoMatch, protoRegex)) {
                    rule.protocol = protoMatch[1].str();
                }
                
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // ufw failed
    }
    
    return rules;
}

std::vector<FirewallRule> FirewallCollector::parseFirewalldRules() {
    std::vector<FirewallRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("firewall-cmd --list-all-zones");
        std::istringstream iss(output);
        std::string line;
        
        std::string currentZone = "";
        std::regex zoneRegex(R"((\w+)\s+\()");
        
        while (std::getline(iss, line)) {
            std::smatch zoneMatch;
            if (std::regex_search(line, zoneMatch, zoneRegex)) {
                currentZone = zoneMatch[1].str();
                continue;
            }
            
            if (line.find("services:") != std::string::npos) {
                FirewallRule rule;
                rule.type = "firewalld";
                rule.table = "filter";
                rule.chain = currentZone;
                rule.target = "ACCEPT";
                rule.additional = line;
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // firewalld failed
    }
    
    return rules;
}

std::string FirewallCollector::formatFirewallData(const std::vector<FirewallRule>& rules) {
    std::ostringstream oss;
    oss << "Firewall Rules Configuration:\n";
    oss << "==============================\n";
    
    if (rules.empty()) {
        oss << "No firewall rules configured or unable to access firewall information.\n";
        return oss.str();
    }
    
    oss << "Type      | Table   | Chain    | # | Target  | Protocol | Source    | Dest      | SPort | DPort | In | Out | State | Packets | Bytes\n";
    oss << "----------|---------|----------|---|---------|----------|-----------|-----------|-------|-------|-----|-----|-------|---------|------\n";
    
    for (const auto& rule : rules) {
        oss << std::setw(9) << rule.type << " | "
            << std::setw(7) << rule.table << " | "
            << std::setw(8) << rule.chain << " | "
            << std::setw(1) << rule.lineNumber << " | "
            << std::setw(7) << rule.target << " | "
            << std::setw(8) << rule.protocol << " | "
            << std::setw(9) << rule.source << " | "
            << std::setw(9) << rule.destination << " | "
            << std::setw(5) << rule.sourcePort << " | "
            << std::setw(5) << rule.destPort << " | "
            << std::setw(3) << rule.inputInterface << " | "
            << std::setw(3) << rule.outputInterface << " | "
            << std::setw(5) << rule.state << " | "
            << std::setw(7) << rule.packets << " | "
            << std::setw(4) << rule.bytes << "\n";
    }
    
    return oss.str();
}

json FirewallCollector::formatFirewallDataJson(const std::vector<FirewallRule>& rules) {
    json result = json::array();
    
    for (const auto& rule : rules) {
        json ruleObj;
        ruleObj["type"] = rule.type;
        ruleObj["table"] = rule.table;
        ruleObj["chain"] = rule.chain;
        ruleObj["lineNumber"] = rule.lineNumber;
        ruleObj["target"] = rule.target;
        ruleObj["protocol"] = rule.protocol;
        ruleObj["source"] = rule.source;
        ruleObj["destination"] = rule.destination;
        ruleObj["sourcePort"] = rule.sourcePort;
        ruleObj["destPort"] = rule.destPort;
        ruleObj["inputInterface"] = rule.inputInterface;
        ruleObj["outputInterface"] = rule.outputInterface;
        ruleObj["state"] = rule.state;
        ruleObj["additional"] = rule.additional;
        ruleObj["packets"] = rule.packets;
        ruleObj["bytes"] = rule.bytes;
        result.push_back(ruleObj);
    }
    
    return result;
}
