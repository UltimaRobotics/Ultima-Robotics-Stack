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
        bool hasPermission = true;
        std::string permissionError = "";
        
        if (CommandExecutor::commandExists("iptables")) {
            try {
                auto filterRules = parseIptablesRules("filter");
                rules.insert(rules.end(), filterRules.begin(), filterRules.end());
                
                auto natRules = parseIptablesRules("nat");
                rules.insert(rules.end(), natRules.begin(), natRules.end());
                
                auto mangleRules = parseIptablesRules("mangle");
                rules.insert(rules.end(), mangleRules.begin(), mangleRules.end());
            } catch (const std::exception& e) {
                std::string errorMsg = e.what();
                if (errorMsg.find("Permission denied") != std::string::npos) {
                    hasPermission = false;
                    permissionError = "iptables requires root privileges";
                } else {
                    throw; // Re-throw non-permission errors
                }
            }
        }
        
        if (hasPermission && CommandExecutor::commandExists("nft")) {
            try {
                auto nftablesRules = parseNftablesRules();
                rules.insert(rules.end(), nftablesRules.begin(), nftablesRules.end());
            } catch (const std::exception& e) {
                std::string errorMsg = e.what();
                if (errorMsg.find("Permission denied") != std::string::npos) {
                    hasPermission = false;
                    permissionError = "nft requires root privileges";
                } else {
                    throw; // Re-throw non-permission errors
                }
            }
        }
        
        if (hasPermission && CommandExecutor::commandExists("ufw")) {
            try {
                auto ufwRules = parseUfwRules();
                rules.insert(rules.end(), ufwRules.begin(), ufwRules.end());
            } catch (const std::exception& e) {
                std::string errorMsg = e.what();
                if (errorMsg.find("Permission denied") != std::string::npos) {
                    hasPermission = false;
                    permissionError = "ufw requires root privileges";
                } else {
                    throw; // Re-throw non-permission errors
                }
            }
        }
        
        if (hasPermission && CommandExecutor::commandExists("firewall-cmd")) {
            try {
                auto firewalldRules = parseFirewalldRules();
                rules.insert(rules.end(), firewalldRules.begin(), firewalldRules.end());
            } catch (const std::exception& e) {
                std::string errorMsg = e.what();
                if (errorMsg.find("Permission denied") != std::string::npos) {
                    hasPermission = false;
                    permissionError = "firewall-cmd requires root privileges";
                } else {
                    throw; // Re-throw non-permission errors
                }
            }
        }
        
        result["data"] = formatFirewallDataJson(rules);
        result["success"] = true;
        
        // Add warning if permission was denied
        if (!hasPermission && !permissionError.empty()) {
            result["warning"] = permissionError;
        }
        
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
        
        // Fixed regex to match actual iptables output structure (10 columns exactly)
        std::regex ruleRegex(R"(^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*$)");
        std::regex chainRegex(R"(^Chain\s+(\S+)\s+\(policy\s+(\S+)\s+(\d+)\s+packets,\s+(\d+)\s+bytes\))");
        std::regex chainRegexSimple(R"(^Chain\s+(\S+)\s+\()");
        
        std::string currentChain = "";
        std::string currentPolicy = "";
        
        while (std::getline(iss, line)) {
            // Skip empty lines
            if (line.empty()) {
                continue;
            }
            
            // Parse chain headers
            std::smatch chainMatch;
            if (std::regex_match(line, chainMatch, chainRegex)) {
                currentChain = chainMatch[1].str();
                currentPolicy = chainMatch[2].str();
                continue;
            } else if (std::regex_search(line, chainMatch, chainRegexSimple)) {
                currentChain = chainMatch[1].str();
                continue;
            }
            
            // Skip header lines
            if (line.find("num") != std::string::npos || 
                line.find("pkts") != std::string::npos || 
                line.find("target") != std::string::npos ||
                line.find("prot") != std::string::npos ||
                line.find("opt") != std::string::npos ||
                line.find("in") != std::string::npos ||
                line.find("out") != std::string::npos ||
                line.find("source") != std::string::npos ||
                line.find("destination") != std::string::npos) {
                continue;
            }
            
            // Parse rule lines - handle both 10-column format and multi-line rules
            std::smatch ruleMatch;
            if (std::regex_match(line, ruleMatch, ruleRegex)) {
                FirewallRule rule;
                rule.type = "iptables";
                rule.table = table;
                rule.chain = currentChain;
                rule.lineNumber = std::stoi(ruleMatch[1].str());
                rule.packets = std::stol(ruleMatch[2].str());
                rule.bytes = std::stol(ruleMatch[3].str());
                rule.target = ruleMatch[4].str();
                rule.protocol = ruleMatch[5].str();
                rule.inputInterface = ruleMatch[6].str();
                rule.outputInterface = ruleMatch[7].str();
                rule.source = ruleMatch[8].str();
                rule.destination = ruleMatch[9].str();
                
                // Validate rule has a meaningful source address
                if (rule.source.empty() || rule.source == "*" || rule.source == "0.0.0.0/0") {
                    // Skip rules with empty or wildcard source addresses
                    continue;
                }
                
                rules.push_back(rule);
            } else {
                // Handle multi-line rules (with additional info on next line)
                // First try to match the first 8 columns
                std::regex partialRegex(R"(^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.+))");
                if (std::regex_match(line, ruleMatch, partialRegex)) {
                    // Read the next line for the rest
                    std::string nextLine;
                    if (std::getline(iss, nextLine)) {
                        std::string combinedLine = line + " " + nextLine;
                        std::smatch fullMatch;
                        if (std::regex_match(combinedLine, fullMatch, ruleRegex)) {
                            FirewallRule rule;
                            rule.type = "iptables";
                            rule.table = table;
                            rule.chain = currentChain;
                            rule.lineNumber = std::stoi(fullMatch[1].str());
                            rule.packets = std::stol(fullMatch[2].str());
                            rule.bytes = std::stol(fullMatch[3].str());
                            rule.target = fullMatch[4].str();
                            rule.protocol = fullMatch[5].str();
                            rule.inputInterface = fullMatch[6].str();
                            rule.outputInterface = fullMatch[7].str();
                            rule.source = fullMatch[8].str();
                            rule.destination = fullMatch[9].str();
                            
                            // Extract port information from the combined line
                            std::regex dportRegex(R"(dpt:(\d+))");
                            std::regex sportRegex(R"(spt:(\d+))");
                            std::regex stateRegex(R"(state\s+(\S+))");
                            std::regex addrTypeRegex(R"(ADDRTYPE match dst-type (\S+))");
                            
                            std::smatch match;
                            if (std::regex_search(line, match, dportRegex)) {
                                rule.destPort = match[1].str();
                            }
                            if (std::regex_search(line, match, sportRegex)) {
                                rule.sourcePort = match[1].str();
                            }
                            if (std::regex_search(line, match, stateRegex)) {
                                rule.state = match[1].str();
                            }
                            
                            // Validate rule has a meaningful source address
                            if (rule.source.empty() || rule.source == "*" || rule.source == "0.0.0.0/0") {
                                // Skip rules with empty or wildcard source addresses
                                continue;
                            }
                            
                            rules.push_back(rule);
                        }
                    }
                }
            }
        }
    } catch (...) {
        // iptables failed - return empty rules
    }
    
    return rules;
}

