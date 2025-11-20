#include "wireguard_wrapper.hpp"
#include <chrono>
#include <ctime>
#include <iostream> // Include for std::cout and std::endl
#include <future> // Include for std::async and std::future
#include <unistd.h> // For usleep

namespace wireguard {

WireGuardWrapper::WireGuardWrapper()
    : bridge_ctx_(nullptr),
      state_(ConnectionState::INITIAL),
      running_(false),
      connected_(false),
      thread_manager_(std::make_unique<ThreadMgr::ThreadManager>(5)),
      worker_thread_id_(0),
      worker_thread_created_(false),
      routing_ctx_(nullptr) {

    wireguard_bridge_init_static();
    bridge_ctx_ = wireguard_bridge_create_context();
    
    if (bridge_ctx_) {
        routing_ctx_ = wireguard_bridge_routing_init(bridge_ctx_);
        if (routing_ctx_) {
            wireguard_bridge_routing_set_callback(routing_ctx_, route_callback_wrapper, this);
        }
    }
}

WireGuardWrapper::~WireGuardWrapper() {
    json destructor_log;
    destructor_log["type"] = "verbose";
    destructor_log["message"] = "WireGuard destructor called";
    destructor_log["config_file"] = config_file_;
    destructor_log["running"] = running_.load();
    destructor_log["connected"] = connected_.load();
    std::cout << destructor_log.dump() << std::endl;

    running_ = false;
    
    if (routing_ctx_) {
        wireguard_bridge_routing_cleanup(routing_ctx_);
        routing_ctx_ = nullptr;
    }

    if (worker_thread_created_ && thread_manager_) {
        json worker_join_log;
        worker_join_log["type"] = "verbose";
        worker_join_log["message"] = "WireGuard destructor stopping worker thread";
        worker_join_log["config_file"] = config_file_;
        std::cout << worker_join_log.dump() << std::endl;

        try {
            thread_manager_->stopThread(worker_thread_id_);
            thread_manager_->joinThread(worker_thread_id_);
        } catch (const std::exception& e) {
            json error_log;
            error_log["type"] = "error";
            error_log["message"] = "Failed to stop worker thread";
            error_log["error"] = e.what();
            std::cout << error_log.dump() << std::endl;
        }
        worker_thread_created_ = false;
    }

    if (bridge_ctx_) {
        json destroy_log;
        destroy_log["type"] = "verbose";
        destroy_log["message"] = "WireGuard destructor destroying bridge context";
        destroy_log["config_file"] = config_file_;
        std::cout << destroy_log.dump() << std::endl;

        wireguard_bridge_destroy_context(bridge_ctx_);
    }

    json uninit_log;
    uninit_log["type"] = "verbose";
    uninit_log["message"] = "WireGuard destructor calling bridge uninit";
    uninit_log["config_file"] = config_file_;
    std::cout << uninit_log.dump() << std::endl;

    wireguard_bridge_uninit_static();

    json complete_log;
    complete_log["type"] = "verbose";
    complete_log["message"] = "WireGuard destructor completed";
    complete_log["config_file"] = config_file_;
    std::cout << complete_log.dump() << std::endl;
}

bool WireGuardWrapper::initializeFromFile(const std::string& config_file) {
    if (!bridge_ctx_) {
        last_error_ = "Bridge context not initialized";
        emitEvent("error", last_error_);
        return false;
    }

    config_file_ = config_file;

    emitEvent("startup", "Initializing from config file: " + config_file);

    if (wireguard_bridge_parse_config(bridge_ctx_, config_file.c_str()) < 0) {
        last_error_ = wireguard_bridge_get_last_error(bridge_ctx_);
        setState(ConnectionState::ERROR_STATE);
        emitEvent("error", last_error_);
        return false;
    }

    setState(ConnectionState::CONFIGURING);
    emitEvent("initialized", "Configuration loaded successfully");

    return true;
}

bool WireGuardWrapper::initializeFromConfig(const std::string& interface_name,
                                            const std::string& private_key,
                                            const std::string& listen_port,
                                            const std::string& peer_public_key,
                                            const std::string& peer_endpoint,
                                            const std::string& allowed_ips) {
    if (!bridge_ctx_) {
        last_error_ = "Bridge context not initialized";
        return false;
    }

    wireguard_bridge_config_t config = {0};
    strncpy(config.interface_name, interface_name.c_str(), sizeof(config.interface_name) - 1);
    strncpy(config.private_key, private_key.c_str(), sizeof(config.private_key) - 1);
    strncpy(config.listen_port, listen_port.c_str(), sizeof(config.listen_port) - 1);
    strncpy(config.peer_public_key, peer_public_key.c_str(), sizeof(config.peer_public_key) - 1);
    strncpy(config.peer_endpoint, peer_endpoint.c_str(), sizeof(config.peer_endpoint) - 1);
    strncpy(config.allowed_ips, allowed_ips.c_str(), sizeof(config.allowed_ips) - 1);

    if (wireguard_bridge_set_config(bridge_ctx_, &config) < 0) {
        last_error_ = "Failed to set configuration";
        setState(ConnectionState::ERROR_STATE);
        return false;
    }

    setState(ConnectionState::CONFIGURING);
    emitEvent("initialized", "Programmatic configuration set");

    return true;
}

bool WireGuardWrapper::connect() {
    if (!bridge_ctx_) {
        last_error_ = "Bridge context not initialized";
        return false;
    }

    setState(ConnectionState::HANDSHAKING);
    emitEvent("handshaking", "Establishing WireGuard tunnel");

    // Use full connection setup (includes interface creation, IP assignment, routing, DNS)
    if (wireguard_bridge_connect_full(bridge_ctx_, true, true) < 0) {
        last_error_ = wireguard_bridge_get_last_error(bridge_ctx_);
        setState(ConnectionState::ERROR_STATE);
        emitEvent("error", last_error_);
        return false;
    }

    connected_ = true;
    setState(ConnectionState::CONNECTED);
    emitEvent("connected", "WireGuard tunnel established");

    // Start worker threads
    running_ = true;
    
    // Create worker thread for state monitoring
    if (!worker_thread_created_ && thread_manager_) {
        worker_thread_id_ = thread_manager_->createThread([this]() {
            workerLoop();
        });
        worker_thread_created_ = true;
    }
    
    // Start stats monitoring with the C bridge
    wireguard_bridge_start_stats_monitor(bridge_ctx_, 
        [](const wireguard_bridge_stats_t* stats, void* user_data) {
            if (user_data) {
                auto* wrapper = static_cast<WireGuardWrapper*>(user_data);
                wrapper->onStatsUpdate(stats);
            }
        },
        this,  // Pass 'this' as user_data
        1000); // Update interval in milliseconds

    return true;
}

bool WireGuardWrapper::disconnect() {
    if (!bridge_ctx_) {
        json log;
        log["type"] = "verbose";
        log["message"] = "WireGuard disconnect called but bridge_ctx is null";
        log["config_file"] = config_file_;
        std::cout << log.dump() << std::endl;
        return false;
    }

    json start_log;
    start_log["type"] = "verbose";
    start_log["message"] = "WireGuard disconnect started";
    start_log["config_file"] = config_file_;
    start_log["running"] = running_.load();
    start_log["connected"] = connected_.load();
    std::cout << start_log.dump() << std::endl;
    std::cout.flush();

    // Set flags FIRST to signal threads to stop
    running_ = false;
    connected_ = false;

    json flags_log;
    flags_log["type"] = "verbose";
    flags_log["message"] = "WireGuard running and connected flags set to false";
    flags_log["config_file"] = config_file_;
    std::cout << flags_log.dump() << std::endl;
    std::cout.flush();

    setState(ConnectionState::DISCONNECTED);
    emitEvent("disconnecting", "Cleaning up WireGuard interface");

    // Give threads a moment to detect the flag change
    usleep(100000); // 100ms

    // No need to join stats_thread_ as it's removed.
    // The C bridge handles its own internal threading for stats.

    // Wait for worker thread to exit
    if (worker_thread_created_ && thread_manager_) {
        json worker_join_log;
        worker_join_log["type"] = "verbose";
        worker_join_log["message"] = "WireGuard waiting for worker thread to stop";
        worker_join_log["config_file"] = config_file_;
        std::cout << worker_join_log.dump() << std::endl;
        std::cout.flush();

        try {
            thread_manager_->stopThread(worker_thread_id_);
            thread_manager_->joinThread(worker_thread_id_);
            worker_thread_created_ = false;

            json worker_joined_log;
            worker_joined_log["type"] = "verbose";
            worker_joined_log["message"] = "WireGuard worker thread stopped successfully";
            worker_joined_log["config_file"] = config_file_;
            std::cout << worker_joined_log.dump() << std::endl;
            std::cout.flush();
        } catch (const std::exception& e) {
            json error_log;
            error_log["type"] = "error";
            error_log["message"] = "Failed to stop worker thread";
            error_log["error"] = e.what();
            std::cout << error_log.dump() << std::endl;
        }
    } else {
        json no_worker_log;
        no_worker_log["type"] = "verbose";
        no_worker_log["message"] = "WireGuard worker thread not created";
        no_worker_log["config_file"] = config_file_;
        std::cout << no_worker_log.dump() << std::endl;
        std::cout.flush();
    }

    // Cleanup the interface (remove routes, DNS, bring down, delete)
    json cleanup_start_log;
    cleanup_start_log["type"] = "verbose";
    cleanup_start_log["message"] = "WireGuard calling wireguard_bridge_cleanup_interface";
    cleanup_start_log["config_file"] = config_file_;
    std::cout << cleanup_start_log.dump() << std::endl;

    if (wireguard_bridge_cleanup_interface(bridge_ctx_) < 0) {
        last_error_ = "Failed to cleanup interface";

        json cleanup_error_log;
        cleanup_error_log["type"] = "verbose";
        cleanup_error_log["message"] = "WireGuard interface cleanup failed";
        cleanup_error_log["config_file"] = config_file_;
        cleanup_error_log["error"] = last_error_;
        std::cout << cleanup_error_log.dump() << std::endl;

        emitEvent("error", last_error_);
        // Continue with disconnect even if cleanup fails
    } else {
        json cleanup_success_log;
        cleanup_success_log["type"] = "verbose";
        cleanup_success_log["message"] = "WireGuard interface cleanup successful";
        cleanup_success_log["config_file"] = config_file_;
        std::cout << cleanup_success_log.dump() << std::endl;
    }

    json disconnect_start_log;
    disconnect_start_log["type"] = "verbose";
    disconnect_start_log["message"] = "WireGuard calling wireguard_bridge_disconnect";
    disconnect_start_log["config_file"] = config_file_;
    std::cout << disconnect_start_log.dump() << std::endl;

    if (wireguard_bridge_disconnect(bridge_ctx_) < 0) {
        last_error_ = "Failed to disconnect";

        json disconnect_error_log;
        disconnect_error_log["type"] = "verbose";
        disconnect_error_log["message"] = "WireGuard bridge disconnect failed";
        disconnect_error_log["config_file"] = config_file_;
        disconnect_error_log["error"] = last_error_;
        std::cout << disconnect_error_log.dump() << std::endl;

        emitEvent("error", last_error_);
        return false;
    }

    json disconnect_success_log;
    disconnect_success_log["type"] = "verbose";
    disconnect_success_log["message"] = "WireGuard bridge disconnect successful";
    disconnect_success_log["config_file"] = config_file_;
    std::cout << disconnect_success_log.dump() << std::endl;

    emitEvent("shutdown", "WireGuard tunnel closed");

    json complete_log;
    complete_log["type"] = "verbose";
    complete_log["message"] = "WireGuard disconnect completed";
    complete_log["config_file"] = config_file_;
    std::cout << complete_log.dump() << std::endl;

    return true;
}

bool WireGuardWrapper::reconnect() {
    setState(ConnectionState::RECONNECTING);
    emitEvent("reconnecting", "Attempting to reconnect");

    if (wireguard_bridge_reconnect(bridge_ctx_) < 0) {
        last_error_ = wireguard_bridge_get_last_error(bridge_ctx_);
        setState(ConnectionState::ERROR_STATE);
        emitEvent("error", last_error_);
        return false;
    }

    setState(ConnectionState::CONNECTED);
    emitEvent("connected", "Reconnected successfully");

    return true;
}

ConnectionState WireGuardWrapper::getState() const {
    return state_.load();
}

VPNStats WireGuardWrapper::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return current_stats_;
}

