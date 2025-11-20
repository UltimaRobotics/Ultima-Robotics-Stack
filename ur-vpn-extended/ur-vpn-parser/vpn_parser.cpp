
#include "vpn_parser.hpp"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <regex>

namespace vpn_parser {

VPNParser::VPNParser() {}

VPNParser::~VPNParser() {}

long long VPNParser::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string VPNParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> VPNParser::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::map<std::string, std::string> VPNParser::parseKeyValue(const std::string& line) {
    std::map<std::string, std::string> result;
    size_t pos = line.find('=');
    if (pos != std::string::npos) {
        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));
        result[key] = value;
    }
    return result;
}

ProtocolType VPNParser::detectProtocol(const std::string& content) {
    std::string lower_content = content;
    std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
    
    // Check for OpenVPN indicators
    if (lower_content.find("client") != std::string::npos &&
        (lower_content.find("remote ") != std::string::npos || 
         lower_content.find("ca ") != std::string::npos ||
         lower_content.find("cert ") != std::string::npos)) {
        return ProtocolType::OPENVPN;
    }
    
    // Check for IKEv2 indicators
    if (lower_content.find("conn ") != std::string::npos ||
        lower_content.find("keyexchange=ikev2") != std::string::npos ||
        (lower_content.find("ike=") != std::string::npos && 
         lower_content.find("esp=") != std::string::npos)) {
        return ProtocolType::IKEV2;
    }
    
    // Check for WireGuard indicators
    if (lower_content.find("[interface]") != std::string::npos ||
        lower_content.find("[peer]") != std::string::npos ||
        (lower_content.find("privatekey") != std::string::npos && 
         lower_content.find("publickey") != std::string::npos)) {
        return ProtocolType::WIREGUARD;
    }
    
    return ProtocolType::UNKNOWN;
}

bool VPNParser::parseOpenVPN(const std::string& content, ProfileData& profile) {
    std::istringstream stream(content);
    std::string line;
    
    profile.protocol = "OpenVPN";
    
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        
        std::vector<std::string> parts = split(line, ' ');
        if (parts.empty()) continue;
        
        std::string directive = parts[0];
        
        if (directive == "remote" && parts.size() >= 2) {
            profile.server = parts[1];
            if (parts.size() >= 3) {
                try {
                    profile.port = std::stoi(parts[2]);
                } catch (...) {}
            }
            if (parts.size() >= 4) {
                profile.protocol = parts[3]; // tcp or udp
            }
        }
        else if (directive == "proto" && parts.size() >= 2) {
            profile.protocol = parts[1];
        }
        else if (directive == "port" && parts.size() >= 2) {
            try {
                profile.port = std::stoi(parts[1]);
            } catch (...) {}
        }
        else if (directive == "auth-user-pass" && parts.size() >= 2) {
            profile.auth_method = "user-pass";
        }
        else if (directive == "ca" && parts.size() >= 2) {
            profile.ca_cert = parts[1];
        }
        else if (directive == "cert" && parts.size() >= 2) {
            profile.client_cert = parts[1];
        }
        else if (directive == "key" && parts.size() >= 2) {
            profile.client_key = parts[1];
        }
        else if (directive == "tls-auth" && parts.size() >= 2) {
            profile.tls_auth = parts[1];
            profile.auth_method = "TLS";
        }
        else if (directive == "cipher" && parts.size() >= 2) {
            profile.cipher = parts[1];
            profile.encryption = parts[1];
        }
        else if (directive == "remote-cert-tls" && parts.size() >= 2) {
            profile.remote_cert_tls = parts[1];
        }
        else if (directive == "verb" && parts.size() >= 2) {
            profile.verb = parts[1];
        }
        else if (directive == "comp-lzo") {
            profile.comp_lzo = parts.size() >= 2 ? parts[1] : "yes";
        }
    }
    
    // Set default name if not found
    if (profile.name.empty() && !profile.server.empty()) {
        profile.name = profile.server;
    }
    
    return !profile.server.empty();
}

bool VPNParser::parseIKEv2(const std::string& content, ProfileData& profile) {
    std::istringstream stream(content);
    std::string line;
    
    profile.protocol = "IKEv2";
    
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        std::vector<std::string> parts = split(line, ' ');
        if (parts.empty()) continue;
        
        std::string directive = parts[0];
        
        if (directive == "conn" && parts.size() >= 2) {
            profile.conn_name = parts[1];
            profile.name = parts[1];
        }
        else if (line.find('=') != std::string::npos) {
            auto kv = parseKeyValue(line);
            for (const auto& pair : kv) {
                if (pair.first == "left") {
                    profile.left = pair.second;
                }
                else if (pair.first == "right") {
                    profile.right = pair.second;
                    profile.server = pair.second;
                }
                else if (pair.first == "leftauth") {
                    profile.leftauth = pair.second;
                    profile.auth_method = pair.second;
                }
                else if (pair.first == "rightauth") {
                    profile.rightauth = pair.second;
                }
                else if (pair.first == "ike") {
                    profile.ike = pair.second;
                    profile.encryption = pair.second;
                }
                else if (pair.first == "esp") {
                    profile.esp = pair.second;
                }
                else if (pair.first == "keyexchange") {
                    profile.keyexchange = pair.second;
                }
            }
        }
    }
    
    // Extract port from server if present
    if (!profile.server.empty()) {
        size_t colon_pos = profile.server.find(':');
        if (colon_pos != std::string::npos) {
            try {
                profile.port = std::stoi(profile.server.substr(colon_pos + 1));
                profile.server = profile.server.substr(0, colon_pos);
            } catch (...) {}
        } else {
            profile.port = 500; // Default IKEv2 port
        }
    }
    
    return !profile.server.empty() || !profile.conn_name.empty();
}

