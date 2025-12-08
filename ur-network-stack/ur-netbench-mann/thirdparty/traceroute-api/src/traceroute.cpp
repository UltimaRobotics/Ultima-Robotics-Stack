
#include "traceroute.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <iostream>
#include <fstream>

namespace traceroute {

nlohmann::json HopInfo::to_json() const {
    nlohmann::json j;
    j["hop"] = hop_number;
    j["ip"] = ip_address;
    j["hostname"] = hostname;
    j["rtt_ms"] = rtt_ms;
    j["timeout"] = timeout;
    return j;
}

TracerouteConfig TracerouteConfig::from_json(const nlohmann::json& j) {
    TracerouteConfig config;
    config.target = j.value("target", "");
    config.max_hops = j.value("max_hops", 30);
    config.timeout_ms = j.value("timeout_ms", 5000);
    config.packet_size = j.value("packet_size", 60);
    config.num_queries = j.value("num_queries", 3);
    config.export_file_path = j.value("export_file_path", "");
    return config;
}

nlohmann::json TracerouteResult::to_json() const {
    nlohmann::json j;
    j["target"] = target;
    j["resolved_ip"] = resolved_ip;
    j["success"] = success;
    j["error_message"] = error_message;
    
    nlohmann::json hops_array = nlohmann::json::array();
    for (const auto& hop : hops) {
        hops_array.push_back(hop.to_json());
    }
    j["hops"] = hops_array;
    
    return j;
}

Traceroute::Traceroute() {}

Traceroute::~Traceroute() {}

std::string Traceroute::resolve_hostname(const std::string& target) {
    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    if (getaddrinfo(target.c_str(), nullptr, &hints, &res) != 0) {
        return "";
    }
    
    char ip_str[INET_ADDRSTRLEN];
    struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
    inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);
    
    std::string result(ip_str);
    freeaddrinfo(res);
    return result;
}

std::string Traceroute::reverse_dns(const std::string& ip) {
    struct sockaddr_in sa;
    char host[NI_MAXHOST];
    
    std::memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);
    
    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa), host, sizeof(host), nullptr, 0, 0) == 0) {
        return std::string(host);
    }
    
    return ip;
}

int Traceroute::create_raw_socket() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        return -1;
    }
    return sockfd;
}

bool Traceroute::send_probe(int sockfd, const std::string& dest_ip, int ttl, int seq) {
    struct sockaddr_in dest_addr;
    std::memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, dest_ip.c_str(), &dest_addr.sin_addr);
    dest_addr.sin_port = htons(33434 + seq);
    
    // Set TTL
    if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        return false;
    }
    
    // Create UDP packet
    char packet[60];
    std::memset(packet, 0, sizeof(packet));
    
    // Simple UDP data
    struct udphdr* udp = (struct udphdr*)packet;
    udp->source = htons(12345);
    udp->dest = htons(33434 + seq);
    udp->len = htons(sizeof(packet));
    udp->check = 0;
    
    if (sendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
        return false;
    }
    
    return true;
}

HopInfo Traceroute::receive_reply(int sockfd, int timeout_ms) {
    HopInfo hop;
    hop.timeout = false;
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    char buffer[512];
    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&recv_addr, &addr_len);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    if (n < 0) {
        hop.timeout = true;
        hop.ip_address = "*";
        hop.hostname = "*";
        hop.rtt_ms = 0.0;
    } else {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(recv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        hop.ip_address = std::string(ip_str);
        hop.hostname = reverse_dns(hop.ip_address);
        hop.rtt_ms = elapsed.count();
    }
    
    return hop;
}

