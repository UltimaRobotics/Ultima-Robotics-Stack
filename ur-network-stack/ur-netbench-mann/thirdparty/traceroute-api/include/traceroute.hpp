
#ifndef TRACEROUTE_HPP
#define TRACEROUTE_HPP

#include <string>
#include <vector>
#include <functional>
#include "json.hpp"

namespace traceroute {

struct HopInfo {
    int hop_number;
    std::string ip_address;
    std::string hostname;
    double rtt_ms;
    bool timeout;
    
    nlohmann::json to_json() const;
};

struct TracerouteConfig {
    std::string target;
    int max_hops;
    int timeout_ms;
    int packet_size;
    int num_queries;
    std::string export_file_path;
    
    static TracerouteConfig from_json(const nlohmann::json& j);
};

struct TracerouteResult {
    std::string target;
    std::string resolved_ip;
    std::vector<HopInfo> hops;
    bool success;
    std::string error_message;
    
    nlohmann::json to_json() const;
};

class Traceroute {
public:
    using HopCallback = std::function<void(const HopInfo&)>;
    
    Traceroute();
    ~Traceroute();
    
    TracerouteResult execute(const TracerouteConfig& config, HopCallback callback = nullptr);
    
private:
    int create_raw_socket();
    bool send_probe(int sockfd, const std::string& dest_ip, int ttl, int seq);
    HopInfo receive_reply(int sockfd, int timeout_ms);
    std::string resolve_hostname(const std::string& target);
    std::string reverse_dns(const std::string& ip);
};

} // namespace traceroute

#endif // TRACEROUTE_HPP

