
#ifndef IPERF_RESULT_SERIALIZER_HPP
#define IPERF_RESULT_SERIALIZER_HPP

#include "json.hpp"
#include <string>
#include <vector>
#include <cstdint>

using json = nlohmann::json;

namespace NetBench {
namespace Shared {

struct IperfIntervalData {
    std::string event;
    json data;
    json formatted_metrics;
};

struct IperfTestResults {
    time_t test_start_time;
    std::vector<IperfIntervalData> intervals;
    time_t test_end_time;
    json summary;
    bool has_summary;
};

struct IperfConfig {
    std::string role;
    std::string server_hostname;
    int port;
    int duration;
    std::string protocol;
    double interval;
    bool json_output;
    bool realtime;
    std::string export_results;
    std::string log_results;
    std::string options;
};

class IperfResultSerializer {
public:
    static json serializeIntervalData(const IperfIntervalData& interval);
    static IperfIntervalData deserializeIntervalData(const json& j);
    
    static json serializeTestResults(const IperfTestResults& results);
    static IperfTestResults deserializeTestResults(const json& j);
    
    static json serializeConfig(const IperfConfig& config);
    static IperfConfig deserializeConfig(const json& j);
};

} // namespace Shared
} // namespace NetBench

#endif // IPERF_RESULT_SERIALIZER_HPP
