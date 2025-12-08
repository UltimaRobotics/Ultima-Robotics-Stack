
#include "../include/PingResultSerializer.hpp"

namespace NetBench {
namespace Shared {

json PingResultSerializer::serializeRealtimeResult(const PingRealtimeResult& result) {
    json j;
    j["sequence"] = result.sequence;
    j["rtt_ms"] = result.rtt_ms;
    j["ttl"] = result.ttl;
    j["success"] = result.success;
    j["error_message"] = result.error_message;
    return j;
}

PingRealtimeResult PingResultSerializer::deserializeRealtimeResult(const json& j) {
    PingRealtimeResult result;
    result.sequence = j.value("sequence", 0);
    result.rtt_ms = j.value("rtt_ms", 0.0);
    result.ttl = j.value("ttl", 0);
    result.success = j.value("success", false);
    result.error_message = j.value("error_message", "");
    return result;
}

json PingResultSerializer::serializeResult(const PingResult& result) {
    json j;
    j["destination"] = result.destination;
    j["ip_address"] = result.ip_address;
    j["packets_sent"] = result.packets_sent;
    j["packets_received"] = result.packets_received;
    j["packets_lost"] = result.packets_lost;
    j["loss_percentage"] = result.loss_percentage;
    j["min_rtt_ms"] = result.min_rtt_ms;
    j["max_rtt_ms"] = result.max_rtt_ms;
    j["avg_rtt_ms"] = result.avg_rtt_ms;
    j["stddev_rtt_ms"] = result.stddev_rtt_ms;
    j["success"] = result.success;
    j["error_message"] = result.error_message;
    
    j["rtt_times"] = result.rtt_times;
    j["sequence_numbers"] = result.sequence_numbers;
    j["ttl_values"] = result.ttl_values;
    
    return j;
}

PingResult PingResultSerializer::deserializeResult(const json& j) {
    PingResult result;
    result.destination = j.value("destination", "");
    result.ip_address = j.value("ip_address", "");
    result.packets_sent = j.value("packets_sent", 0);
    result.packets_received = j.value("packets_received", 0);
    result.packets_lost = j.value("packets_lost", 0);
    result.loss_percentage = j.value("loss_percentage", 0.0);
    result.min_rtt_ms = j.value("min_rtt_ms", 0.0);
    result.max_rtt_ms = j.value("max_rtt_ms", 0.0);
    result.avg_rtt_ms = j.value("avg_rtt_ms", 0.0);
    result.stddev_rtt_ms = j.value("stddev_rtt_ms", 0.0);
    result.success = j.value("success", false);
    result.error_message = j.value("error_message", "");
    
    if (j.contains("rtt_times") && j["rtt_times"].is_array()) {
        result.rtt_times = j["rtt_times"].get<std::vector<double>>();
    }
    if (j.contains("sequence_numbers") && j["sequence_numbers"].is_array()) {
        result.sequence_numbers = j["sequence_numbers"].get<std::vector<int>>();
    }
    if (j.contains("ttl_values") && j["ttl_values"].is_array()) {
        result.ttl_values = j["ttl_values"].get<std::vector<int>>();
    }
    
    return result;
}

json PingResultSerializer::serializeConfig(const PingConfig& config) {
    json j;
    j["destination"] = config.destination;
    j["count"] = config.count;
    j["timeout_ms"] = config.timeout_ms;
    j["interval_ms"] = config.interval_ms;
    j["packet_size"] = config.packet_size;
    j["ttl"] = config.ttl;
    j["resolve_hostname"] = config.resolve_hostname;
    j["export_file_path"] = config.export_file_path;
    return j;
}

PingConfig PingResultSerializer::deserializeConfig(const json& j) {
    PingConfig config;
    config.destination = j.value("destination", "");
    config.count = j.value("count", 4);
    config.timeout_ms = j.value("timeout_ms", 1000);
    config.interval_ms = j.value("interval_ms", 1000);
    config.packet_size = j.value("packet_size", 56);
    config.ttl = j.value("ttl", 64);
    config.resolve_hostname = j.value("resolve_hostname", true);
    config.export_file_path = j.value("export_file_path", "");
    return config;
}

} // namespace Shared
} // namespace NetBench
