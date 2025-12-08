
#ifndef DNS_LOOKUP_API_HPP
#define DNS_LOOKUP_API_HPP

#include <string>
#include <vector>
#include <cstdint>

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
    std::string query_type = "A";  // A, AAAA, MX, NS, TXT, CNAME, SOA, PTR, ANY
    std::string nameserver = "";   // Empty means use system default
    int timeout_ms = 5000;
    bool use_tcp = false;
    std::string export_file_path = "";  // Path to export results in real-time
};

class DNSLookupAPI {
public:
    DNSLookupAPI();
    ~DNSLookupAPI();
    
    void setConfig(const DNSConfig& config);
    DNSResult execute();
    std::string getLastError() const;

private:
    DNSConfig config_;
    std::string last_error_;
    
    bool performLookup(DNSResult& result);
    std::string getQueryTypeName(int type);
    int getQueryTypeValue(const std::string& type);
};

#endif // DNS_LOOKUP_API_HPP
