
#ifndef OPERATION_CONFIG_SERIALIZER_HPP
#define OPERATION_CONFIG_SERIALIZER_HPP

#include "json.hpp"
#include "DNSResultSerializer.hpp"
#include "PingResultSerializer.hpp"
#include "TracerouteResultSerializer.hpp"
#include "IperfResultSerializer.hpp"
#include <string>
#include <optional>

using json = nlohmann::json;

namespace NetBench {
namespace Shared {

enum class OperationType {
    DNS,
    PING,
    TRACEROUTE,
    IPERF,
    SERVERS_STATUS,
    UNKNOWN
};

struct ServerFilters {
    std::string keyword;
    std::string continent;
    std::string country;
    std::string site;
    std::string provider;
    std::string host;
    int port;
    std::string min_speed;
    std::string options;
    
    ServerFilters() : port(0) {}
};

struct OperationConfig {
    OperationType operation;
    std::string output_file;
    std::string output_dir;
    std::string servers_list_path;
    
    // Optional configs for each operation type
    std::optional<DNSConfig> dns_config;
    std::optional<PingConfig> ping_config;
    std::optional<TracerouteConfig> traceroute_config;
    std::optional<IperfConfig> iperf_config;
    std::optional<ServerFilters> filters;
    
    OperationConfig() : operation(OperationType::UNKNOWN) {}
};

class OperationConfigSerializer {
public:
    static json serializeFilters(const ServerFilters& filters);
    static ServerFilters deserializeFilters(const json& j);
    
    static json serializeOperationConfig(const OperationConfig& config);
    static OperationConfig deserializeOperationConfig(const json& j);
    
    static std::string operationTypeToString(OperationType type);
    static OperationType stringToOperationType(const std::string& type_str);
    
    static OperationConfig loadFromFile(const std::string& filepath);
    static void saveToFile(const OperationConfig& config, const std::string& filepath);
};

} // namespace Shared
} // namespace NetBench

#endif // OPERATION_CONFIG_SERIALIZER_HPP
