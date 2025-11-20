#ifndef WIREGUARD_WRAPPER_HPP
#define WIREGUARD_WRAPPER_HPP

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <future>
#include <nlohmann/json.hpp>
#include "ThreadManager.hpp"
#include "wireguard_c_bridge.h"

using json = nlohmann::json;

namespace wireguard {

enum class ConnectionState {
    INITIAL = WG_STATE_INITIAL,
    CONFIGURING = WG_STATE_CONFIGURING,
    HANDSHAKING = WG_STATE_HANDSHAKING,
    CONNECTED = WG_STATE_CONNECTED,
    RECONNECTING = WG_STATE_RECONNECTING,
    DISCONNECTED = WG_STATE_DISCONNECTED,
    ERROR_STATE = WG_STATE_ERROR
};

struct VPNStats {
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t tx_packets = 0;
    uint64_t rx_packets = 0;
    time_t last_handshake = 0;
    uint32_t latency_ms = 0;
    std::string endpoint;
    std::string allowed_ips;
    std::string peer_public_key;
    std::string local_ip;
    int connected_duration = 0;
    std::string interface_name;
    std::string routes;  // JSON array of route objects

    // Real-time bandwidth rates (bytes per second)
    uint64_t upload_rate_bps = 0;
    uint64_t download_rate_bps = 0;
};

struct VPNEvent {
    std::string type;
    std::string message;
    ConnectionState state;
    time_t timestamp;
    json data;
};

class WireGuardWrapper {
public:
    using EventCallback = std::function<void(const VPNEvent&)>;
    using StatsCallback = std::function<void(const VPNStats&)>;

    WireGuardWrapper();
    ~WireGuardWrapper();

    // Initialization methods
    bool initializeFromFile(const std::string& config_file);
    bool initializeFromConfig(const std::string& interface_name,
                             const std::string& private_key,
                             const std::string& listen_port,
                             const std::string& peer_public_key,
                             const std::string& peer_endpoint,
                             const std::string& allowed_ips);

    // Connection control
    bool connect();
    bool disconnect();
    bool reconnect();

    // State and statistics
    ConnectionState getState() const;
    VPNStats getStats() const;
    bool isConnected() const;

    // Callbacks
    void setEventCallback(EventCallback callback);
    void setStatsCallback(StatsCallback callback);

    // JSON output
    json getStatusJson() const;
    json getStatsJson() const;
    json getLastErrorJson() const;

    // Routing Management
    struct RouteRule {
        std::string id;
        std::string name;
        std::string type;
        std::string destination;
        std::string gateway;
        std::string source_type;
        std::string source_value;
        std::string protocol;
        uint32_t metric;
        bool enabled;
        bool is_automatic;
        std::string description;
        
        json to_json() const;
        static RouteRule from_json(const json& j);
    };
    
    using RouteEventCallback = std::function<void(
        const std::string& event_type,
        const RouteRule& rule,
        const std::string& error_msg
    )>;
    
    bool addRouteRule(const RouteRule& rule);
    bool removeRouteRule(const std::string& rule_id);
    std::vector<RouteRule> getRouteRules() const;
    RouteRule getRouteRule(const std::string& rule_id) const;
    bool applyPreConnectionRoutes();
    bool detectPostConnectionRoutes();
    void setRouteEventCallback(RouteEventCallback callback);

private:
    wireguard_bridge_ctx_t* bridge_ctx_;
    std::atomic<ConnectionState> state_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;

    mutable std::mutex stats_mutex_;
    VPNStats current_stats_;

    std::string config_file_;
    std::string last_error_;

    EventCallback event_callback_;
    StatsCallback stats_callback_;

    std::unique_ptr<ThreadMgr::ThreadManager> thread_manager_;
    unsigned int worker_thread_id_;
    bool worker_thread_created_;

    wireguard_routing_ctx_t routing_ctx_;
    RouteEventCallback route_event_callback_;

    void workerLoop();
    void onStatsUpdate(const wireguard_bridge_stats_t* stats);
    void emitEvent(const std::string& type, const std::string& message, 
                   const json& data = json::object());
    void setState(ConnectionState new_state);
    std::string stateToString(ConnectionState state) const;
    
    static void route_callback_wrapper(
        const char* event_type,
        const char* rule_json,
        const char* error_msg,
        void* user_data
    );
};

} // namespace wireguard

#endif /* WIREGUARD_WRAPPER_HPP */