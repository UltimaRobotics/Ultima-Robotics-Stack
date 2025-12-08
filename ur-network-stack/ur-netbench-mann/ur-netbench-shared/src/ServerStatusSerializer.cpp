
#include "../include/ServerStatusSerializer.hpp"
#include <fstream>
#include <iomanip>
#include <chrono>
#include <sstream>

namespace NetBench {
namespace Shared {

std::string ServerStatusSerializer::qualityToString(ConnectionQuality quality) {
    switch (quality) {
        case ConnectionQuality::EXCELLENT:
            return "EXCELLENT";
        case ConnectionQuality::GOOD:
            return "GOOD";
        case ConnectionQuality::FAIR:
            return "FAIR";
        case ConnectionQuality::POOR:
            return "POOR";
        case ConnectionQuality::UNREACHABLE:
            return "UNREACHABLE";
        case ConnectionQuality::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

ConnectionQuality ServerStatusSerializer::stringToQuality(const std::string& quality_str) {
    if (quality_str == "EXCELLENT") return ConnectionQuality::EXCELLENT;
    if (quality_str == "GOOD") return ConnectionQuality::GOOD;
    if (quality_str == "FAIR") return ConnectionQuality::FAIR;
    if (quality_str == "POOR") return ConnectionQuality::POOR;
    if (quality_str == "UNREACHABLE") return ConnectionQuality::UNREACHABLE;
    return ConnectionQuality::UNKNOWN;
}

json ServerStatusSerializer::serializeResult(const ServerStatusResult& result) {
    json j;
    j["server_id"] = result.server_id;
    j["server_name"] = result.server_name;
    j["server_host"] = result.server_host;
    j["server_port"] = result.server_port;
    j["continent"] = result.continent;
    j["country"] = result.country;
    j["site"] = result.site;
    j["provider"] = result.provider;
    j["quality"] = qualityToString(result.quality);
    j["avg_rtt_ms"] = result.avg_rtt_ms;
    j["packet_loss_percent"] = result.packet_loss_percent;
    j["last_update_time"] = result.last_update_time;
    j["is_reachable"] = result.is_reachable;
    j["consecutive_failures"] = result.consecutive_failures;
    return j;
}

json ServerStatusSerializer::serializeResults(const ServersStatusResults& results) {
    json j;
    j["timestamp"] = results.timestamp;
    j["total_servers"] = results.total_servers;
    j["reachable_servers"] = results.reachable_servers;
    j["unreachable_servers"] = results.unreachable_servers;
    j["success"] = results.success;
    j["error_message"] = results.error_message;
    
    json servers_array = json::array();
    for (const auto& server : results.servers) {
        servers_array.push_back(serializeResult(server));
    }
    j["servers"] = servers_array;
    
    return j;
}

ServerStatusResult ServerStatusSerializer::deserializeResult(const json& j) {
    ServerStatusResult result;
    result.server_id = j.value("server_id", "");
    result.server_name = j.value("server_name", "");
    result.server_host = j.value("server_host", "");
    result.server_port = j.value("server_port", "");
    result.continent = j.value("continent", "");
    result.country = j.value("country", "");
    result.site = j.value("site", "");
    result.provider = j.value("provider", "");
    result.quality = stringToQuality(j.value("quality", "UNKNOWN"));
    result.avg_rtt_ms = j.value("avg_rtt_ms", 0.0);
    result.packet_loss_percent = j.value("packet_loss_percent", 0.0);
    result.last_update_time = j.value("last_update_time", "");
    result.is_reachable = j.value("is_reachable", false);
    result.consecutive_failures = j.value("consecutive_failures", 0);
    return result;
}

ServersStatusResults ServerStatusSerializer::deserializeResults(const json& j) {
    ServersStatusResults results;
    results.timestamp = j.value("timestamp", "");
    results.total_servers = j.value("total_servers", 0);
    results.reachable_servers = j.value("reachable_servers", 0);
    results.unreachable_servers = j.value("unreachable_servers", 0);
    results.success = j.value("success", false);
    results.error_message = j.value("error_message", "");
    
    if (j.contains("servers") && j["servers"].is_array()) {
        for (const auto& server_json : j["servers"]) {
            results.servers.push_back(deserializeResult(server_json));
        }
    }
    
    return results;
}

void ServerStatusSerializer::exportToFile(const ServersStatusResults& results, const std::string& filepath) {
    json j = serializeResults(results);
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << j.dump(2);
        file.close();
    }
}

} // namespace Shared
} // namespace NetBench
