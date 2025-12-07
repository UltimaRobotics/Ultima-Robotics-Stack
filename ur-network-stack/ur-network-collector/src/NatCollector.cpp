#include "NatCollector.h"
#include <sstream>
#include <regex>
#include <iomanip>

json NatCollector::collectDataJson() {
    json result;
    result["type"] = "nat";
    result["timestamp"] = getCurrentTimestamp();
    
    try {
        std::vector<NatRule> rules;
        
        if (CommandExecutor::commandExists("iptables")) {
            auto iptablesRules = parseIptablesNat();
            rules.insert(rules.end(), iptablesRules.begin(), iptablesRules.end());
        }
        
        if (CommandExecutor::commandExists("nft")) {
            auto nftablesRules = parseNftablesNat();
            rules.insert(rules.end(), nftablesRules.begin(), nftablesRules.end());
        }
        
        if (CommandExecutor::commandExists("conntrack")) {
            auto conntrackRules = parseConntrack();
            rules.insert(rules.end(), conntrackRules.begin(), conntrackRules.end());
        }
        
        result["data"] = formatNatDataJson(rules);
        result["success"] = true;
        
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Error collecting NAT data: " + std::string(e.what());
        result["data"] = json::array();
    }
    
    return result;
}

std::vector<NetworkData> NatCollector::collectData() {
    std::vector<NetworkData> results;
    
    try {
        std::vector<NatRule> rules;
        
        if (CommandExecutor::commandExists("iptables")) {
            auto iptablesRules = parseIptablesNat();
            rules.insert(rules.end(), iptablesRules.begin(), iptablesRules.end());
        }
        
        if (CommandExecutor::commandExists("nft")) {
            auto nftablesRules = parseNftablesNat();
            rules.insert(rules.end(), nftablesRules.begin(), nftablesRules.end());
        }
        
        if (CommandExecutor::commandExists("conntrack")) {
            auto conntrackRules = parseConntrack();
            rules.insert(rules.end(), conntrackRules.begin(), conntrackRules.end());
        }
        
        NetworkData data;
        data.type = "nat";
        data.data = formatNatData(rules);
        data.timestamp = getCurrentTimestamp();
        results.push_back(data);
        
    } catch (const std::exception& e) {
        NetworkData errorData;
        errorData.type = "nat_error";
        errorData.data = "Error collecting NAT data: " + std::string(e.what());
        errorData.timestamp = getCurrentTimestamp();
        results.push_back(errorData);
    }
    
    return results;
}

