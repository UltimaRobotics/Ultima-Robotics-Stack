
#include "../include/TracerouteResultSerializer.hpp"

namespace NetBench {
namespace Shared {

json TracerouteResultSerializer::serializeHopInfo(const HopInfo& hop) {
    json j;
    j["hop_number"] = hop.hop_number;
    j["ip_address"] = hop.ip_address;
    j["hostname"] = hop.hostname;
    j["rtt_ms"] = hop.rtt_ms;
    j["timeout"] = hop.timeout;
    return j;
}

HopInfo TracerouteResultSerializer::deserializeHopInfo(const json& j) {
    HopInfo hop;
    hop.hop_number = j.value("hop_number", 0);
    hop.ip_address = j.value("ip_address", "");
    hop.hostname = j.value("hostname", "");
    hop.rtt_ms = j.value("rtt_ms", 0.0);
    hop.timeout = j.value("timeout", false);
    return hop;
}

json TracerouteResultSerializer::serializeResult(const TracerouteResult& result) {
    json j;
    j["target"] = result.target;
    j["resolved_ip"] = result.resolved_ip;
    j["success"] = result.success;
    j["error_message"] = result.error_message;
    
    json hops_array = json::array();
    for (const auto& hop : result.hops) {
        hops_array.push_back(serializeHopInfo(hop));
    }
    j["hops"] = hops_array;
    
    return j;
}

TracerouteResult TracerouteResultSerializer::deserializeResult(const json& j) {
    TracerouteResult result;
    result.target = j.value("target", "");
    result.resolved_ip = j.value("resolved_ip", "");
    result.success = j.value("success", false);
    result.error_message = j.value("error_message", "");
    
    if (j.contains("hops") && j["hops"].is_array()) {
        for (const auto& hop_json : j["hops"]) {
            result.hops.push_back(deserializeHopInfo(hop_json));
        }
    }
    
    return result;
}

json TracerouteResultSerializer::serializeConfig(const TracerouteConfig& config) {
    json j;
    j["target"] = config.target;
    j["max_hops"] = config.max_hops;
    j["timeout_ms"] = config.timeout_ms;
    j["packet_size"] = config.packet_size;
    j["num_queries"] = config.num_queries;
    j["export_file_path"] = config.export_file_path;
    return j;
}

TracerouteConfig TracerouteResultSerializer::deserializeConfig(const json& j) {
    TracerouteConfig config;
    config.target = j.value("target", "");
    config.max_hops = j.value("max_hops", 30);
    config.timeout_ms = j.value("timeout_ms", 5000);
    config.packet_size = j.value("packet_size", 60);
    config.num_queries = j.value("num_queries", 3);
    config.export_file_path = j.value("export_file_path", "");
    return config;
}

} // namespace Shared
} // namespace NetBench