bool WireGuardWrapper::isConnected() const {
    return connected_.load();
}

void WireGuardWrapper::setEventCallback(EventCallback callback) {
    event_callback_ = callback;
}

void WireGuardWrapper::setStatsCallback(StatsCallback callback) {
    stats_callback_ = callback;
}

json WireGuardWrapper::getStatusJson() const {
    json j;
    j["state"] = stateToString(state_.load());
    j["connected"] = connected_.load();
    j["timestamp"] = std::time(nullptr);
    return j;
}

json WireGuardWrapper::getStatsJson() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    json j;
    j["bytes_sent"] = current_stats_.bytes_sent;
    j["bytes_received"] = current_stats_.bytes_received;
    j["tx_packets"] = current_stats_.tx_packets;
    j["rx_packets"] = current_stats_.rx_packets;
    j["last_handshake"] = current_stats_.last_handshake;
    j["latency_ms"] = current_stats_.latency_ms;
    j["endpoint"] = current_stats_.endpoint;
    j["allowed_ips"] = current_stats_.allowed_ips;
    j["peer_public_key"] = current_stats_.peer_public_key;
    j["local_ip"] = current_stats_.local_ip;
    j["connected_duration"] = current_stats_.connected_duration;
    j["upload_rate_bps"] = current_stats_.upload_rate_bps;
    j["download_rate_bps"] = current_stats_.download_rate_bps;
    j["interface_name"] = current_stats_.interface_name;
    j["routes"] = json::parse(current_stats_.routes.empty() ? "[]" : current_stats_.routes);

    return j;
}

