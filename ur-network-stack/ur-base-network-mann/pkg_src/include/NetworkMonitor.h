#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace OpenWrtNetwork {

struct NetworkStats {
    double downloadSpeed; // Mbps
    double uploadSpeed; // Mbps
    long long totalBytesReceived;
    long long totalBytesSent;
    std::chrono::system_clock::time_point timestamp;
};

struct InterfaceInfo {
    std::string name;
    std::string macAddress;
    std::string ipv4Address;
    std::string ipv6Address;
    std::string linkSpeed;
    bool isUp;
    bool isConnected;
    int mtu;
};

struct MonitorConfig {
    std::string resolvConfPath;
    std::string defaultInterface;
};

class NetworkMonitor {
public:
    NetworkMonitor();
    ~NetworkMonitor();

    bool initialize(const std::string& interface = "eth0");
    bool initialize(const MonitorConfig& config, const std::string& interface = "eth0");
    
    // Real-time monitoring
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Data collection
    NetworkStats getCurrentStats();
    std::vector<NetworkStats> getHistoricalStats(int minutes = 60);
    InterfaceInfo getInterfaceInfo();
    std::string getExternalIP();
    std::vector<std::string> getDnsServers();
    std::string getGatewayAddress();
    
    // Callback registration
    void onStatsUpdate(std::function<void(const NetworkStats&)> callback);
    void onConnectionChange(std::function<void(bool)> callback);

private:
    std::string interfaceName;
    MonitorConfig config;
    std::atomic<bool> monitoring;
    std::unique_ptr<std::thread> monitorThread;
    
    NetworkStats lastStats;
    std::vector<NetworkStats> statsHistory;
    std::mutex statsMutex;
    
    // Callbacks
    std::function<void(const NetworkStats&)> statsCallback;
    std::function<void(bool)> connectionCallback;
    
    void monitorLoop();
    NetworkStats collectStats();
    InterfaceInfo collectInterfaceInfo();
    std::string executeCommand(const std::string& command);
    long long parseInterfaceCounter(const std::string& counter);
    double calculateSpeed(long long bytes, std::chrono::milliseconds timeDiff);
};

} // namespace OpenWrtNetwork
