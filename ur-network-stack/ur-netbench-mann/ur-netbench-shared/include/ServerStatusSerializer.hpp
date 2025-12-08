
#ifndef SERVER_STATUS_SERIALIZER_HPP
#define SERVER_STATUS_SERIALIZER_HPP

#include "json.hpp"
#include <string>
#include <vector>

using json = nlohmann::json;

namespace NetBench {
namespace Shared {

enum class ConnectionQuality {
    EXCELLENT,
    GOOD,
    FAIR,
    POOR,
    UNREACHABLE,
    UNKNOWN
};

struct ServerStatusResult {
    std::string server_id;
    std::string server_name;
    std::string server_host;
    std::string server_port;
    std::string continent;
    std::string country;
    std::string site;
    std::string provider;
    ConnectionQuality quality;
    double avg_rtt_ms;
    double packet_loss_percent;
    std::string last_update_time;
    bool is_reachable;
    int consecutive_failures;
    
    ServerStatusResult() 
        : quality(ConnectionQuality::UNKNOWN), 
          avg_rtt_ms(0.0), 
          packet_loss_percent(0.0),
          is_reachable(false),
          consecutive_failures(0) {}
};

struct ServersStatusResults {
    std::vector<ServerStatusResult> servers;
    std::string timestamp;
    int total_servers;
    int reachable_servers;
    int unreachable_servers;
    bool success;
    std::string error_message;
    
    ServersStatusResults() 
        : total_servers(0),
          reachable_servers(0),
          unreachable_servers(0),
          success(false) {}
};

class ServerStatusSerializer {
public:
    static json serializeResult(const ServerStatusResult& result);
    static json serializeResults(const ServersStatusResults& results);
    static ServerStatusResult deserializeResult(const json& j);
    static ServersStatusResults deserializeResults(const json& j);
    static void exportToFile(const ServersStatusResults& results, const std::string& filepath);
    static std::string qualityToString(ConnectionQuality quality);
    static ConnectionQuality stringToQuality(const std::string& quality_str);
};

} // namespace Shared
} // namespace NetBench

#endif // SERVER_STATUS_SERIALIZER_HPP
