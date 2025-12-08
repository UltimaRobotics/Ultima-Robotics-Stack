
#ifndef PING_API_HPP
#define PING_API_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <functional>

struct PingRealtimeResult {
    int sequence;
    double rtt_ms;
    int ttl;
    bool success;
    std::string error_message;
};

struct PingResult {
    std::string destination;
    std::string ip_address;
    int packets_sent;
    int packets_received;
    int packets_lost;
    double loss_percentage;
    double min_rtt_ms;
    double max_rtt_ms;
    double avg_rtt_ms;
    double stddev_rtt_ms;
    std::vector<double> rtt_times;
    std::vector<int> sequence_numbers;
    std::vector<int> ttl_values;
    bool success;
    std::string error_message;
};

struct PingConfig {
    std::string destination;
    int count = 4;
    int timeout_ms = 1000;
    int interval_ms = 1000;
    int packet_size = 56;
    int ttl = 64;
    bool resolve_hostname = true;
    std::string export_file_path = "";
};

class PingAPI {
public:
    using RealtimeCallback = std::function<void(const PingRealtimeResult&)>;
    
    PingAPI();
    ~PingAPI();
    
    // Set configuration
    void setConfig(const PingConfig& config);
    
    // Set callback for real-time results
    void setRealtimeCallback(RealtimeCallback callback);
    
    // Execute ping
    PingResult execute();
    
    // Get last error
    std::string getLastError() const;

private:
    PingConfig config_;
    std::string last_error_;
    int sock_fd_;
    RealtimeCallback realtime_callback_;
    
    // Helper functions
    bool createSocket();
    void closeSocket();
    bool resolveHostname(const std::string& hostname, std::string& ip_address);
    bool sendPing(int sequence, const std::string& ip_address);
    bool receivePing(int sequence, double& rtt_ms, int& ttl);
    uint16_t calculateChecksum(uint16_t* buf, int len);
    double calculateStdDev(const std::vector<double>& values, double mean);
};

#endif // PING_API_HPP
