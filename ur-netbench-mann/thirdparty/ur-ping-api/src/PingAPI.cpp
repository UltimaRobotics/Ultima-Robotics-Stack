
#include "PingAPI.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <poll.h>
#include <errno.h>
#include <fstream>
#include <sstream>

PingAPI::PingAPI() : sock_fd_(-1) {
}

PingAPI::~PingAPI() {
    closeSocket();
}

void PingAPI::setConfig(const PingConfig& config) {
    config_ = config;
}

void PingAPI::setRealtimeCallback(RealtimeCallback callback) {
    realtime_callback_ = callback;
}

std::string PingAPI::getLastError() const {
    return last_error_;
}

bool PingAPI::createSocket() {
    sock_fd_ = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock_fd_ < 0) {
        last_error_ = "Failed to create socket. Need root/CAP_NET_RAW privileges: " + std::string(strerror(errno));
        return false;
    }
    
    // Set TTL
    if (setsockopt(sock_fd_, IPPROTO_IP, IP_TTL, &config_.ttl, sizeof(config_.ttl)) < 0) {
        last_error_ = "Failed to set TTL: " + std::string(strerror(errno));
        closeSocket();
        return false;
    }
    
    // Set timeout
    struct timeval tv;
    tv.tv_sec = config_.timeout_ms / 1000;
    tv.tv_usec = (config_.timeout_ms % 1000) * 1000;
    
    if (setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        last_error_ = "Failed to set timeout: " + std::string(strerror(errno));
        closeSocket();
        return false;
    }
    
    return true;
}

void PingAPI::closeSocket() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool PingAPI::resolveHostname(const std::string& hostname, std::string& ip_address) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    
    int status = getaddrinfo(hostname.c_str(), NULL, &hints, &res);
    if (status != 0) {
        last_error_ = "Failed to resolve hostname: " + std::string(gai_strerror(status));
        return false;
    }
    
    struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);
    ip_address = ip_str;
    
    freeaddrinfo(res);
    return true;
}

uint16_t PingAPI::calculateChecksum(uint16_t* buf, int len) {
    uint32_t sum = 0;
    
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(uint8_t*)buf;
    }
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    
    return (uint16_t)(~sum);
}

bool PingAPI::sendPing(int sequence, const std::string& ip_address) {
    struct icmp icmp_hdr;
    char packet[sizeof(struct icmp) + config_.packet_size];
    
    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.icmp_type = ICMP_ECHO;
    icmp_hdr.icmp_code = 0;
    icmp_hdr.icmp_id = getpid() & 0xFFFF;
    icmp_hdr.icmp_seq = sequence;
    
    // Fill data with pattern
    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));
    for (int i = sizeof(icmp_hdr); i < sizeof(packet); i++) {
        packet[i] = i & 0xFF;
    }
    
    // Calculate checksum
    struct icmp* icmp_ptr = (struct icmp*)packet;
    icmp_ptr->icmp_cksum = 0;
    icmp_ptr->icmp_cksum = calculateChecksum((uint16_t*)packet, sizeof(packet));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
    
    ssize_t sent = sendto(sock_fd_, packet, sizeof(packet), 0, 
                          (struct sockaddr*)&addr, sizeof(addr));
    
    if (sent <= 0) {
        last_error_ = "Failed to send packet: " + std::string(strerror(errno));
        return false;
    }
    
    return true;
}

