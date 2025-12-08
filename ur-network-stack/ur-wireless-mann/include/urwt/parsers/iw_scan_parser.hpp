#ifndef URWT_PARSERS_IW_SCAN_PARSER_HPP
#define URWT_PARSERS_IW_SCAN_PARSER_HPP

#include <string>
#include <vector>
#include "../models/network_info.hpp"
#include "../utils/result.hpp"

namespace urwt {

class IwScanParser {
public:
    Result<std::vector<NetworkInfo>, std::string> parse(const std::string& output);

private:
    NetworkInfo parseNetworkBlock(const std::string& block);
    MacAddress parseBSSID(const std::string& line);
    std::string parseSSID(const std::string& line);
    int parseFrequency(const std::string& line);
    int parseSignalStrength(const std::string& line);
    SecurityType parseSecurity(const std::vector<std::string>& lines);
    std::vector<std::string> parseCapabilities(const std::string& line);
    
    std::vector<std::string> splitIntoBlocks(const std::string& output);
    std::vector<std::string> splitLines(const std::string& text);
    std::string trim(const std::string& str);
};

}

#endif
