
#ifndef PING_RESULT_SERIALIZER_HPP
#define PING_RESULT_SERIALIZER_HPP

#include "json.hpp"
#include <string>
#include <vector>

using json = nlohmann::json;

namespace NetBench {
namespace Shared {

struct PingRealtimeResult {
    int sequence;
    double rtt_ms;
    int ttl;
    bool success;
    std::string error_message;
};

struct PingResult {
    std::string destination;
    std::string ip_address;
    int packets_sent;
    int packets_received;
    int packets_lost;
    double loss_percentage;
    double min_rtt_ms;
    double max_rtt_ms;
    double avg_rtt_ms;
    double stddev_rtt_ms;
    std::vector<double> rtt_times;
    std::vector<int> sequence_numbers;
    std::vector<int> ttl_values;
    bool success;
    std::string error_message;
};

struct PingConfig {
    std::string destination;
    int count;
    int timeout_ms;
    int interval_ms;
    int packet_size;
    int ttl;
    bool resolve_hostname;
    std::string export_file_path;
};

class PingResultSerializer {
public:
    static json serializeRealtimeResult(const PingRealtimeResult& result);
    static PingRealtimeResult deserializeRealtimeResult(const json& j);
    
    static json serializeResult(const PingResult& result);
    static PingResult deserializeResult(const json& j);
    
    static json serializeConfig(const PingConfig& config);
    static PingConfig deserializeConfig(const json& j);
};

} // namespace Shared
} // namespace NetBench

#endif // PING_RESULT_SERIALIZER_HPP
