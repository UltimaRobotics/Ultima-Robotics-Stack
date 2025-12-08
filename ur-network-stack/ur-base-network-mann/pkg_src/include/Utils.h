#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <map>

namespace OpenWrtNetwork {

class Utils {
public:
    // String utilities
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string trim(const std::string& str);
    static std::string join(const std::vector<std::string>& parts, const std::string& delimiter);
    static std::string toLower(const std::string& str);
    static bool startsWith(const std::string& str, const std::string& prefix);
    static bool endsWith(const std::string& str, const std::string& suffix);
    
    // IP address utilities
    static bool isValidIPv4(const std::string& ip);
    static bool isValidIPv6(const std::string& ip);
    static bool isValidMac(const std::string& mac);
    static std::string calculateNetworkAddress(const std::string& ip, const std::string& subnet);
    static std::string calculateBroadcastAddress(const std::string& ip, const std::string& subnet);
    static int cidrToSubnetMask(int cidr);
    static int subnetMaskToCidr(const std::string& subnetMask);
    
    // File utilities
    static bool fileExists(const std::string& path);
    static bool createDirectory(const std::string& path);
    static std::string readFile(const std::string& path);
    static bool writeFile(const std::string& path, const std::string& content);
    static bool backupFile(const std::string& path);
    static bool restoreFile(const std::string& backupPath, const std::string& targetPath);
    
    // System utilities
    static std::string executeCommand(const std::string& command);
    static bool isRoot();
    static std::string getTimestamp();
    static std::string formatDuration(std::chrono::seconds duration);
    
    // JSON utilities (simple implementation)
    static std::string escapeJsonString(const std::string& str);
    static std::string formatJson(const std::map<std::string, std::string>& data);
    static std::map<std::string, std::string> parseJson(const std::string& json);

private:
    static std::vector<std::string> splitWithQuotes(const std::string& str, char delimiter);
};

} // namespace OpenWrtNetwork