std::vector<NatRule> NatCollector::parseIptablesNat() {
    std::vector<NatRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("iptables -t nat -L -n -v --line-numbers");
        std::istringstream iss(output);
        std::string line;
        
        std::regex ruleRegex(R"((\d+)\s+(\d+)\s+(\S+)\s+(\S+)\s+(.+)\s+(\S+))");
        std::regex chainRegex(R"(Chain\s+(\S+))");
        
        std::string currentChain = "";
        
        while (std::getline(iss, line)) {
            std::smatch chainMatch;
            if (std::regex_match(line, chainMatch, chainRegex)) {
                currentChain = chainMatch[1].str();
                continue;
            }
            
            std::smatch ruleMatch;
            if (std::regex_match(line, ruleMatch, ruleRegex)) {
                NatRule rule;
                rule.chain = currentChain;
                rule.target = ruleMatch[6].str();
                
                std::string details = ruleMatch[5].str();
                
                std::regex srcRegex(R"(s=(\S+))");
                std::regex dstRegex(R"(d=(\S+))");
                std::regex protoRegex(R"(proto=(\S+))");
                std::regex sportRegex(R"(spt:(\d+))");
                std::regex dportRegex(R"(dpt:(\d+))");
                
                std::smatch match;
                if (std::regex_search(details, match, srcRegex)) {
                    rule.source = match[1].str();
                }
                if (std::regex_search(details, match, dstRegex)) {
                    rule.destination = match[1].str();
                }
                if (std::regex_search(details, match, protoRegex)) {
                    rule.protocol = match[1].str();
                }
                if (std::regex_search(details, match, sportRegex)) {
                    rule.sourcePort = match[1].str();
                }
                if (std::regex_search(details, match, dportRegex)) {
                    rule.destPort = match[1].str();
                }
                
                rule.type = "iptables";
                rule.additional = details;
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // iptables failed
    }
    
    return rules;
}

std::vector<NatRule> NatCollector::parseNftablesNat() {
    std::vector<NatRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("nft list table ip nat");
        std::istringstream iss(output);
        std::string line;
        
        std::regex chainRegex(R"(chain\s+(\S+)\s+\{)");
        std::regex ruleRegex(R"(\s+(.+))");
        
        std::string currentChain = "";
        
        while (std::getline(iss, line)) {
            std::smatch chainMatch;
            if (std::regex_search(line, chainMatch, chainRegex)) {
                currentChain = chainMatch[1].str();
                continue;
            }
            
            if (line.find("type nat hook") != std::string::npos) {
                continue;
            }
            
            if (line.find("ip") != std::string::npos && line.find("daddr") != std::string::npos) {
                NatRule rule;
                rule.chain = currentChain;
                rule.type = "nftables";
                rule.additional = line;
                
                std::regex daddrRegex(R"(daddr\s+(\S+))");
                std::regex snatRegex(R"(snat\s+to\s+(\S+))");
                std::regex dnatRegex(R"(dnat\s+to\s+(\S+))");
                std::regex masqRegex(R"(masquerade)");
                
                std::smatch match;
                if (std::regex_search(line, match, daddrRegex)) {
                    rule.destination = match[1].str();
                }
                if (std::regex_search(line, match, snatRegex)) {
                    rule.target = "SNAT";
                    rule.additional = match[1].str();
                } else if (std::regex_search(line, match, dnatRegex)) {
                    rule.target = "DNAT";
                    rule.additional = match[1].str();
                } else if (std::regex_search(line, masqRegex)) {
                    rule.target = "MASQUERADE";
                }
                
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // nftables failed
    }
    
    return rules;
}

std::vector<NatRule> NatCollector::parseConntrack() {
    std::vector<NatRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("conntrack -L 2>/dev/null");
        std::istringstream iss(output);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.find("NAT") != std::string::npos) {
                NatRule rule;
                rule.type = "conntrack";
                rule.additional = line;
                
                std::regex srcRegex(R"(src=(\d+\.\d+\.\d+\.\d+))");
                std::regex dstRegex(R"(dst=(\d+\.\d+\.\d+\.\d+))");
                std::regex sportRegex(R"(sport=(\d+))");
                std::regex dportRegex(R"(dport=(\d+))");
                
                std::smatch match;
                if (std::regex_search(line, match, srcRegex)) {
                    rule.source = match[1].str();
                }
                if (std::regex_search(line, match, dstRegex)) {
                    rule.destination = match[1].str();
                }
                if (std::regex_search(line, match, sportRegex)) {
                    rule.sourcePort = match[1].str();
                }
                if (std::regex_search(line, match, dportRegex)) {
                    rule.destPort = match[1].str();
                }
                
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // conntrack failed
    }
    
    return rules;
}

std::string NatCollector::formatNatData(const std::vector<NatRule>& rules) {
    std::ostringstream oss;
    oss << "NAT Rules Configuration:\n";
    oss << "========================\n";
    
    if (rules.empty()) {
        oss << "No NAT rules configured or unable to access NAT information.\n";
        return oss.str();
    }
    
    oss << "Type      | Chain     | Source           | Destination      | Protocol | SPort | DPort | Target     | Additional\n";
    oss << "----------|-----------|------------------|------------------|----------|-------|-------|------------|-----------\n";
    
    for (const auto& rule : rules) {
        oss << std::setw(9) << rule.type << " | "
            << std::setw(9) << rule.chain << " | "
            << std::setw(16) << rule.source << " | "
            << std::setw(16) << rule.destination << " | "
            << std::setw(8) << rule.protocol << " | "
            << std::setw(5) << rule.sourcePort << " | "
            << std::setw(5) << rule.destPort << " | "
            << std::setw(10) << rule.target << " | "
            << rule.additional << "\n";
    }
    
    return oss.str();
}

json NatCollector::formatNatDataJson(const std::vector<NatRule>& rules) {
    json result = json::array();
    
    for (const auto& rule : rules) {
        json ruleObj;
        ruleObj["type"] = rule.type;
        ruleObj["chain"] = rule.chain;
        ruleObj["source"] = rule.source;
        ruleObj["destination"] = rule.destination;
        ruleObj["protocol"] = rule.protocol;
        ruleObj["sourcePort"] = rule.sourcePort;
        ruleObj["destPort"] = rule.destPort;
        ruleObj["target"] = rule.target;
        ruleObj["additional"] = rule.additional;
        result.push_back(ruleObj);
    }
    
    return result;
}
