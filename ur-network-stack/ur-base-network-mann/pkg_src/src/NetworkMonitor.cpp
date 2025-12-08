#include "../include/NetworkMonitor.h"
#include "../include/Utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <string>
#include <vector>

namespace OpenWrtNetwork {

NetworkMonitor::NetworkMonitor() : monitoring(false) {
    // Set default config
    config.resolvConfPath = "/etc/resolv.conf";
    config.defaultInterface = "eth0";
}

NetworkMonitor::~NetworkMonitor() {
    stopMonitoring();
}

bool NetworkMonitor::initialize(const std::string& interface) {
    interfaceName = interface;
    
    // Check if interface exists
    std::string output = Utils::executeCommand("ip link show " + interfaceName);
    if (output.find("does not exist") != std::string::npos) {
        std::cerr << "Interface " << interfaceName << " does not exist" << std::endl;
        return false;
    }
    
    // Initialize stats
    lastStats = collectStats();
    statsHistory.push_back(lastStats);
    
    return true;
}

bool NetworkMonitor::initialize(const MonitorConfig& monitorConfig, const std::string& interface) {
    config = monitorConfig;
    return initialize(interface);
}

void NetworkMonitor::startMonitoring() {
    if (monitoring.load()) {
        return;
    }
    
    monitoring = true;
    monitorThread = std::make_unique<std::thread>(&NetworkMonitor::monitorLoop, this);
}

void NetworkMonitor::stopMonitoring() {
    if (!monitoring.load()) {
        return;
    }
    
    monitoring = false;
    if (monitorThread && monitorThread->joinable()) {
        monitorThread->join();
    }
}

bool NetworkMonitor::isMonitoring() const {
    return monitoring.load();
}

NetworkStats NetworkMonitor::getCurrentStats() {
    std::lock_guard<std::mutex> lock(statsMutex);
    return lastStats;
}

std::vector<NetworkStats> NetworkMonitor::getHistoricalStats(int minutes) {
    std::lock_guard<std::mutex> lock(statsMutex);
    
    auto cutoff = std::chrono::system_clock::now() - std::chrono::minutes(minutes);
    std::vector<NetworkStats> filtered;
    
    for (const auto& stats : statsHistory) {
        if (stats.timestamp >= cutoff) {
            filtered.push_back(stats);
        }
    }
    
    return filtered;
}

InterfaceInfo NetworkMonitor::getInterfaceInfo() {
    return collectInterfaceInfo();
}

std::string NetworkMonitor::getExternalIP() {
    std::string ip = Utils::executeCommand("curl -s --connect-timeout 5 ifconfig.me");
    if (ip.empty()) {
        ip = Utils::executeCommand("curl -s --connect-timeout 5 ipinfo.io/ip");
    }
    if (ip.empty()) {
        ip = "Unknown";
    }
    return Utils::trim(ip);
}

std::vector<std::string> NetworkMonitor::getDnsServers() {
    std::string output = Utils::executeCommand("cat " + config.resolvConfPath);
    std::vector<std::string> dnsServers;
    
    auto lines = Utils::split(output, '\n');
    for (const auto& line : lines) {
        if (Utils::startsWith(line, "nameserver ")) {
            dnsServers.push_back(Utils::trim(line.substr(11)));
        }
    }
    
    return dnsServers;
}

std::string NetworkMonitor::getGatewayAddress() {
    std::string output = Utils::executeCommand("ip route show default");
    size_t pos = output.find("default via ");
    if (pos != std::string::npos) {
        size_t endPos = output.find(" ", pos + 12);
        return Utils::trim(output.substr(pos + 12, endPos - pos - 12));
    }
    return "Unknown";
}

void NetworkMonitor::onStatsUpdate(std::function<void(const NetworkStats&)> callback) {
    statsCallback = callback;
}

void NetworkMonitor::onConnectionChange(std::function<void(bool)> callback) {
    connectionCallback = callback;
}

void NetworkMonitor::monitorLoop() {
    auto lastConnectionState = false;
    
    while (monitoring.load()) {
        try {
            // Collect current stats
            NetworkStats currentStats = collectStats();
            InterfaceInfo interfaceInfo = collectInterfaceInfo();
            
            // Check for connection state change
            if (interfaceInfo.isConnected != lastConnectionState) {
                lastConnectionState = interfaceInfo.isConnected;
                if (connectionCallback) {
                    connectionCallback(interfaceInfo.isConnected);
                }
            }
            
            // Update stats
            {
                std::lock_guard<std::mutex> lock(statsMutex);
                lastStats = currentStats;
                statsHistory.push_back(currentStats);
                
                // Keep only last 24 hours of data
                auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24);
                statsHistory.erase(
                    std::remove_if(statsHistory.begin(), statsHistory.end(),
                        [cutoff](const NetworkStats& stats) {
                            return stats.timestamp < cutoff;
                        }),
                    statsHistory.end()
                );
            }
            
            // Call callback
            if (statsCallback) {
                statsCallback(currentStats);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error in monitor loop: " << e.what() << std::endl;
        }
        
        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

NetworkStats NetworkMonitor::collectStats() {
    NetworkStats stats;
    stats.timestamp = std::chrono::system_clock::now();
    
    // Get interface statistics from /proc/net/dev
    std::ifstream procFile("/proc/net/dev");
    if (!procFile.is_open()) {
        return stats;
    }
    
    std::string line;
    bool foundInterface = false;
    
    // Skip first two lines (headers)
    std::getline(procFile, line);
    std::getline(procFile, line);
    
    while (std::getline(procFile, line)) {
        line = Utils::trim(line);
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string interface = line.substr(0, colonPos);
            if (interface == interfaceName) {
                foundInterface = true;
                std::string data = line.substr(colonPos + 1);
                std::istringstream iss(data);
                
                long long rxBytes, rxPackets, rxErrs, rxDrop, rxFifo, rxFrame, rxCompressed, rxMulticast;
                long long txBytes, txPackets, txErrs, txDrop, txFifo, txColls, txCarrier, txCompressed;
                
                iss >> rxBytes >> rxPackets >> rxErrs >> rxDrop >> rxFifo >> rxFrame >> rxCompressed >> rxMulticast
                    >> txBytes >> txPackets >> txErrs >> txDrop >> txFifo >> txColls >> txCarrier >> txCompressed;
                
                stats.totalBytesReceived = rxBytes;
                stats.totalBytesSent = txBytes;
                
                // Calculate speed based on difference from last measurement
                if (lastStats.timestamp != std::chrono::system_clock::time_point{}) {
                    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
                        stats.timestamp - lastStats.timestamp);
                    
                    if (timeDiff.count() > 0) {
                        long long rxDiff = rxBytes - lastStats.totalBytesReceived;
                        long long txDiff = txBytes - lastStats.totalBytesSent;
                        
                        stats.downloadSpeed = calculateSpeed(rxDiff, timeDiff);
                        stats.uploadSpeed = calculateSpeed(txDiff, timeDiff);
                    }
                }
                
                break;
            }
        }
    }
    
    if (!foundInterface) {
        stats.downloadSpeed = 0.0;
        stats.uploadSpeed = 0.0;
    }
    
    return stats;
}

InterfaceInfo NetworkMonitor::collectInterfaceInfo() {
    InterfaceInfo info;
    info.name = interfaceName;
    
    // Get interface information using ip command
    std::string output = Utils::executeCommand("ip addr show " + interfaceName);
    
    // Check if interface is up
    info.isUp = output.find("UP") != std::string::npos;
    info.isConnected = info.isUp && output.find("inet ") != std::string::npos;
    
    // Parse MAC address
    size_t macPos = output.find("link/ether ");
    if (macPos != std::string::npos) {
        size_t endPos = output.find("\n", macPos);
        std::string macLine = output.substr(macPos, endPos - macPos);
        size_t spacePos = macLine.find(" ");
        if (spacePos != std::string::npos) {
            info.macAddress = macLine.substr(spacePos + 1, 17);
        }
    }
    
    // Parse IPv4 address
    size_t inetPos = output.find("inet ");
    if (inetPos != std::string::npos) {
        size_t endPos = output.find("\n", inetPos);
        std::string inetLine = output.substr(inetPos, endPos - inetPos);
        size_t spacePos = inetLine.find(" ");
        size_t slashPos = inetLine.find("/");
        if (spacePos != std::string::npos && slashPos != std::string::npos) {
            info.ipv4Address = inetLine.substr(spacePos + 1, slashPos - spacePos - 1);
        }
    }
    
    // Parse IPv6 address
    size_t inet6Pos = output.find("inet6 ");
    if (inet6Pos != std::string::npos) {
        size_t endPos = output.find("\n", inet6Pos);
        std::string inet6Line = output.substr(inet6Pos, endPos - inet6Pos);
        size_t spacePos = inet6Line.find(" ");
        size_t slashPos = inet6Line.find("/");
        if (spacePos != std::string::npos && slashPos != std::string::npos) {
            info.ipv6Address = inet6Line.substr(spacePos + 1, slashPos - spacePos - 1);
        }
    }
    
    // Get MTU
    size_t mtuPos = output.find("mtu ");
    if (mtuPos != std::string::npos) {
        size_t spacePos = output.find(" ", mtuPos + 4);
        if (spacePos != std::string::npos) {
            std::string mtuStr = output.substr(mtuPos + 4, spacePos - mtuPos - 4);
            info.mtu = std::stoi(mtuStr);
        }
    }
    
    // Get link speed using ethtool
    std::string ethtoolOutput = Utils::executeCommand("ethtool " + interfaceName);
    size_t speedPos = ethtoolOutput.find("Speed: ");
    if (speedPos != std::string::npos) {
        size_t endPos = ethtoolOutput.find("\n", speedPos);
        info.linkSpeed = ethtoolOutput.substr(speedPos + 7, endPos - speedPos - 7);
    }
    
    return info;
}

double NetworkMonitor::calculateSpeed(long long bytes, std::chrono::milliseconds timeDiff) {
    if (timeDiff.count() == 0) {
        return 0.0;
    }
    
    double bytesPerSecond = static_cast<double>(bytes) * 1000.0 / timeDiff.count();
    double bitsPerSecond = bytesPerSecond * 8.0;
    return bitsPerSecond / 1000000.0; // Convert to Mbps
}

} // namespace OpenWrtNetwork