json WireGuardWrapper::getLastErrorJson() const {
    json j;
    j["error"] = last_error_;
    j["timestamp"] = std::time(nullptr);
    return j;
}

void WireGuardWrapper::workerLoop() {
    while (running_) {
        usleep(1000000); // 1 second

        if (bridge_ctx_) {
            auto current_state = static_cast<ConnectionState>(wireguard_bridge_get_state(bridge_ctx_));
            if (current_state != state_.load()) {
                setState(current_state);
            }
        }
    }
}

// Removed statsLoop() and updateStats() as per the new architecture.
// Statistics are now handled by the C bridge and a callback.

// New callback handler for stats updates from the C bridge
void WireGuardWrapper::onStatsUpdate(const wireguard_bridge_stats_t* stats) {
    if (!stats || !running_.load()) return;

    VPNStats vpn_stats;
    
    // Use instance-level tracking instead of static to support multiple instances
    static thread_local uint64_t last_bytes_sent = 0;
    static thread_local uint64_t last_bytes_received = 0;
    static thread_local auto last_update_time = std::chrono::steady_clock::now();
    static thread_local bool first_update = true;

    // Copy C stats to C++ stats structure and calculate rates
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);

        // Calculate bandwidth rates
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_update_time).count();
        
        if (elapsed_ms > 0 && !first_update) {
            double elapsed_seconds = elapsed_ms / 1000.0;
            
            // Calculate byte differences
            uint64_t bytes_sent_diff = 0;
            uint64_t bytes_received_diff = 0;
            
            if (stats->bytes_sent >= last_bytes_sent) {
                bytes_sent_diff = stats->bytes_sent - last_bytes_sent;
            }
            if (stats->bytes_received >= last_bytes_received) {
                bytes_received_diff = stats->bytes_received - last_bytes_received;
            }
            
            // Calculate rates (bytes per second)
            current_stats_.upload_rate_bps = static_cast<uint64_t>(bytes_sent_diff / elapsed_seconds);
            current_stats_.download_rate_bps = static_cast<uint64_t>(bytes_received_diff / elapsed_seconds);
        } else {
            first_update = false;
        }
        
        // Update tracking variables
        last_bytes_sent = stats->bytes_sent;
        last_bytes_received = stats->bytes_received;
        last_update_time = current_time;

        // Update cumulative counters
        current_stats_.bytes_sent = stats->bytes_sent;
        current_stats_.bytes_received = stats->bytes_received;
        current_stats_.tx_packets = stats->tx_packets;
        current_stats_.rx_packets = stats->rx_packets;
        current_stats_.last_handshake = stats->last_handshake;
        current_stats_.latency_ms = stats->latency_ms;
        current_stats_.connected_duration = stats->connected_duration;

        // Safely copy strings with validation
        current_stats_.endpoint = stats->endpoint[0] ? std::string(stats->endpoint) : "";
        current_stats_.allowed_ips = stats->allowed_ips[0] ? std::string(stats->allowed_ips) : "";
        current_stats_.peer_public_key = stats->public_key[0] ? std::string(stats->public_key) : "";
        current_stats_.local_ip = stats->local_ip[0] ? std::string(stats->local_ip) : "";
        current_stats_.interface_name = stats->interface_name[0] ? std::string(stats->interface_name) : "";
        current_stats_.routes = stats->routes[0] ? std::string(stats->routes) : "[]";

        vpn_stats = current_stats_;
    }

    // Invoke user callback if set
    if (stats_callback_ && running_.load()) {
        stats_callback_(vpn_stats);
    }

    // Emit event with stats data
    try {
        json stats_data;
        stats_data["bytes_sent"] = vpn_stats.bytes_sent;
        stats_data["bytes_received"] = vpn_stats.bytes_received;
        stats_data["tx_packets"] = vpn_stats.tx_packets;
        stats_data["rx_packets"] = vpn_stats.rx_packets;
        stats_data["last_handshake"] = vpn_stats.last_handshake;
        stats_data["latency_ms"] = vpn_stats.latency_ms;
        stats_data["endpoint"] = vpn_stats.endpoint;
        stats_data["allowed_ips"] = vpn_stats.allowed_ips;
        stats_data["peer_public_key"] = vpn_stats.peer_public_key;
        stats_data["local_ip"] = vpn_stats.local_ip;
        stats_data["connected_duration"] = vpn_stats.connected_duration;
        stats_data["upload_rate_bps"] = vpn_stats.upload_rate_bps;
        stats_data["download_rate_bps"] = vpn_stats.download_rate_bps;
        stats_data["interface_name"] = vpn_stats.interface_name;
        stats_data["routes"] = json::parse(vpn_stats.routes.empty() ? "[]" : vpn_stats.routes);

        emitEvent("stats", "Statistics updated", stats_data);
    } catch (const std::exception& e) {
        json error_log;
        error_log["type"] = "verbose";
        error_log["message"] = "WireGuard onStatsUpdate: JSON serialization error";
        error_log["config_file"] = config_file_;
        error_log["error"] = e.what();
        std::cout << error_log.dump() << std::endl;
    }
}

