
#ifndef DNS_RESULT_SERIALIZER_HPP
#define DNS_RESULT_SERIALIZER_HPP

#include "json.hpp"
#include <string>
#include <vector>

using json = nlohmann::json;

namespace NetBench {
namespace Shared {

struct DNSRecord {
    std::string type;
    std::string value;
    int ttl;
};

struct DNSResult {
    std::string hostname;
    std::string query_type;
    bool success;
    std::string error_message;
    std::vector<DNSRecord> records;
    std::string nameserver;
    double query_time_ms;
};

struct DNSConfig {
    std::string hostname;
    std::string query_type;
    std::string nameserver;
    int timeout_ms;
    bool use_tcp;
    std::string export_file_path;
};

class DNSResultSerializer {
public:
    static json serializeRecord(const DNSRecord& record);
    static DNSRecord deserializeRecord(const json& j);
    
    static json serializeResult(const DNSResult& result);
    static DNSResult deserializeResult(const json& j);
    
    static json serializeConfig(const DNSConfig& config);
    static DNSConfig deserializeConfig(const json& j);
};

} // namespace Shared
} // namespace NetBench

#endif // DNS_RESULT_SERIALIZER_HPP