TracerouteResult Traceroute::execute(const TracerouteConfig& config, HopCallback callback) {
    TracerouteResult result;
    result.target = config.target;
    result.success = false;
    
    // Initialize export file if path is provided
    std::ofstream export_file;
    bool use_export = !config.export_file_path.empty();
    
    if (use_export) {
        export_file.open(config.export_file_path, std::ios::out | std::ios::trunc);
        if (!export_file.is_open()) {
            result.error_message = "Failed to open export file: " + config.export_file_path;
            return result;
        }
        // Initialize with JSON object structure
        export_file << "{\n";
        export_file << "\"trace\": {\n";
        export_file << "  \"target\": \"" << config.target << "\",\n";
        export_file << "  \"max_hops\": " << config.max_hops << ",\n";
        export_file << "  \"timeout_ms\": " << config.timeout_ms << ",\n";
        export_file << "  \"packet_size\": " << config.packet_size << ",\n";
        export_file << "  \"num_queries\": " << config.num_queries << "\n";
        export_file << "},\n";
        export_file << "\"hops\": [\n";
        export_file.flush();
    }
    
    // Resolve target
    std::string dest_ip = resolve_hostname(config.target);
    if (dest_ip.empty()) {
        result.error_message = "Failed to resolve hostname: " + config.target;
        
        if (use_export) {
            export_file << "],\n";
            export_file << "\"summary\": {\n";
            export_file << "  \"resolved_ip\": \"\",\n";
            export_file << "  \"success\": false,\n";
            export_file << "  \"total_hops\": 0,\n";
            export_file << "  \"error\": \"" << result.error_message << "\"\n";
            export_file << "}\n}\n";
            export_file.close();
        }
        
        return result;
    }
    result.resolved_ip = dest_ip;
    
    // Create sockets
    int send_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int recv_sock = create_raw_socket();
    
    if (send_sock < 0 || recv_sock < 0) {
        result.error_message = "Failed to create sockets. Run with sudo/root privileges.";
        
        if (use_export) {
            export_file << "],\n";
            export_file << "\"summary\": {\n";
            export_file << "  \"resolved_ip\": \"" << dest_ip << "\",\n";
            export_file << "  \"success\": false,\n";
            export_file << "  \"total_hops\": 0,\n";
            export_file << "  \"error\": \"" << result.error_message << "\"\n";
            export_file << "}\n}\n";
            export_file.close();
        }
        
        if (send_sock >= 0) close(send_sock);
        if (recv_sock >= 0) close(recv_sock);
        return result;
    }
    
    bool reached_destination = false;
    
    for (int ttl = 1; ttl <= config.max_hops && !reached_destination; ++ttl) {
        HopInfo hop;
        hop.hop_number = ttl;
        hop.timeout = true;
        
        for (int query = 0; query < config.num_queries; ++query) {
            if (!send_probe(send_sock, dest_ip, ttl, query)) {
                continue;
            }
            
            HopInfo query_hop = receive_reply(recv_sock, config.timeout_ms);
            
            if (!query_hop.timeout) {
                hop = query_hop;
                hop.hop_number = ttl;
                
                if (hop.ip_address == dest_ip) {
                    reached_destination = true;
                }
                break;
            }
        }
        
        result.hops.push_back(hop);
        
        // Write hop to export file in real-time
        if (use_export) {
            // Close and reopen to ensure filesystem updates modification time
            export_file.close();
            export_file.open(config.export_file_path, std::ios::out | std::ios::trunc);
            
            // Rewrite entire file with all results so far
            export_file << "{\n";
            export_file << "\"trace\": {\n";
            export_file << "  \"target\": \"" << config.target << "\",\n";
            export_file << "  \"max_hops\": " << config.max_hops << ",\n";
            export_file << "  \"timeout_ms\": " << config.timeout_ms << ",\n";
            export_file << "  \"packet_size\": " << config.packet_size << ",\n";
            export_file << "  \"num_queries\": " << config.num_queries << "\n";
            export_file << "},\n";
            export_file << "\"hops\": [\n";
            
            // Write all hops collected so far
            for (size_t i = 0; i < result.hops.size(); i++) {
                const auto& h = result.hops[i];
                if (i > 0) {
                    export_file << ",\n";
                }
                export_file << "  {\n";
                export_file << "    \"hop\": " << h.hop_number << ",\n";
                export_file << "    \"ip\": \"" << h.ip_address << "\",\n";
                export_file << "    \"hostname\": \"" << h.hostname << "\",\n";
                export_file << "    \"rtt_ms\": " << h.rtt_ms << ",\n";
                export_file << "    \"timeout\": " << (h.timeout ? "true" : "false") << "\n";
                export_file << "  }";
            }
            
            export_file << "\n]\n";
            export_file << "}\n";
            export_file.flush();
        }
        
        // Call callback for real-time reporting
        if (callback) {
            callback(hop);
        }
    }
    
    close(send_sock);
    close(recv_sock);
    
    result.success = reached_destination;
    if (!reached_destination && result.error_message.empty()) {
        result.error_message = "Maximum hops reached without reaching destination";
    }
    
    // Write summary to export file
    if (use_export) {
        export_file << "\n],\n";
        export_file << "\"summary\": {\n";
        export_file << "  \"resolved_ip\": \"" << result.resolved_ip << "\",\n";
        export_file << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
        export_file << "  \"total_hops\": " << result.hops.size();
        
        if (!result.error_message.empty()) {
            export_file << ",\n";
            export_file << "  \"error\": \"" << result.error_message << "\"";
        }
        
        export_file << "\n}\n}\n";
        export_file.close();
    }
    
    return result;
}

} // namespace traceroute