void WireGuardWrapper::emitEvent(const std::string& type, const std::string& message, const json& data) {
    if (event_callback_) {
        VPNEvent event;
        event.type = type;
        event.message = message;
        event.state = state_.load();
        event.timestamp = std::time(nullptr);
        event.data = data;

        event_callback_(event);
    }
}

void WireGuardWrapper::setState(ConnectionState new_state) {
    auto old_state = state_.exchange(new_state);
    if (old_state != new_state) {
        emitEvent("status", "State changed to " + stateToString(new_state));
    }
}

std::string WireGuardWrapper::stateToString(ConnectionState state) const {
    switch (state) {
        case ConnectionState::INITIAL: return "initial";
        case ConnectionState::CONFIGURING: return "configuring";
        case ConnectionState::HANDSHAKING: return "handshaking";
        case ConnectionState::CONNECTED: return "connected";
        case ConnectionState::RECONNECTING: return "reconnecting";
        case ConnectionState::DISCONNECTED: return "disconnected";
        case ConnectionState::ERROR_STATE: return "error";
        default: return "unknown";
    }
}

json WireGuardWrapper::RouteRule::to_json() const {
    json j;
    j["id"] = id;
    j["name"] = name;
    j["type"] = type;
    j["destination"] = destination;
    j["gateway"] = gateway;
    j["source_type"] = source_type;
    j["source_value"] = source_value;
    j["protocol"] = protocol;
    j["metric"] = metric;
    j["enabled"] = enabled;
    j["is_automatic"] = is_automatic;
    j["description"] = description;
    return j;
}