std::vector<FirewallRule> FirewallCollector::parseNftablesRules() {
    std::vector<FirewallRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("nft list ruleset");
        std::istringstream iss(output);
        std::string line;
        
        // Updated regex patterns to match actual nftables output
        std::regex tableRegex(R"(^table\s+(\w+)\s+(\w+)\s*\{)");
        std::regex chainRegex(R"(^\s*chain\s+(\S+)\s*\{)");
        std::regex hookRegex(R"(^\s*type\s+(\w+)\s+hook\s+(\w+)\s+priority\s+(\w+);\s+policy\s+(\w+);)");
        std::regex ruleRegex(R"(^\s*(.+))");
        
        std::string currentTable = "";
        std::string currentChain = "";
        std::string currentHookType = "";
        int ruleCounter = 0;
        
        while (std::getline(iss, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            // Skip empty lines and closing braces
            if (line.empty() || line == "}" || line == "};") {
                continue;
            }
            
            // Parse table declarations
            std::smatch tableMatch;
            if (std::regex_match(line, tableMatch, tableRegex)) {
                currentTable = tableMatch[2].str();
                continue;
            }
            
            // Parse chain declarations
            std::smatch chainMatch;
            if (std::regex_match(line, chainMatch, chainRegex)) {
                currentChain = chainMatch[1].str();
                ruleCounter = 0;
                continue;
            }
            
            // Parse hook specifications (skip these as they're not rules)
            std::smatch hookMatch;
            if (std::regex_match(line, hookMatch, hookRegex)) {
                currentHookType = hookMatch[1].str();
                continue;
            }
            
            // Parse actual rules (skip hook specifications and empty lines)
            if ((line.find("counter packets") != std::string::npos ||
                line.find("accept") != std::string::npos ||
                line.find("drop") != std::string::npos ||
                line.find("reject") != std::string::npos ||
                line.find("jump") != std::string::npos ||
                line.find("masquerade") != std::string::npos ||
                line.find("return") != std::string::npos) &&
                line.find("type filter hook") == std::string::npos &&
                line.find("type nat hook") == std::string::npos &&
                line.find("policy") == std::string::npos) {
                
                FirewallRule rule;
                rule.type = "nftables";
                rule.table = currentTable;
                rule.chain = currentChain;
                rule.lineNumber = ++ruleCounter;
                
                // Extract target/action
                if (line.find("accept") != std::string::npos) {
                    rule.target = "ACCEPT";
                } else if (line.find("drop") != std::string::npos) {
                    rule.target = "DROP";
                } else if (line.find("reject") != std::string::npos) {
                    rule.target = "REJECT";
                } else if (line.find("jump") != std::string::npos) {
                    std::regex jumpRegex(R"(jump\s+(\S+))");
                    std::smatch jumpMatch;
                    if (std::regex_search(line, jumpMatch, jumpRegex)) {
                        rule.target = jumpMatch[1].str();
                    }
                }
                
                // Extract packet and byte counters
                std::regex counterRegex(R"(counter packets (\d+) bytes (\d+))");
                std::smatch counterMatch;
                if (std::regex_search(line, counterMatch, counterRegex)) {
                    rule.packets = std::stol(counterMatch[1].str());
                    rule.bytes = std::stol(counterMatch[2].str());
                }
                
                // Extract interface information with proper quote handling
                std::regex iifnameRegex(R"(iifname\s+["']?([^"'\s]+)["']?)");
                std::regex oifnameRegex(R"(oifname\s+["']?([^"'\s]+)["']?)");
                std::regex oifnameNotRegex(R"(oifname\s+!=\s*["']?([^"'\s]+)["']?)");
                std::smatch match;
                if (std::regex_search(line, match, iifnameRegex)) {
                    rule.inputInterface = match[1].str();
                }
                if (std::regex_search(line, match, oifnameRegex)) {
                    rule.outputInterface = match[1].str();
                }
                if (std::regex_search(line, match, oifnameNotRegex)) {
                    rule.outputInterface = "!" + match[1].str();
                }
                
                // Extract address information
                std::regex saddrRegex(R"(saddr\s+["']?([^"'\s]+)["']?)");
                std::regex daddrRegex(R"(daddr\s+["']?([^"'\s]+)["']?)");
                std::regex ipSaddrRegex(R"(ip\s+saddr\s+["']?([^"'\s]+)["']?)");
                std::regex ipDaddrRegex(R"(ip\s+daddr\s+["']?([^"'\s]+)["']?)");
                if (std::regex_search(line, match, saddrRegex)) {
                    rule.source = match[1].str();
                } else if (std::regex_search(line, match, ipSaddrRegex)) {
                    rule.source = match[1].str();
                }
                if (std::regex_search(line, match, daddrRegex)) {
                    rule.destination = match[1].str();
                } else if (std::regex_search(line, match, ipDaddrRegex)) {
                    rule.destination = match[1].str();
                }
                
                // Extract protocol information
                std::regex ipProtoRegex(R"(ip protocol\s+(\S+))");
                std::regex metaProtoRegex(R"(meta l4proto\s+(\S+))");
                if (std::regex_search(line, match, ipProtoRegex)) {
                    rule.protocol = match[1].str();
                } else if (std::regex_search(line, match, metaProtoRegex)) {
                    rule.protocol = match[1].str();
                }
                
                // Extract port information
                std::regex dportRegex(R"(dport\s+(\S+))");
                std::regex sportRegex(R"(sport\s+(\S+))");
                if (std::regex_search(line, match, dportRegex)) {
                    rule.destPort = match[1].str();
                }
                if (std::regex_search(line, match, sportRegex)) {
                    rule.sourcePort = match[1].str();
                }
                
                // Extract connection state
                std::regex ctStateRegex(R"(ct state\s+(\S+))");
                if (std::regex_search(line, match, ctStateRegex)) {
                    rule.state = match[1].str();
                }
                
                // Validate rule has a meaningful source address or specific interface
                if (rule.source.empty() && rule.inputInterface.empty() && rule.outputInterface.empty()) {
                    // Skip rules with no source, input interface, or output interface
                    continue;
                }
                
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // nftables failed - return empty rules
    }
    
    return rules;
}

