
#include "IperfWrapper.hpp"
#include "MetricFormatter.hpp"

IntervalData::IntervalData(const std::string& evt, const json& d) 
    : event(evt), data(d) {
    generateFormattedMetrics();
}

void IntervalData::generateFormattedMetrics() {
    formatted_metrics = json::object();
    
    try {
        // Check if data is nested inside a "data" field (common in interval events)
        const json* actualData = &data;
        if (data.contains("data") && data["data"].is_object()) {
            actualData = &data["data"];
        }
        
        // Format sum metrics if they exist
        if (actualData->contains("sum") && (*actualData)["sum"].is_object()) {
            const auto& sum = (*actualData)["sum"];
            json formatted_sum;
            
            if (sum.contains("bits_per_second")) {
                double bps = sum["bits_per_second"].get<double>();
                formatted_sum["bits_per_second"] = MetricFormatter::formatBitsPerSecond(bps);
            }
            
            if (sum.contains("bytes")) {
                uint64_t bytes = sum["bytes"].get<uint64_t>();
                formatted_sum["bytes"] = MetricFormatter::formatBytes(bytes);
            }
            
            if (sum.contains("seconds")) {
                double seconds = sum["seconds"].get<double>();
                formatted_sum["duration"] = MetricFormatter::formatSeconds(seconds);
            }
            
            if (sum.contains("retransmits")) {
                formatted_sum["retransmits"] = std::to_string(sum["retransmits"].get<int>());
            }
            
            formatted_metrics["sum"] = formatted_sum;
        }
        
        // Format individual stream metrics
        if (actualData->contains("streams") && (*actualData)["streams"].is_array()) {
            json formatted_streams = json::array();
            
            for (const auto& stream : (*actualData)["streams"]) {
                json formatted_stream;
                
                if (stream.contains("bits_per_second")) {
                    double bps = stream["bits_per_second"].get<double>();
                    formatted_stream["bits_per_second"] = MetricFormatter::formatBitsPerSecond(bps);
                }
                
                if (stream.contains("bytes")) {
                    uint64_t bytes = stream["bytes"].get<uint64_t>();
                    formatted_stream["bytes"] = MetricFormatter::formatBytes(bytes);
                }
                
                if (stream.contains("seconds")) {
                    double seconds = stream["seconds"].get<double>();
                    formatted_stream["duration"] = MetricFormatter::formatSeconds(seconds);
                }
                
                if (stream.contains("retransmits")) {
                    formatted_stream["retransmits"] = std::to_string(stream["retransmits"].get<int>());
                }
                
                if (stream.contains("snd_cwnd")) {
                    uint64_t cwnd = stream["snd_cwnd"].get<uint64_t>();
                    formatted_stream["snd_cwnd"] = MetricFormatter::formatBytes(cwnd);
                }
                
                if (stream.contains("rtt")) {
                    int rtt = stream["rtt"].get<int>();
                    formatted_stream["rtt"] = std::to_string(rtt) + " µs";
                }
                
                if (stream.contains("socket")) {
                    formatted_stream["socket"] = std::to_string(stream["socket"].get<int>());
                }
                
                formatted_streams.push_back(formatted_stream);
            }
            
            formatted_metrics["streams"] = formatted_streams;
        }
        
        // Format sum_sent and sum_received if they exist (summary data)
        if (actualData->contains("sum_sent") && (*actualData)["sum_sent"].is_object()) {
            const auto& sum_sent = (*actualData)["sum_sent"];
            json formatted_sum_sent;
            
            if (sum_sent.contains("bits_per_second")) {
                formatted_sum_sent["bits_per_second"] = 
                    MetricFormatter::formatBitsPerSecond(sum_sent["bits_per_second"].get<double>());
            }
            if (sum_sent.contains("bytes")) {
                formatted_sum_sent["bytes"] = 
                    MetricFormatter::formatBytes(sum_sent["bytes"].get<uint64_t>());
            }
            
            formatted_metrics["sum_sent"] = formatted_sum_sent;
        }
        
        if (actualData->contains("sum_received") && (*actualData)["sum_received"].is_object()) {
            const auto& sum_recv = (*actualData)["sum_received"];
            json formatted_sum_recv;
            
            if (sum_recv.contains("bits_per_second")) {
                formatted_sum_recv["bits_per_second"] = 
                    MetricFormatter::formatBitsPerSecond(sum_recv["bits_per_second"].get<double>());
            }
            if (sum_recv.contains("bytes")) {
                formatted_sum_recv["bytes"] = 
                    MetricFormatter::formatBytes(sum_recv["bytes"].get<uint64_t>());
            }
            
            formatted_metrics["sum_received"] = formatted_sum_recv;
        }
        
    } catch (const json::exception& e) {
        // If formatting fails, leave formatted_metrics as empty object
    }
}

