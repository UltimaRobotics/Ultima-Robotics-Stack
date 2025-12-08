
#ifndef TRACEROUTE_RESULT_SERIALIZER_HPP
#define TRACEROUTE_RESULT_SERIALIZER_HPP

#include "json.hpp"
#include <string>
#include <vector>

using json = nlohmann::json;

namespace NetBench {
namespace Shared {

struct HopInfo {
    int hop_number;
    std::string ip_address;
    std::string hostname;
    double rtt_ms;
    bool timeout;
};

struct TracerouteConfig {
    std::string target;
    int max_hops;
    int timeout_ms;
    int packet_size;
    int num_queries;
    std::string export_file_path;
};

struct TracerouteResult {
    std::string target;
    std::string resolved_ip;
    std::vector<HopInfo> hops;
    bool success;
    std::string error_message;
};

class TracerouteResultSerializer {
public:
    static json serializeHopInfo(const HopInfo& hop);
    static HopInfo deserializeHopInfo(const json& j);
    
    static json serializeResult(const TracerouteResult& result);
    static TracerouteResult deserializeResult(const json& j);
    
    static json serializeConfig(const TracerouteConfig& config);
    static TracerouteConfig deserializeConfig(const json& j);
};

} // namespace Shared
} // namespace NetBench

#endif // TRACEROUTE_RESULT_SERIALIZER_HPP
