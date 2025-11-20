
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include "vpn_parser.hpp"
#include "thirdparty/nlohmann/json.hpp"

using json = nlohmann::json;

void print_help() {
    std::cout << "VPN Configuration Parser v1.0\n\n";
    std::cout << "Usage:\n";
    std::cout << "  vpn-parser -c <config_file>    Parse VPN config directly from file\n";
    std::cout << "  vpn-parser -j <json_string>    Parse VPN config from JSON string\n";
    std::cout << "  vpn-parser -h                  Show this help message\n";
    std::cout << "  vpn-parser                     Read JSON from stdin\n\n";
    std::cout << "JSON format:\n";
    std::cout << "  {\n";
    std::cout << "    \"config_content\": \"<VPN configuration content>\"\n";
    std::cout << "  }\n\n";
    std::cout << "Supported VPN types: OpenVPN, IKEv2, WireGuard\n";
}

std::string read_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    try {
        std::string config_content;
        
        // Parse command-line arguments
        if (argc > 1) {
            std::string flag = argv[1];
            
            if (flag == "-h" || flag == "--help") {
                print_help();
                return 0;
            }
            else if (flag == "-c" || flag == "--config") {
                if (argc < 3) {
                    json error_response;
                    error_response["success"] = false;
                    error_response["error"] = "Missing file path after -c flag";
                    error_response["protocol_detected"] = "Unknown";
                    std::cout << error_response.dump(2) << std::endl;
                    return 1;
                }
                
                std::string filepath = argv[2];
                config_content = read_file(filepath);
            }
            else if (flag == "-j" || flag == "--json") {
                if (argc < 3) {
                    json error_response;
                    error_response["success"] = false;
                    error_response["error"] = "Missing JSON string after -j flag";
                    error_response["protocol_detected"] = "Unknown";
                    std::cout << error_response.dump(2) << std::endl;
                    return 1;
                }
                
                std::string input_json_str = argv[2];
                
                // Parse input JSON
                json input_json;
                try {
                    input_json = json::parse(input_json_str);
                } catch (const json::parse_error& e) {
                    json error_response;
                    error_response["success"] = false;
                    error_response["error"] = std::string("JSON parse error: ") + e.what();
                    error_response["protocol_detected"] = "Unknown";
                    std::cout << error_response.dump(2) << std::endl;
                    return 1;
                }
                
                // Extract config_content
                if (!input_json.contains("config_content")) {
                    json error_response;
                    error_response["success"] = false;
                    error_response["error"] = "Missing 'config_content' field in input JSON";
                    error_response["protocol_detected"] = "Unknown";
                    std::cout << error_response.dump(2) << std::endl;
                    return 1;
                }
                
                config_content = input_json["config_content"].get<std::string>();
            }
            else {
                // Legacy mode: treat first argument as JSON string
                std::string input_json_str = argv[1];
                
                json input_json;
                try {
                    input_json = json::parse(input_json_str);
                } catch (const json::parse_error& e) {
                    json error_response;
                    error_response["success"] = false;
                    error_response["error"] = std::string("JSON parse error: ") + e.what() + "\nUse -h for help";
                    error_response["protocol_detected"] = "Unknown";
                    std::cout << error_response.dump(2) << std::endl;
                    return 1;
                }
                
                if (!input_json.contains("config_content")) {
                    json error_response;
                    error_response["success"] = false;
                    error_response["error"] = "Missing 'config_content' field in input JSON. Use -h for help";
                    error_response["protocol_detected"] = "Unknown";
                    std::cout << error_response.dump(2) << std::endl;
                    return 1;
                }
                
                config_content = input_json["config_content"].get<std::string>();
            }
        } else {
            // Read JSON from stdin
            std::stringstream buffer;
            buffer << std::cin.rdbuf();
            std::string input_json_str = buffer.str();
            
            if (input_json_str.empty()) {
                json error_response;
                error_response["success"] = false;
                error_response["error"] = "No input provided. Use -h for help";
                error_response["protocol_detected"] = "Unknown";
                std::cout << error_response.dump(2) << std::endl;
                return 1;
            }
            
            json input_json;
            try {
                input_json = json::parse(input_json_str);
            } catch (const json::parse_error& e) {
                json error_response;
                error_response["success"] = false;
                error_response["error"] = std::string("JSON parse error: ") + e.what();
                error_response["protocol_detected"] = "Unknown";
                std::cout << error_response.dump(2) << std::endl;
                return 1;
            }
            
            if (!input_json.contains("config_content")) {
                json error_response;
                error_response["success"] = false;
                error_response["error"] = "Missing 'config_content' field in input JSON";
                error_response["protocol_detected"] = "Unknown";
                std::cout << error_response.dump(2) << std::endl;
                return 1;
            }
            
            config_content = input_json["config_content"].get<std::string>();
        }
        
        // Create parser and parse configuration
        vpn_parser::VPNParser parser;
        vpn_parser::ParseResult result = parser.parse(config_content);
        
        // Convert result to JSON and output
        json output = parser.toJson(result);
        std::cout << output.dump(2) << std::endl;
        
        return result.success ? 0 : 1;
        
    } catch (const std::exception& e) {
        json error_response;
        error_response["success"] = false;
        error_response["error"] = std::string("Exception: ") + e.what();
        error_response["protocol_detected"] = "Unknown";
        std::cout << error_response.dump(2) << std::endl;
        return 1;
    }
}