WireGuardWrapper::RouteRule WireGuardWrapper::RouteRule::from_json(const json& j) {
    RouteRule rule;
    rule.id = j.value("id", "");
    rule.name = j.value("name", "");
    rule.type = j.value("type", "");
    rule.destination = j.value("destination", "");
    rule.gateway = j.value("gateway", "");
    rule.source_type = j.value("source_type", "any");
    rule.source_value = j.value("source_value", "");
    rule.protocol = j.value("protocol", "both");
    rule.metric = j.value("metric", 100);
    rule.enabled = j.value("enabled", true);
    rule.is_automatic = j.value("is_automatic", false);
    rule.description = j.value("description", "");
    return rule;
}

bool WireGuardWrapper::addRouteRule(const RouteRule& rule) {
    if (!routing_ctx_) {
        return false;
    }
    
    std::string rule_json = rule.to_json().dump();
    return wireguard_bridge_routing_add_rule_json(routing_ctx_, rule_json.c_str()) == 0;
}

bool WireGuardWrapper::removeRouteRule(const std::string& rule_id) {
    if (!routing_ctx_) {
        return false;
    }
    
    return wireguard_bridge_routing_remove_rule(routing_ctx_, rule_id.c_str()) == 0;
}

std::vector<WireGuardWrapper::RouteRule> WireGuardWrapper::getRouteRules() const {
    std::vector<RouteRule> rules;
    
    if (!routing_ctx_) {
        return rules;
    }
    
    char* json_str = wireguard_bridge_routing_get_all_json(routing_ctx_);
    if (!json_str) {
        return rules;
    }
    
    try {
        json j = json::parse(json_str);
        if (j.contains("rules") && j["rules"].is_array()) {
            for (const auto& rule_json : j["rules"]) {
                rules.push_back(RouteRule::from_json(rule_json));
            }
        }
    } catch (const std::exception& e) {
        json error_log;
        error_log["type"] = "error";
        error_log["message"] = "Failed to parse route rules JSON";
        error_log["error"] = e.what();
        std::cout << error_log.dump() << std::endl;
    }
    
    free(json_str);
    return rules;
}