bool PingAPI::receivePing(int sequence, double& rtt_ms, int& ttl) {
    char recv_buf[1024];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    struct pollfd pfd;
    pfd.fd = sock_fd_;
    pfd.events = POLLIN;
    
    int poll_result = poll(&pfd, 1, config_.timeout_ms);
    
    if (poll_result <= 0) {
        last_error_ = "Timeout waiting for reply";
        return false;
    }
    
    ssize_t received = recvfrom(sock_fd_, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr*)&from, &fromlen);
    
    auto end = std::chrono::high_resolution_clock::now();
    
    if (received <= 0) {
        last_error_ = "Failed to receive packet: " + std::string(strerror(errno));
        return false;
    }
    
    // Parse IP header
    struct ip* ip_hdr = (struct ip*)recv_buf;
    int ip_hdr_len = ip_hdr->ip_hl * 4;
    ttl = ip_hdr->ip_ttl;
    
    // Parse ICMP header
    struct icmp* icmp_hdr = (struct icmp*)(recv_buf + ip_hdr_len);
    
    if (icmp_hdr->icmp_type == ICMP_ECHOREPLY && 
        icmp_hdr->icmp_id == (getpid() & 0xFFFF) &&
        icmp_hdr->icmp_seq == sequence) {
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        rtt_ms = duration.count() / 1000.0;
        return true;
    }
    
    last_error_ = "Received invalid ICMP reply";
    return false;
}

double PingAPI::calculateStdDev(const std::vector<double>& values, double mean) {
    if (values.empty()) return 0.0;
    
    double sum = 0.0;
    for (double val : values) {
        sum += (val - mean) * (val - mean);
    }
    
    return std::sqrt(sum / values.size());
}

