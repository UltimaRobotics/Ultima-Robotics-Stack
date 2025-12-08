
#include "../include/DNSResultSerializer.hpp"

namespace NetBench {
namespace Shared {

json DNSResultSerializer::serializeRecord(const DNSRecord& record) {
    json j;
    j["type"] = record.type;
    j["value"] = record.value;
    j["ttl"] = record.ttl;
    return j;
}

DNSRecord DNSResultSerializer::deserializeRecord(const json& j) {
    DNSRecord record;
    record.type = j.value("type", "");
    record.value = j.value("value", "");
    record.ttl = j.value("ttl", 0);
    return record;
}

json DNSResultSerializer::serializeResult(const DNSResult& result) {
    json j;
    j["hostname"] = result.hostname;
    j["query_type"] = result.query_type;
    j["success"] = result.success;
    j["error_message"] = result.error_message;
    j["nameserver"] = result.nameserver;
    j["query_time_ms"] = result.query_time_ms;
    
    json records_array = json::array();
    for (const auto& record : result.records) {
        records_array.push_back(serializeRecord(record));
    }
    j["records"] = records_array;
    
    return j;
}

DNSResult DNSResultSerializer::deserializeResult(const json& j) {
    DNSResult result;
    result.hostname = j.value("hostname", "");
    result.query_type = j.value("query_type", "");
    result.success = j.value("success", false);
    result.error_message = j.value("error_message", "");
    result.nameserver = j.value("nameserver", "");
    result.query_time_ms = j.value("query_time_ms", 0.0);
    
    if (j.contains("records") && j["records"].is_array()) {
        for (const auto& record_json : j["records"]) {
            result.records.push_back(deserializeRecord(record_json));
        }
    }
    
    return result;
}

json DNSResultSerializer::serializeConfig(const DNSConfig& config) {
    json j;
    j["hostname"] = config.hostname;
    j["query_type"] = config.query_type;
    j["nameserver"] = config.nameserver;
    j["timeout_ms"] = config.timeout_ms;
    j["use_tcp"] = config.use_tcp;
    j["export_file_path"] = config.export_file_path;
    return j;
}

DNSConfig DNSResultSerializer::deserializeConfig(const json& j) {
    DNSConfig config;
    config.hostname = j.value("hostname", "");
    config.query_type = j.value("query_type", "A");
    config.nameserver = j.value("nameserver", "");
    config.timeout_ms = j.value("timeout_ms", 5000);
    config.use_tcp = j.value("use_tcp", false);
    config.export_file_path = j.value("export_file_path", "");
    return config;
}

} // namespace Shared
} // namespace NetBench
