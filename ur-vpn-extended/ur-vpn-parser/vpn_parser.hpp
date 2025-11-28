
#ifndef VPN_PARSER_HPP
#define VPN_PARSER_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include "thirdparty/nlohmann/json.hpp"

using json = nlohmann::json;

namespace vpn_parser {

enum class ProtocolType {
    OPENVPN,
    IKEV2,
    WIREGUARD,
    UNKNOWN
};

struct ProfileData {
    std::string name;
    std::string server;
    std::string protocol;
    int port = 0;
    std::string username;
    std::string password;
    std::string auth_method;
    std::string encryption;
    
    // OpenVPN specific
    std::string ca_cert;
    std::string client_cert;
    std::string client_key;
    std::string tls_auth;
    std::string cipher;
    std::string remote_cert_tls;
    std::string verb;
    std::string comp_lzo;
    
    // IKEv2 specific
    std::string conn_name;
    std::string left;
    std::string right;
    std::string leftauth;
    std::string rightauth;
    std::string ike;
    std::string esp;
    std::string keyexchange;
    
    // WireGuard specific
    std::string private_key;
    std::string public_key;
    std::string endpoint;
    std::string allowed_ips;
    std::string dns;
    std::string address;
    std::string peer_public_key;
    std::string preshared_key;
    std::string persistent_keepalive;
    
    // Full-tunnel detection fields
    bool is_full_tunnel = false;
    bool has_ipv4_full_tunnel = false;
    bool has_ipv6_full_tunnel = false;
    std::string full_tunnel_type; // "wireguard_allowed_ips", "openvpn_redirect_gateway", etc.
};

struct ParseResult {
    bool success = false;
    std::string protocol_detected;
    ProfileData profile_data;
    long long timestamp = 0;
    std::string parser_type;
    std::string error_message;
};

class VPNParser {
public:
    VPNParser();
    ~VPNParser();
    
    ParseResult parse(const std::string& config_content);
    json toJson(const ParseResult& result);
    
    // Full-tunnel detection methods
    bool detectFullTunnel(const std::string& config_content, ProfileData& profile);
    std::string generateSplitTunnelConfig(const std::string& original_config, const ProfileData& profile);
    
private:
    ProtocolType detectProtocol(const std::string& content);
    
    bool parseOpenVPN(const std::string& content, ProfileData& profile);
    bool parseIKEv2(const std::string& content, ProfileData& profile);
    bool parseWireGuard(const std::string& content, ProfileData& profile);
    
    // Full-tunnel detection private methods
    bool detectWireGuardFullTunnel(const std::string& config_content, ProfileData& profile);
    bool detectOpenVPNFullTunnel(const std::string& config_content, ProfileData& profile);
    std::string generateWireGuardSplitTunnelConfig(const std::string& original_config);
    std::string generateOpenVPNSplitTunnelConfig(const std::string& original_config);
    
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::map<std::string, std::string> parseKeyValue(const std::string& line);
    
    long long getCurrentTimestamp();
};

} // namespace vpn_parser

#endif // VPN_PARSER_HPP