WireGuardWrapper::RouteRule WireGuardWrapper::getRouteRule(const std::string& rule_id) const {
    auto rules = getRouteRules();
    for (const auto& rule : rules) {
        if (rule.id == rule_id) {
            return rule;
        }
    }
    return RouteRule();
}

bool WireGuardWrapper::applyPreConnectionRoutes() {
    if (!routing_ctx_) {
        return false;
    }
    
    return wireguard_bridge_routing_apply_pre_connect(routing_ctx_) >= 0;
}

bool WireGuardWrapper::detectPostConnectionRoutes() {
    if (!routing_ctx_) {
        return false;
    }
    
    return wireguard_bridge_routing_detect_post_connect(routing_ctx_) >= 0;
}

void WireGuardWrapper::setRouteEventCallback(RouteEventCallback callback) {
    route_event_callback_ = callback;
}

void WireGuardWrapper::route_callback_wrapper(
    const char* event_type,
    const char* rule_json,
    const char* error_msg,
    void* user_data
) {
    auto* wrapper = static_cast<WireGuardWrapper*>(user_data);
    if (!wrapper || !wrapper->route_event_callback_) {
        return;
    }
    
    try {
        json j = json::parse(rule_json);
        RouteRule rule = RouteRule::from_json(j);
        
        wrapper->route_event_callback_(
            event_type ? event_type : "unknown",
            rule,
            error_msg ? error_msg : ""
        );
    } catch (const std::exception& e) {
        json error_log;
        error_log["type"] = "error";
        error_log["message"] = "Failed to parse route event";
        error_log["error"] = e.what();
        std::cout << error_log.dump() << std::endl;
    }
}

} // namespace wireguard