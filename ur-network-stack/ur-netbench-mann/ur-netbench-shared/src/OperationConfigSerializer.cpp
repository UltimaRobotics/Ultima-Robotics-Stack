
#include "../include/OperationConfigSerializer.hpp"
#include <fstream>
#include <stdexcept>

namespace NetBench {
namespace Shared {

std::string OperationConfigSerializer::operationTypeToString(OperationType type) {
    switch (type) {
        case OperationType::DNS:
            return "dns";
        case OperationType::PING:
            return "ping";
        case OperationType::TRACEROUTE:
            return "traceroute";
        case OperationType::IPERF:
            return "iperf";
        case OperationType::SERVERS_STATUS:
            return "servers-status";
        default:
            return "unknown";
    }
}

OperationType OperationConfigSerializer::stringToOperationType(const std::string& type_str) {
    if (type_str == "dns") return OperationType::DNS;
    if (type_str == "ping") return OperationType::PING;
    if (type_str == "traceroute") return OperationType::TRACEROUTE;
    if (type_str == "iperf") return OperationType::IPERF;
    if (type_str == "servers-status") return OperationType::SERVERS_STATUS;
    return OperationType::UNKNOWN;
}

json OperationConfigSerializer::serializeFilters(const ServerFilters& filters) {
    json j;
    j["keyword"] = filters.keyword;
    j["continent"] = filters.continent;
    j["country"] = filters.country;
    j["site"] = filters.site;
    j["provider"] = filters.provider;
    j["host"] = filters.host;
    j["port"] = filters.port;
    j["min_speed"] = filters.min_speed;
    j["options"] = filters.options;
    return j;
}

ServerFilters OperationConfigSerializer::deserializeFilters(const json& j) {
    ServerFilters filters;
    
    if (j.contains("keyword") && j["keyword"].is_string())
        filters.keyword = j["keyword"].get<std::string>();
    if (j.contains("continent") && j["continent"].is_string())
        filters.continent = j["continent"].get<std::string>();
    if (j.contains("country") && j["country"].is_string())
        filters.country = j["country"].get<std::string>();
    if (j.contains("site") && j["site"].is_string())
        filters.site = j["site"].get<std::string>();
    if (j.contains("provider") && j["provider"].is_string())
        filters.provider = j["provider"].get<std::string>();
    if (j.contains("host") && j["host"].is_string())
        filters.host = j["host"].get<std::string>();
    if (j.contains("port") && j["port"].is_number())
        filters.port = j["port"].get<int>();
    if (j.contains("min_speed") && j["min_speed"].is_string())
        filters.min_speed = j["min_speed"].get<std::string>();
    if (j.contains("options") && j["options"].is_string())
        filters.options = j["options"].get<std::string>();
    
    return filters;
}

json OperationConfigSerializer::serializeOperationConfig(const OperationConfig& config) {
    json j;
    
    j["operation"] = operationTypeToString(config.operation);
    
    if (!config.output_file.empty())
        j["output_file"] = config.output_file;
    if (!config.output_dir.empty())
        j["output_dir"] = config.output_dir;
    if (!config.servers_list_path.empty())
        j["servers_list_path"] = config.servers_list_path;
    
    if (config.filters.has_value())
        j["filters"] = serializeFilters(config.filters.value());
    
    if (config.dns_config.has_value())
        j["dns"] = DNSResultSerializer::serializeConfig(config.dns_config.value());
    
    if (config.ping_config.has_value())
        j["ping"] = PingResultSerializer::serializeConfig(config.ping_config.value());
    
    if (config.traceroute_config.has_value())
        j["traceroute"] = TracerouteResultSerializer::serializeConfig(config.traceroute_config.value());
    
    if (config.iperf_config.has_value())
        j["iperf"] = IperfResultSerializer::serializeConfig(config.iperf_config.value());
    
    return j;
}

OperationConfig OperationConfigSerializer::deserializeOperationConfig(const json& j) {
    OperationConfig config;
    
    if (j.contains("operation") && j["operation"].is_string()) {
        config.operation = stringToOperationType(j["operation"].get<std::string>());
    }
    
    if (j.contains("output_file") && j["output_file"].is_string())
        config.output_file = j["output_file"].get<std::string>();
    
    if (j.contains("output_dir") && j["output_dir"].is_string())
        config.output_dir = j["output_dir"].get<std::string>();
    
    if (j.contains("servers_list_path") && j["servers_list_path"].is_string())
        config.servers_list_path = j["servers_list_path"].get<std::string>();
    
    if (j.contains("filters") && j["filters"].is_object())
        config.filters = deserializeFilters(j["filters"]);
    
    if (j.contains("dns") && j["dns"].is_object())
        config.dns_config = DNSResultSerializer::deserializeConfig(j["dns"]);
    
    if (j.contains("ping") && j["ping"].is_object())
        config.ping_config = PingResultSerializer::deserializeConfig(j["ping"]);
    
    if (j.contains("traceroute") && j["traceroute"].is_object())
        config.traceroute_config = TracerouteResultSerializer::deserializeConfig(j["traceroute"]);
    
    if (j.contains("iperf") && j["iperf"].is_object())
        config.iperf_config = IperfResultSerializer::deserializeConfig(j["iperf"]);
    
    return config;
}

OperationConfig OperationConfigSerializer::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filepath);
    }
    
    json j;
    file >> j;
    file.close();
    
    return deserializeOperationConfig(j);
}

void OperationConfigSerializer::saveToFile(const OperationConfig& config, const std::string& filepath) {
    json j = serializeOperationConfig(config);
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    
    file << j.dump(2);
    file.close();
}

} // namespace Shared
} // namespace NetBench
