
#include "DNSLookupAPI.hpp"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

DNSConfig parseConfig(const json& j) {
    DNSConfig config;
    
    if (j.contains("hostname")) {
        config.hostname = j["hostname"].get<std::string>();
    }
    
    if (j.contains("query_type")) {
        config.query_type = j["query_type"].get<std::string>();
    }
    
    if (j.contains("nameserver")) {
        config.nameserver = j["nameserver"].get<std::string>();
    }
    
    if (j.contains("timeout_ms")) {
        config.timeout_ms = j["timeout_ms"].get<int>();
    }
    
    if (j.contains("use_tcp")) {
        config.use_tcp = j["use_tcp"].get<bool>();
    }
    
    if (j.contains("export_file_path")) {
        config.export_file_path = j["export_file_path"].get<std::string>();
    }
    
    return config;
}

json resultToJson(const DNSResult& result) {
    json j;
    
    j["hostname"] = result.hostname;
    j["query_type"] = result.query_type;
    j["success"] = result.success;
    j["nameserver"] = result.nameserver;
    j["query_time_ms"] = result.query_time_ms;
    
    if (!result.error_message.empty()) {
        j["error_message"] = result.error_message;
    }
    
    if (result.success) {
        json records_array = json::array();
        for (const auto& record : result.records) {
            json record_obj;
            record_obj["type"] = record.type;
            record_obj["value"] = record.value;
            record_obj["ttl"] = record.ttl;
            records_array.push_back(record_obj);
        }
        j["records"] = records_array;
    }
    
    return j;
}

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -c, --config <file>     Load configuration from JSON file\n"
              << "  -j, --json <string>     Load configuration from JSON string\n"
              << "  -h, --help              Show this help message\n"
              << "  -e, --example           Show example JSON configuration\n\n"
              << "Examples:\n"
              << "  " << program << " --config dns_config.json\n"
              << "  " << program << " --json '{\"hostname\":\"google.com\",\"query_type\":\"A\"}'\n\n";
}

void printExample() {
    std::cout << "\n=== Example JSON Configuration ===\n\n"
              << "{\n"
              << "  \"hostname\": \"google.com\",\n"
              << "  \"query_type\": \"A\",\n"
              << "  \"nameserver\": \"\",\n"
              << "  \"timeout_ms\": 5000,\n"
              << "  \"use_tcp\": false,\n"
              << "  \"export_file_path\": \"dns_export.json\"\n"
              << "}\n\n"
              << "=== Field Descriptions ===\n\n"
              << "REQUIRED FIELDS:\n"
              << "  hostname          : Domain name or IP to lookup\n\n"
              << "OPTIONAL FIELDS:\n"
              << "  query_type        : DNS record type (A, AAAA, MX, NS, TXT, CNAME, SOA, PTR, ANY) (default: A)\n"
              << "  nameserver        : Custom nameserver to use (default: system default)\n"
              << "  timeout_ms        : Timeout in milliseconds (default: 5000)\n"
              << "  use_tcp           : Use TCP instead of UDP (default: false)\n"
              << "  export_file_path  : Path to export results to JSON file (default: none)\n\n"
              << "=== Query Types ===\n\n"
              << "  A      : IPv4 address\n"
              << "  AAAA   : IPv6 address\n"
              << "  MX     : Mail exchange records\n"
              << "  NS     : Name server records\n"
              << "  TXT    : Text records\n"
              << "  CNAME  : Canonical name records\n"
              << "  SOA    : Start of authority records\n"
              << "  PTR    : Pointer records (reverse DNS)\n"
              << "  ANY    : All available records\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string configFile;
    std::string jsonString;
    bool useFile = false;
    bool useString = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-e" || arg == "--example") {
            printExample();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
                useFile = true;
            } else {
                std::cerr << "Error: --config requires a file path\n";
                return 1;
            }
        } else if (arg == "-j" || arg == "--json") {
            if (i + 1 < argc) {
                jsonString = argv[++i];
                useString = true;
            } else {
                std::cerr << "Error: --json requires a JSON string\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (!useFile && !useString) {
        std::cerr << "Error: Either --config or --json must be specified\n";
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        json config_json;
        
        if (useFile) {
            std::ifstream file(configFile);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open config file: " << configFile << "\n";
                return 1;
            }
            file >> config_json;
        } else {
            config_json = json::parse(jsonString);
        }
        
        if (!config_json.contains("hostname")) {
            std::cerr << "Error: Configuration must contain 'hostname' field\n";
            return 1;
        }
        
        DNSConfig config = parseConfig(config_json);
        
        DNSLookupAPI dns;
        dns.setConfig(config);
        
        DNSResult result = dns.execute();
        
        json output = resultToJson(result);
        std::cout << output.dump(2) << std::endl;
        
        return result.success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
