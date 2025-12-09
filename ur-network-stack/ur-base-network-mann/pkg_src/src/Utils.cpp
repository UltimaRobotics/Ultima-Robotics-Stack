#include "../include/Utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <array>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <unistd.h>
#include <sys/types.h>
#include <chrono>

namespace OpenWrtNetwork {

// Initialize static member
bool Utils::verboseMode = false;

// Verbose mode control
void Utils::setVerboseMode(bool enabled) {
    verboseMode = enabled;
}

bool Utils::isVerboseMode() {
    return verboseMode;
}

std::vector<std::string> Utils::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    
    return tokens;
}

std::string Utils::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string Utils::join(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::ostringstream oss;
    
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            oss << delimiter;
        }
        oss << parts[i];
    }
    
    return oss.str();
}

std::string Utils::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool Utils::startsWith(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() && str.substr(0, prefix.length()) == prefix;
}

bool Utils::endsWith(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && str.substr(str.length() - suffix.length()) == suffix;
}

bool Utils::isValidIPv4(const std::string& ip) {
    std::regex ipv4Regex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return std::regex_match(ip, ipv4Regex);
}

bool Utils::isValidIPv6(const std::string& ip) {
    std::regex ipv6Regex("^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::1$|^::$");
    return std::regex_match(ip, ipv6Regex);
}

bool Utils::isValidMac(const std::string& mac) {
    std::regex macRegex("^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$");
    return std::regex_match(mac, macRegex);
}

std::string Utils::calculateNetworkAddress(const std::string& ip, const std::string& subnet) {
    if (!isValidIPv4(ip) || !isValidIPv4(subnet)) {
        return "";
    }
    
    auto ipParts = split(ip, '.');
    auto subnetParts = split(subnet, '.');
    
    std::vector<int> networkParts;
    for (size_t i = 0; i < 4; ++i) {
        int ipOctet = std::stoi(ipParts[i]);
        int subnetOctet = std::stoi(subnetParts[i]);
        networkParts.push_back(ipOctet & subnetOctet);
    }
    
    return std::to_string(networkParts[0]) + "." + 
           std::to_string(networkParts[1]) + "." + 
           std::to_string(networkParts[2]) + "." + 
           std::to_string(networkParts[3]);
}

std::string Utils::calculateBroadcastAddress(const std::string& ip, const std::string& subnet) {
    if (!isValidIPv4(ip) || !isValidIPv4(subnet)) {
        return "";
    }
    
    auto ipParts = split(ip, '.');
    auto subnetParts = split(subnet, '.');
    
    std::vector<int> broadcastParts;
    for (size_t i = 0; i < 4; ++i) {
        int ipOctet = std::stoi(ipParts[i]);
        int subnetOctet = std::stoi(subnetParts[i]);
        broadcastParts.push_back(ipOctet | (~subnetOctet & 0xFF));
    }
    
    return std::to_string(broadcastParts[0]) + "." + 
           std::to_string(broadcastParts[1]) + "." + 
           std::to_string(broadcastParts[2]) + "." + 
           std::to_string(broadcastParts[3]);
}

int Utils::cidrToSubnetMask(int cidr) {
    if (cidr < 0 || cidr > 32) {
        return 0;
    }
    
    return (0xFFFFFFFF << (32 - cidr)) & 0xFFFFFFFF;
}

int Utils::subnetMaskToCidr(const std::string& subnetMask) {
    if (!isValidIPv4(subnetMask)) {
        return -1;
    }
    
    auto parts = split(subnetMask, '.');
    int cidr = 0;
    
    for (const auto& part : parts) {
        int octet = std::stoi(part);
        while (octet > 0) {
            cidr += octet & 1;
            octet >>= 1;
        }
    }
    
    return cidr;
}

bool Utils::fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool Utils::createDirectory(const std::string& path) {
    try {
        if (std::filesystem::create_directories(path)) {
            return true;
        } else {
            // Directory might already exist, check if it's actually a directory
            if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
                return true;
            }
            return false;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error creating directory '" << path << "': " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory '" << path << "': " << e.what() << std::endl;
        return false;
    }
}

std::string Utils::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

bool Utils::writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return file.good();
}

bool Utils::backupFile(const std::string& path) {
    if (!fileExists(path)) {
        return false;
    }
    
    std::string backupPath = path + ".backup." + getTimestamp();
    std::ifstream src(path, std::ios::binary);
    std::ofstream dst(backupPath, std::ios::binary);
    
    if (!src.is_open() || !dst.is_open()) {
        return false;
    }
    
    dst << src.rdbuf();
    return dst.good();
}

bool Utils::restoreFile(const std::string& backupPath, const std::string& targetPath) {
    if (!fileExists(backupPath)) {
        return false;
    }
    
    std::ifstream src(backupPath, std::ios::binary);
    std::ofstream dst(targetPath, std::ios::binary);
    
    if (!src.is_open() || !dst.is_open()) {
        return false;
    }
    
    dst << src.rdbuf();
    return dst.good();
}

std::string Utils::executeCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;
    
    // Print command in verbose mode
    if (verboseMode) {
        std::cout << "[VERBOSE] Executing command: " << command << std::endl;
    }
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        if (verboseMode) {
            std::cout << "[VERBOSE] Command failed to execute (popen returned NULL)" << std::endl;
        }
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    // Trim and print result in verbose mode
    std::string trimmedResult = trim(result);
    if (verboseMode) {
        if (trimmedResult.empty()) {
            std::cout << "[VERBOSE] Command executed successfully (no output)" << std::endl;
        } else {
            std::cout << "[VERBOSE] Command output:" << std::endl;
            std::cout << "[VERBOSE] " << trimmedResult << std::endl;
        }
    }
    
    return trimmedResult;
}

bool Utils::isRoot() {
    return geteuid() == 0;
}

std::string Utils::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&timeT);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string Utils::formatDuration(std::chrono::seconds duration) {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration - hours);
    auto seconds = duration - hours - minutes;
    
    std::ostringstream oss;
    if (hours.count() > 0) {
        oss << hours.count() << "h ";
    }
    if (minutes.count() > 0) {
        oss << minutes.count() << "m ";
    }
    oss << seconds.count() << "s";
    
    return oss.str();
}

std::string Utils::escapeJsonString(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string Utils::formatJson(const std::map<std::string, std::string>& data) {
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (const auto& pair : data) {
        if (!first) {
            oss << ",";
        }
        oss << "\"" << escapeJsonString(pair.first) << "\":\"" << escapeJsonString(pair.second) << "\"";
        first = false;
    }
    
    oss << "}";
    return oss.str();
}

std::map<std::string, std::string> Utils::parseJson(const std::string& json) {
    std::map<std::string, std::string> result;
    
    // Simple JSON parser - this is a basic implementation
    // In production, use a proper JSON library
    std::regex pairRegex("\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
    auto wordsBegin = std::sregex_iterator(json.begin(), json.end(), pairRegex);
    auto wordsEnd = std::sregex_iterator();
    
    for (std::sregex_iterator i = wordsBegin; i != wordsEnd; ++i) {
        std::smatch match = *i;
        result[match[1].str()] = match[2].str();
    }
    
    return result;
}

std::vector<std::string> Utils::splitWithQuotes(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    
    for (char c : str) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == delimiter && !inQuotes) {
            tokens.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        tokens.push_back(trim(current));
    }
    
    return tokens;
}

} // namespace OpenWrtNetwork
