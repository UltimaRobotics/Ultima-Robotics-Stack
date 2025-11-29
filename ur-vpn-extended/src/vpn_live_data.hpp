#ifndef VPN_LIVE_DATA_HPP
#define VPN_LIVE_DATA_HPP

#include <string>
#include <chrono>
#include <memory>

// Forward declarations
class VpnRpcClient;
namespace ThreadMgr { class ThreadManager; } // Forward declare ThreadManager to avoid including ThreadManager.hpp
class VPNInstanceManager;

// Include VPN types for enum access
#include "vpn_instance_manager.hpp"

namespace vpn_manager {

/**
 * @brief Unified live data structure for both OpenVPN and WireGuard instances
 */
struct VpnLiveData {
    // Basic instance information
    std::string instance_id;
    std::string instance_name;
    std::string vpn_type;  // "openvpn" or "wireguard"
    std::string status;    // "connected", "disconnected", "connecting", "error"
    
    // Connection metrics
    struct ConnectionMetrics {
        uint64_t session_duration_seconds;
        std::string session_duration_formatted;
        std::chrono::system_clock::time_point session_start_time;
        std::string last_handshake_time;
        uint64_t total_connection_time;
        std::string local_ip;
        std::string remote_endpoint;
        uint64_t latency_ms;
    } connection;
    
    // Data transfer metrics
    struct DataTransferMetrics {
        uint64_t upload_bytes;
        uint64_t download_bytes;
        uint64_t upload_rate_bps;
        uint64_t download_rate_bps;
        std::string upload_formatted;
        std::string download_formatted;
        std::string upload_rate_formatted;
        std::string download_rate_formatted;
        uint64_t total_session_bytes;
        double total_session_mb;
    } data_transfer;
    
    // Protocol-specific metrics
    struct ProtocolSpecificMetrics {
        // WireGuard specific
        std::string peer_public_key;
        std::string allowed_ips;
        std::string interface_name;
        std::string routes_json;
        
        // OpenVPN specific
        std::string cipher;
        std::string auth_method;
        std::string tunnel_protocol;
        std::string compression;
        
        // Common packet metrics
        uint64_t tx_packets;
        uint64_t rx_packets;
        uint64_t tx_dropped;
        uint64_t rx_dropped;
    } protocol;
    
    // Timestamps
    std::chrono::system_clock::time_point last_update_time;
    std::string last_update_timestamp;
    uint64_t update_sequence_number;
    
    // Constructor
    VpnLiveData() {
        // Initialize everything with empty values to avoid null string issues
        instance_id = "";
        instance_name = "";
        vpn_type = "";
        status = "";
        
        connection.session_duration_seconds = 0;
        connection.session_duration_formatted = "";
        connection.session_start_time = std::chrono::system_clock::now();
        connection.last_handshake_time = "";
        connection.total_connection_time = 0;
        connection.local_ip = "";
        connection.remote_endpoint = "";
        connection.latency_ms = 0;
        
        data_transfer.upload_bytes = 0;
        data_transfer.download_bytes = 0;
        data_transfer.upload_rate_bps = 0;
        data_transfer.download_rate_bps = 0;
        data_transfer.upload_formatted = "";
        data_transfer.download_formatted = "";
        data_transfer.upload_rate_formatted = "";
        data_transfer.download_rate_formatted = "";
        data_transfer.total_session_bytes = 0;
        data_transfer.total_session_mb = 0.0;
        
        protocol.peer_public_key = "";
        protocol.allowed_ips = "";
        protocol.interface_name = "";
        protocol.routes_json = "";
        protocol.cipher = "";
        protocol.auth_method = "";
        protocol.tunnel_protocol = "";
        protocol.compression = "";
        protocol.tx_packets = 0;
        protocol.rx_packets = 0;
        protocol.tx_dropped = 0;
        protocol.rx_dropped = 0;
        
        last_update_time = std::chrono::system_clock::now();
        last_update_timestamp = "";
        update_sequence_number = 0;
    }
    
    /**
     * @brief Convert to JSON for publishing
     */
    std::string toJson() const;
    
    /**
     * @brief Update timestamp and sequence number
     */
    void updateTimestamp();
    
    /**
     * @brief Format bytes to human readable string
     */
    static std::string formatBytes(uint64_t bytes);
    
    /**
     * @brief Format time duration to human readable string
     */
    static std::string formatDuration(uint64_t seconds);
    
    /**
     * @brief Format timestamp to ISO string
     */
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
};

/**
 * @brief Live data collector for VPN instances
 * 
 * This class runs as a separate thread and collects live statistics
 * from all VPN instances every second, publishing them via RPC.
 */
class VpnLiveDataCollector {
public:
    /**
     * @brief Constructor
     * @param rpcClient Reference to the RPC client for publishing
     * @param threadManager Reference to ThreadManager for thread control
     * @param instanceManager Reference to VPN instance manager for data collection
     * @param publishIntervalMs Interval between data collections (default 1000ms)
     */
    VpnLiveDataCollector(VpnRpcClient& rpcClient, 
                        ThreadMgr::ThreadManager& threadManager,
                        VPNInstanceManager& instanceManager,
                        uint32_t publishIntervalMs = 1000);
    
    /**
     * @brief Destructor
     */
    ~VpnLiveDataCollector();
    
    /**
     * @brief Start the live data collection thread
     */
    bool start();
    
    /**
     * @brief Stop the live data collection thread
     */
    void stop();
    
    /**
     * @brief Check if collector is running
     */
    bool isRunning() const;
    
    /**
     * @brief Set publish interval
     */
    void setPublishInterval(uint32_t intervalMs);
    
    /**
     * @brief Enable/disable verbose logging
     */
    void setVerbose(bool verbose);
    
private:
    // References to external components
    VpnRpcClient& rpcClient_;
    ThreadMgr::ThreadManager& threadManager_;
    VPNInstanceManager& instanceManager_;
    
    // Thread control
    unsigned int collectorThreadId_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> shouldStop_{false};
    
    // Configuration
    uint32_t publishIntervalMs_;
    bool verbose_;
    
    // Sequence numbering for published data
    std::atomic<uint64_t> sequenceCounter_{0};
    
    /**
     * @brief Main collection thread function
     */
    void collectorThreadFunc();
    
    /**
     * @brief Collect live data from all instances
     */
    std::vector<VpnLiveData> collectLiveData();
    
    /**
     * @brief Collect WireGuard instance data
     */
    VpnLiveData collectWireGuardData(const class VPNInstance& instance);
    
    /**
     * @brief Collect OpenVPN instance data
     */
    VpnLiveData collectOpenVpnData(const class VPNInstance& instance);
    
    /**
     * @brief Publish live data to MQTT topic
     */
    void publishLiveData(const std::vector<VpnLiveData>& data);
    
    /**
     * @brief Get reference to VPN instance manager
     */
    class VPNInstanceManager& getInstanceManager();
};

} // namespace vpn_manager

#endif // VPN_LIVE_DATA_HPP