std::vector<FirewallRule> FirewallCollector::parseUfwRules() {
    std::vector<FirewallRule> rules;
    
    try {
        std::string output = CommandExecutor::executeCommand("ufw status numbered");
        std::istringstream iss(output);
        std::string line;
        
        // Check if UFW is inactive
        if (output.find("Status: inactive") != std::string::npos) {
            // Return a single rule indicating UFW is inactive with proper values
            FirewallRule rule;
            rule.type = "ufw";
            rule.table = "filter";
            rule.chain = "INPUT";
            rule.lineNumber = 0;
            rule.target = "INACTIVE";
            rule.protocol = "all";
            rule.source = "0.0.0.0/0";
            rule.destination = "0.0.0.0/0";
            rule.packets = 0;
            rule.bytes = 0;
            rules.push_back(rule);
            return rules;
        }
        
        // Parse active rules
        std::regex ruleRegex(R"(^\s*\[(\d+)\]\s+(\S+)\s+(.+))");
        std::regex headerRegex(R"(^Status:\s+(\S+))");
        
        while (std::getline(iss, line)) {
            // Skip header lines and empty lines
            if (line.empty() || 
                line.find("--") != std::string::npos ||
                line.find("To") != std::string::npos ||
                line.find("Action") != std::string::npos ||
                line.find("From") != std::string::npos) {
                continue;
            }
            
            // Parse status header
            std::smatch headerMatch;
            if (std::regex_match(line, headerMatch, headerRegex)) {
                continue;
            }
            
            // Parse rule lines
            std::smatch match;
            if (std::regex_match(line, match, ruleRegex)) {
                FirewallRule rule;
                rule.type = "ufw";
                rule.table = "filter";
                rule.lineNumber = std::stoi(match[1].str());
                rule.target = match[2].str(); // ALLOW, DENY, LIMIT, etc.
                rule.additional = match[3].str();
                
                std::string details = match[3].str();
                
                // Enhanced parsing for protocol, ports, and addresses
                std::regex protoPortRegex(R"((\w+)(?:\s+(\S+))?(?:\s+(.+))?)");
                std::smatch protoPortMatch;
                
                if (std::regex_match(details, protoPortMatch, protoPortRegex)) {
                    rule.protocol = protoPortMatch[1].str();
                    
                    if (protoPortMatch.size() > 2 && !protoPortMatch[2].str().empty()) {
                        // Check if it's a port number or service name
                        std::string portOrService = protoPortMatch[2].str();
                        if (std::isdigit(portOrService[0]) || portOrService.find("/") != std::string::npos) {
                            rule.destPort = portOrService;
                        }
                    }
                    
                    if (protoPortMatch.size() > 3 && !protoPortMatch[3].str().empty()) {
                        rule.source = protoPortMatch[3].str();
                        // Default destination if not specified
                        if (rule.source.find("Anywhere") != std::string::npos) {
                            rule.destination = "0.0.0.0/0";
                        }
                    }
                }
                
                // Handle common UFW patterns
                if (details.find("Anywhere") != std::string::npos) {
                    if (details.find("Anywhere") == 0) {
                        rule.source = "0.0.0.0/0";
                        rule.destination = "0.0.0.0/0";
                    }
                }
                
                // Extract specific port patterns
                std::regex portRegex(R"((\d+)/(\w+))");
                std::smatch portMatch;
                if (std::regex_search(details, portMatch, portRegex)) {
                    rule.destPort = portMatch[1].str();
                    rule.protocol = portMatch[2].str();
                }
                
                // Set default values if not found
                if (rule.source.empty()) rule.source = "0.0.0.0/0";
                if (rule.destination.empty()) rule.destination = "0.0.0.0/0";
                if (rule.protocol.empty()) rule.protocol = "all";
                
                // Validate rule has a meaningful source address (not wildcard for active rules)
                if (rule.source.empty() || rule.source == "*" || rule.source == "0.0.0.0/0") {
                    // Skip UFW rules with empty or wildcard source addresses
                    continue;
                }
                
                rules.push_back(rule);
            }
        }
    } catch (...) {
        // UFW failed - return empty rules
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
