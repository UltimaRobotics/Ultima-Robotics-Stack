#include "urwt/parsers/iw_scan_parser.hpp"
#include <sstream>
#include <algorithm>
#include <regex>

namespace urwt {

Result<std::vector<NetworkInfo>, std::string> IwScanParser::parse(const std::string& output) {
    if (output.empty()) {
        return Result<std::vector<NetworkInfo>, std::string>::error("Empty scan output");
    }
    
    std::vector<NetworkInfo> networks;
    auto blocks = splitIntoBlocks(output);
    
    for (const auto& block : blocks) {
        try {
            auto network = parseNetworkBlock(block);
            networks.push_back(network);
        } catch (const std::exception& e) {
            continue;
        }
    }
    
    std::sort(networks.begin(), networks.end());
    
    return Result<std::vector<NetworkInfo>, std::string>::ok(networks);
}

std::vector<std::string> IwScanParser::splitIntoBlocks(const std::string& output) {
    std::vector<std::string> blocks;
    std::string current_block;
    auto lines = splitLines(output);
    
    for (const auto& line : lines) {
        if (line.find("BSS ") == 0) {
            if (!current_block.empty()) {
                blocks.push_back(current_block);
            }
            current_block = line + "\n";
        } else {
            current_block += line + "\n";
        }
    }
    
    if (!current_block.empty()) {
        blocks.push_back(current_block);
    }
    
    return blocks;
}

NetworkInfo IwScanParser::parseNetworkBlock(const std::string& block) {
    auto lines = splitLines(block);
    
    MacAddress bssid = parseBSSID(lines[0]);
    std::string ssid;
    int frequency = 0;
    int signal = -100;
    SecurityType security = SecurityType::Unknown;
    
    for (const auto& line : lines) {
        std::string trimmed = trim(line);
        
        if (trimmed.find("SSID:") != std::string::npos) {
            ssid = parseSSID(trimmed);
        } else if (trimmed.find("freq:") != std::string::npos) {
            frequency = parseFrequency(trimmed);
        } else if (trimmed.find("signal:") != std::string::npos) {
            signal = parseSignalStrength(trimmed);
        }
    }
    
    security = parseSecurity(lines);
    
    NetworkInfo network(bssid, ssid);
    network.setFrequency(frequency)
           .setSignalStrength(signal)
           .setSecurity(security);
    
    return network;
}

MacAddress IwScanParser::parseBSSID(const std::string& line) {
    std::regex bssid_regex("BSS\\s+([0-9a-fA-F:]{17})");
    std::smatch match;
    
    if (std::regex_search(line, match, bssid_regex)) {
        return MacAddress(match[1].str());
    }
    
    return MacAddress("00:00:00:00:00:00");
}

std::string IwScanParser::parseSSID(const std::string& line) {
    size_t pos = line.find("SSID:");
    if (pos != std::string::npos) {
        return trim(line.substr(pos + 5));
    }
    return "";
}

int IwScanParser::parseFrequency(const std::string& line) {
    std::regex freq_regex("freq:\\s*(\\d+)");
    std::smatch match;
    
    if (std::regex_search(line, match, freq_regex)) {
        return std::stoi(match[1].str());
    }
    
    return 0;
}

int IwScanParser::parseSignalStrength(const std::string& line) {
    std::regex signal_regex("signal:\\s*(-?\\d+\\.?\\d*)");
    std::smatch match;
    
    if (std::regex_search(line, match, signal_regex)) {
        return static_cast<int>(std::stod(match[1].str()));
    }
    
    return -100;
}

SecurityType IwScanParser::parseSecurity(const std::vector<std::string>& lines) {
    bool has_rsn = false;
    bool has_wpa = false;
    bool has_wep = false;
    
    for (const auto& line : lines) {
        std::string trimmed = trim(line);
        if (trimmed.find("RSN:") != std::string::npos || trimmed.find("WPA2") != std::string::npos) {
            has_rsn = true;
        }
        if (trimmed.find("WPA:") != std::string::npos) {
            has_wpa = true;
        }
        if (trimmed.find("WEP") != std::string::npos) {
            has_wep = true;
        }
    }
    
    if (has_rsn) return SecurityType::WPA2;
    if (has_wpa) return SecurityType::WPA;
    if (has_wep) return SecurityType::WEP;
    return SecurityType::Open;
}

std::vector<std::string> IwScanParser::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

std::string IwScanParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

}
