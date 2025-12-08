
#include "DNSLookupAPI.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <resolv.h>
#include <cstring>
#include <chrono>
#include <sstream>
#include <fstream>

DNSLookupAPI::DNSLookupAPI() {
}

DNSLookupAPI::~DNSLookupAPI() {
}

void DNSLookupAPI::setConfig(const DNSConfig& config) {
    config_ = config;
}

std::string DNSLookupAPI::getLastError() const {
    return last_error_;
}

int DNSLookupAPI::getQueryTypeValue(const std::string& type) {
    if (type == "A") return 1;
    if (type == "AAAA") return 28;
    if (type == "MX") return 15;
    if (type == "NS") return 2;
    if (type == "TXT") return 16;
    if (type == "CNAME") return 5;
    if (type == "SOA") return 6;
    if (type == "PTR") return 12;
    if (type == "ANY") return 255;
    return 1; // Default to A
}

std::string DNSLookupAPI::getQueryTypeName(int type) {
    switch(type) {
        case 1: return "A";
        case 28: return "AAAA";
        case 15: return "MX";
        case 2: return "NS";
        case 16: return "TXT";
        case 5: return "CNAME";
        case 6: return "SOA";
        case 12: return "PTR";
        case 255: return "ANY";
        default: return "UNKNOWN";
    }
}

bool DNSLookupAPI::performLookup(DNSResult& result) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    
    // Set hints based on query type
    if (config_.query_type == "A") {
        hints.ai_family = AF_INET;
    } else if (config_.query_type == "AAAA") {
        hints.ai_family = AF_INET6;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    
    hints.ai_socktype = SOCK_STREAM;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int status = getaddrinfo(config_.hostname.c_str(), NULL, &hints, &res);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    result.query_time_ms = duration.count() / 1000.0;
    
    if (status != 0) {
        last_error_ = gai_strerror(status);
        return false;
    }
    
    // Process results
    for (p = res; p != NULL; p = p->ai_next) {
        DNSRecord record;
        record.ttl = 0; // getaddrinfo doesn't provide TTL
        
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, INET_ADDRSTRLEN);
            record.type = "A";
            record.value = ipstr;
            result.records.push_back(record);
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            char ipstr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipstr, INET6_ADDRSTRLEN);
            record.type = "AAAA";
            record.value = ipstr;
            result.records.push_back(record);
        }
    }
    
    freeaddrinfo(res);
    
    // Get nameserver info
    res_init();
    if (_res.nscount > 0) {
        char ns[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(_res.nsaddr_list[0].sin_addr), ns, INET_ADDRSTRLEN);
        result.nameserver = ns;
    }
    
    return true;
}

DNSResult DNSLookupAPI::execute() {
    DNSResult result;
    result.hostname = config_.hostname;
    result.query_type = config_.query_type;
    result.success = false;
    
    // Initialize export file if path is provided
    std::ofstream export_file;
    bool use_export = !config_.export_file_path.empty();
    
    if (use_export) {
        export_file.open(config_.export_file_path, std::ios::out | std::ios::trunc);
        if (!export_file.is_open()) {
            last_error_ = "Failed to open export file: " + config_.export_file_path;
            result.error_message = last_error_;
            return result;
        }
        // Initialize with JSON object structure
        export_file << "{\n";
        export_file << "\"query\": {\n";
        export_file << "  \"hostname\": \"" << config_.hostname << "\",\n";
        export_file << "  \"query_type\": \"" << config_.query_type << "\",\n";
        export_file << "  \"nameserver\": \"" << config_.nameserver << "\",\n";
        export_file << "  \"timeout_ms\": " << config_.timeout_ms << ",\n";
        export_file << "  \"use_tcp\": " << (config_.use_tcp ? "true" : "false") << "\n";
        export_file << "},\n";
        export_file << "\"records\": [\n";
        export_file.flush();
    }
    
    if (config_.hostname.empty()) {
        result.error_message = "Hostname cannot be empty";
        last_error_ = result.error_message;
        
        if (use_export) {
            export_file << "],\n";
            export_file << "\"summary\": {\n";
            export_file << "  \"success\": false,\n";
            export_file << "  \"total_records\": 0,\n";
            export_file << "  \"query_time_ms\": 0.0,\n";
            export_file << "  \"error\": \"" << result.error_message << "\"\n";
            export_file << "}\n}\n";
            export_file.close();
        }
        
        return result;
    }
    
    if (performLookup(result)) {
        result.success = true;
        
        // Write records to export file in real-time
        if (use_export) {
            for (size_t i = 0; i < result.records.size(); i++) {
                const auto& record = result.records[i];
                if (i > 0) {
                    export_file << ",\n";
                }
                export_file << "  {\n";
                export_file << "    \"type\": \"" << record.type << "\",\n";
                export_file << "    \"value\": \"" << record.value << "\",\n";
                export_file << "    \"ttl\": " << record.ttl << "\n";
                export_file << "  }";
                export_file.flush();
            }
        }
    } else {
        result.error_message = last_error_;
    }
    
    // Write summary to export file
    if (use_export) {
        export_file << "\n],\n";
        export_file << "\"summary\": {\n";
        export_file << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
        export_file << "  \"total_records\": " << result.records.size() << ",\n";
        export_file << "  \"query_time_ms\": " << result.query_time_ms << ",\n";
        export_file << "  \"nameserver\": \"" << result.nameserver << "\"";
        
        if (!result.error_message.empty()) {
            export_file << ",\n";
            export_file << "  \"error\": \"" << result.error_message << "\"";
        }
        
        export_file << "\n}\n}\n";
        export_file.close();
    }
    
    return result;
}