bool VPNParser::parseWireGuard(const std::string& content, ProfileData& profile) {
    std::istringstream stream(content);
    std::string line;
    std::string current_section;
    
    profile.protocol = "WireGuard";
    
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        // Check for section headers
        if (line[0] == '[' && line[line.length()-1] == ']') {
            current_section = line.substr(1, line.length()-2);
            std::transform(current_section.begin(), current_section.end(), 
                         current_section.begin(), ::tolower);
            continue;
        }
        
        if (line.find('=') != std::string::npos) {
            auto kv = parseKeyValue(line);
            for (const auto& pair : kv) {
                std::string key_lower = pair.first;
                std::transform(key_lower.begin(), key_lower.end(), 
                             key_lower.begin(), ::tolower);
                
                if (current_section == "interface") {
                    if (key_lower == "privatekey") {
                        profile.private_key = pair.second;
                    }
                    else if (key_lower == "address") {
                        profile.address = pair.second;
                    }
                    else if (key_lower == "dns") {
                        profile.dns = pair.second;
                    }
                }
                else if (current_section == "peer") {
                    if (key_lower == "publickey") {
                        profile.peer_public_key = pair.second;
                        profile.public_key = pair.second;
                    }
                    else if (key_lower == "endpoint") {
                        profile.endpoint = pair.second;
                        // Extract server and port from endpoint
                        size_t colon_pos = pair.second.rfind(':');
                        if (colon_pos != std::string::npos) {
                            profile.server = pair.second.substr(0, colon_pos);
                            try {
                                profile.port = std::stoi(pair.second.substr(colon_pos + 1));
                            } catch (...) {}
                        } else {
                            profile.server = pair.second;
                        }
                    }
                    else if (key_lower == "allowedips") {
                        profile.allowed_ips = pair.second;
                    }
                    else if (key_lower == "presharedkey") {
                        profile.preshared_key = pair.second;
                        profile.auth_method = "PSK";
                    }
                    else if (key_lower == "persistentkeepalive") {
                        profile.persistent_keepalive = pair.second;
                    }
                }
            }
        }
    }
    
    // Set default name
    if (profile.name.empty() && !profile.server.empty()) {
        profile.name = profile.server;
    }
    
    // Default encryption for WireGuard
    if (profile.encryption.empty()) {
        profile.encryption = "ChaCha20-Poly1305";
    }
    
    return !profile.server.empty() || !profile.endpoint.empty();
}

ParseResult VPNParser::parse(const std::string& config_content) {
    ParseResult result;
    result.timestamp = getCurrentTimestamp();
    result.parser_type = "VPN Configuration Parser v1.0";
    
    if (config_content.empty()) {
        result.success = false;
        result.error_message = "Empty configuration content";
        result.protocol_detected = "Unknown";
        return result;
    }
    
    ProtocolType protocol = detectProtocol(config_content);
    
    bool parse_success = false;
    
    switch (protocol) {
        case ProtocolType::OPENVPN:
            result.protocol_detected = "OpenVPN";
            parse_success = parseOpenVPN(config_content, result.profile_data);
            break;
            
        case ProtocolType::IKEV2:
            result.protocol_detected = "IKEv2";
            parse_success = parseIKEv2(config_content, result.profile_data);
            break;
            
        case ProtocolType::WIREGUARD:
            result.protocol_detected = "WireGuard";
            parse_success = parseWireGuard(config_content, result.profile_data);
            break;
            
        default:
            result.success = false;
            result.error_message = "Unsupported or unknown VPN protocol";
            result.protocol_detected = "Unknown";
            return result;
    }
    
    if (!parse_success) {
        result.success = false;
        result.error_message = "Failed to parse configuration";
    } else {
        result.success = true;
    }
    
    return result;
}

json VPNParser::toJson(const ParseResult& result) {
    json j;
    j["success"] = result.success;
    j["protocol_detected"] = result.protocol_detected;
    j["timestamp"] = result.timestamp;
    j["parser_type"] = result.parser_type;
    
    json profile;
    profile["name"] = result.profile_data.name;
    profile["server"] = result.profile_data.server;
    profile["protocol"] = result.profile_data.protocol;
    profile["port"] = result.profile_data.port;
    profile["username"] = result.profile_data.username;
    profile["password"] = result.profile_data.password;
    profile["auth_method"] = result.profile_data.auth_method;
    profile["encryption"] = result.profile_data.encryption;
    
    j["profile_data"] = profile;
    
    if (!result.success && !result.error_message.empty()) {
        j["error"] = result.error_message;
    }
    
    return j;
}

} // namespace vpn_parser