PingResult PingAPI::execute() {
    PingResult result;
    result.destination = config_.destination;
    result.packets_sent = 0;
    result.packets_received = 0;
    result.packets_lost = 0;
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
        export_file << "\"results\": [\n";
        export_file.flush();
    }
    
    // Resolve hostname
    std::string ip_address;
    if (config_.resolve_hostname) {
        if (!resolveHostname(config_.destination, ip_address)) {
            result.error_message = last_error_;
            if (use_export) {
                export_file << "],\n";
                export_file << "\"summary\": {\n";
                export_file << "  \"packets_sent\": 0,\n";
                export_file << "  \"packets_received\": 0,\n";
                export_file << "  \"packets_lost\": 0,\n";
                export_file << "  \"loss_percentage\": 0.0,\n";
                export_file << "  \"rtt_min_ms\": 0.0,\n";
                export_file << "  \"rtt_max_ms\": 0.0,\n";
                export_file << "  \"rtt_avg_ms\": 0.0,\n";
                export_file << "  \"rtt_stddev_ms\": 0.0,\n";
                export_file << "  \"error\": \"" << last_error_ << "\"\n";
                export_file << "}\n}\n";
                export_file.close();
            }
            return result;
        }
    } else {
        ip_address = config_.destination;
    }
    
    result.ip_address = ip_address;
    
    // Create socket
    if (!createSocket()) {
        result.error_message = last_error_;
        if (use_export) {
            export_file << "],\n";
            export_file << "\"summary\": {\n";
            export_file << "  \"packets_sent\": 0,\n";
            export_file << "  \"packets_received\": 0,\n";
            export_file << "  \"packets_lost\": 0,\n";
            export_file << "  \"loss_percentage\": 0.0,\n";
            export_file << "  \"rtt_min_ms\": 0.0,\n";
            export_file << "  \"rtt_max_ms\": 0.0,\n";
            export_file << "  \"rtt_avg_ms\": 0.0,\n";
            export_file << "  \"rtt_stddev_ms\": 0.0,\n";
            export_file << "  \"error\": \"" << last_error_ << "\"\n";
            export_file << "}\n}\n";
            export_file.close();
        }
        return result;
    }
    
    // Send pings
    for (int i = 0; i < config_.count; i++) {
        result.packets_sent++;
        
        auto send_time = std::chrono::high_resolution_clock::now();
        
        PingRealtimeResult rt_result;
        rt_result.sequence = i;
        rt_result.success = false;
        
        if (sendPing(i, ip_address)) {
            double rtt_ms;
            int ttl;
            
            if (receivePing(i, rtt_ms, ttl)) {
                result.packets_received++;
                result.rtt_times.push_back(rtt_ms);
                result.sequence_numbers.push_back(i);
                result.ttl_values.push_back(ttl);
                
                rt_result.rtt_ms = rtt_ms;
                rt_result.ttl = ttl;
                rt_result.success = true;
            } else {
                rt_result.error_message = last_error_;
            }
        } else {
            rt_result.error_message = last_error_;
        }
        
        // Call real-time callback if set
        if (realtime_callback_) {
            realtime_callback_(rt_result);
        }
        
        // Write to export file in real-time
        if (use_export) {
            // Close and reopen to ensure filesystem updates modification time
            export_file.close();
            export_file.open(config_.export_file_path, std::ios::out | std::ios::trunc);
            
            // Rewrite entire file with all results so far
            export_file << "{\n";
            export_file << "\"results\": [\n";
            
            // Write all results collected so far
            for (int j = 0; j <= i; j++) {
                if (j > 0) {
                    export_file << ",\n";
                }
                export_file << "  {\n";
                export_file << "    \"sequence\": " << j << ",\n";
                
                // Check if this sequence was successful
                bool found = false;
                int result_idx = -1;
                for (size_t k = 0; k < result.sequence_numbers.size(); k++) {
                    if (result.sequence_numbers[k] == j) {
                        found = true;
                        result_idx = k;
                        break;
                    }
                }
                
                if (found && result_idx >= 0) {
                    export_file << "    \"success\": true,\n";
                    export_file << "    \"rtt_ms\": " << result.rtt_times[result_idx] << ",\n";
                    export_file << "    \"ttl\": " << result.ttl_values[result_idx] << "\n";
                } else {
                    export_file << "    \"success\": false,\n";
                    export_file << "    \"error\": \"" << (j == i ? rt_result.error_message : "Timeout or failure") << "\"\n";
                }
                
                export_file << "  }";
            }
            
            export_file << "\n]\n";
            export_file << "}\n";
            export_file.flush();
        }
        
        // Wait for interval
        if (i < config_.count - 1) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - send_time);
            int sleep_time = config_.interval_ms - elapsed.count();
            if (sleep_time > 0) {
                usleep(sleep_time * 1000);
            }
        }
    }
    
    closeSocket();
    
    // Calculate statistics
    result.packets_lost = result.packets_sent - result.packets_received;
    result.loss_percentage = (result.packets_sent > 0) ? 
        (result.packets_lost * 100.0 / result.packets_sent) : 100.0;
    
    if (!result.rtt_times.empty()) {
        result.min_rtt_ms = *std::min_element(result.rtt_times.begin(), result.rtt_times.end());
        result.max_rtt_ms = *std::max_element(result.rtt_times.begin(), result.rtt_times.end());
        
        double sum = 0.0;
        for (double rtt : result.rtt_times) {
            sum += rtt;
        }
        result.avg_rtt_ms = sum / result.rtt_times.size();
        result.stddev_rtt_ms = calculateStdDev(result.rtt_times, result.avg_rtt_ms);
        
        result.success = true;
    } else {
        result.min_rtt_ms = 0.0;
        result.max_rtt_ms = 0.0;
        result.avg_rtt_ms = 0.0;
        result.stddev_rtt_ms = 0.0;
        result.error_message = "No packets received";
    }
    
    // Write summary statistics to export file
    if (use_export) {
        export_file << "\n],\n";
        export_file << "\"summary\": {\n";
        export_file << "  \"packets_sent\": " << result.packets_sent << ",\n";
        export_file << "  \"packets_received\": " << result.packets_received << ",\n";
        export_file << "  \"packets_lost\": " << result.packets_lost << ",\n";
        export_file << "  \"loss_percentage\": " << result.loss_percentage << ",\n";
        export_file << "  \"rtt_min_ms\": " << result.min_rtt_ms << ",\n";
        export_file << "  \"rtt_max_ms\": " << result.max_rtt_ms << ",\n";
        export_file << "  \"rtt_avg_ms\": " << result.avg_rtt_ms << ",\n";
        export_file << "  \"rtt_stddev_ms\": " << result.stddev_rtt_ms << "\n";
        export_file << "}\n";
        export_file << "}\n";
        export_file.close();
    }
    
    return result;
}
