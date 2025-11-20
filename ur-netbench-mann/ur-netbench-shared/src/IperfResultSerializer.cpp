
#include "../include/IperfResultSerializer.hpp"

using json = nlohmann::json;

namespace NetBench {
namespace Shared {

json IperfResultSerializer::serializeIntervalData(const IperfIntervalData& interval) {
    json j;
    j["event"] = interval.event;
    j["data"] = interval.data;
    if (!interval.formatted_metrics.is_null()) {
        j["formatted_metrics"] = interval.formatted_metrics;
    }
    return j;
}

IperfIntervalData IperfResultSerializer::deserializeIntervalData(const json& j) {
    IperfIntervalData interval;
    interval.event = j.value("event", "");
    interval.data = j.value("data", json::object());
    if (j.contains("formatted_metrics")) {
        interval.formatted_metrics = j["formatted_metrics"];
    }
    return interval;
}

json IperfResultSerializer::serializeTestResults(const IperfTestResults& results) {
    json j;
    j["test_start_time"] = results.test_start_time;
    j["test_end_time"] = results.test_end_time;
    j["has_summary"] = results.has_summary;
    
    if (results.has_summary) {
        j["summary"] = results.summary;
    }
    
    json intervals_array = json::array();
    for (const auto& interval : results.intervals) {
        intervals_array.push_back(serializeIntervalData(interval));
    }
    j["intervals"] = intervals_array;
    
    return j;
}

IperfTestResults IperfResultSerializer::deserializeTestResults(const json& j) {
    IperfTestResults results;
    results.test_start_time = j.value("test_start_time", 0);
    results.test_end_time = j.value("test_end_time", 0);
    results.has_summary = j.value("has_summary", false);
    
    if (j.contains("summary")) {
        results.summary = j["summary"];
    }
    
    if (j.contains("intervals") && j["intervals"].is_array()) {
        for (const auto& interval_json : j["intervals"]) {
            results.intervals.push_back(deserializeIntervalData(interval_json));
        }
    }
    
    return results;
}

json IperfResultSerializer::serializeConfig(const IperfConfig& config) {
    json j;
    j["role"] = config.role;
    j["server_hostname"] = config.server_hostname;
    j["port"] = config.port;
    j["duration"] = config.duration;
    j["protocol"] = config.protocol;
    j["interval"] = config.interval;
    j["json"] = config.json_output;
    j["realtime"] = config.realtime;
    j["export_results"] = config.export_results;
    j["log_results"] = config.log_results;
    j["options"] = config.options;
    return j;
}

IperfConfig IperfResultSerializer::deserializeConfig(const json& j) {
    IperfConfig config;
    config.role = j.value("role", "client");
    config.server_hostname = j.value("server_hostname", "");
    config.port = j.value("port", 5201);
    config.duration = j.value("duration", 10);
    config.protocol = j.value("protocol", "tcp");
    config.interval = j.value("interval", 1.0);
    config.json_output = j.value("json", true);
    config.realtime = j.value("realtime", false);
    config.export_results = j.value("export_results", "");
    config.log_results = j.value("log_results", "");
    config.options = j.value("options", "");
    return config;
}

} // namespace Shared
} // namespace NetBench
