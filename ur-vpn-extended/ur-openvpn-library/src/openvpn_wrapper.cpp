#include "openvpn_wrapper.hpp"
#include <chrono>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>
#include <csignal>
#include <future> // Required for std::async and std::future
#include <thread> // Required for std::this_thread

// Include logger for source control
extern "C" {
    #include "../ur-rpc-template/deps/ur-logger-api/logger.h"
}

namespace openvpn {

OpenVPNWrapper::OpenVPNWrapper()
    : bridge_ctx_(nullptr),
      state_(ConnectionState::INITIAL),
      running_(false),
      connected_(false),
      routing_ctx_(nullptr) {

    // Initialize OpenVPN static components via bridge
    if (openvpn_bridge_init_static() != 0) {
        last_error_ = "Failed to initialize OpenVPN static components";
        state_ = ConnectionState::ERROR_STATE;
    }
}

OpenVPNWrapper::~OpenVPNWrapper() {
    disconnect();
    running_ = false; // Ensure running_ is false for thread termination

    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }

    if (stats_thread_ && stats_thread_->joinable()) {
        stats_thread_->join();
    }
    
    if (routing_ctx_) {
        openvpn_bridge_routing_cleanup(routing_ctx_);
        routing_ctx_ = nullptr;
    }

    if (bridge_ctx_) {
        openvpn_bridge_destroy_context(bridge_ctx_);
        bridge_ctx_ = nullptr;
    }

    openvpn_bridge_uninit_static();
}

bool OpenVPNWrapper::initializeFromFile(const std::string& config_file) {
    config_file_ = config_file;

    std::ifstream file(config_file);
    if (!file.good()) {
        last_error_ = "Configuration file not found: " + config_file;
        setState(ConnectionState::ERROR_STATE);
        return false;
    }
    file.close();

    try {
        // Create bridge context
        bridge_ctx_ = openvpn_bridge_create_context();
        if (!bridge_ctx_) {
            last_error_ = "Failed to create OpenVPN context";
            setState(ConnectionState::ERROR_STATE);
            return false;
        }

        // Parse configuration
        if (openvpn_bridge_parse_config(bridge_ctx_, config_file.c_str()) != 0) {
            last_error_ = "Failed to parse configuration file";
            setState(ConnectionState::ERROR_STATE);
            return false;
        }

        // Initialize routing context
        routing_ctx_ = openvpn_bridge_routing_init(bridge_ctx_);
        if (routing_ctx_) {
            openvpn_bridge_routing_set_callback(routing_ctx_, route_callback_wrapper, this);
        }

        emitEvent("initialized", "OpenVPN wrapper initialized from config file");
        setState(ConnectionState::INITIAL);

        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Initialization failed: ") + e.what();
        emitEvent("error", last_error_);
        setState(ConnectionState::ERROR_STATE);
        return false;
    }
}

bool OpenVPNWrapper::connect() {
    if (!bridge_ctx_) {
        last_error_ = "Context not initialized";
        return false;
    }

    if (running_) {
        last_error_ = "Already connecting or connected";
        return false;
    }

    running_ = true;
    setState(ConnectionState::CONNECTING);

    emitEvent("connecting", "Starting VPN connection");

    // Start worker thread
    worker_thread_ = std::make_unique<std::thread>(&OpenVPNWrapper::workerLoop, this);

    // Start stats monitoring thread
    stats_thread_ = std::make_unique<std::thread>(&OpenVPNWrapper::statsLoop, this);

    return true;
}

bool OpenVPNWrapper::disconnect() {
    if (!running_) {
        json already_stopped_log;
        already_stopped_log["type"] = "verbose";
        already_stopped_log["message"] = "OpenVPN disconnect called but already stopped";
        already_stopped_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << already_stopped_log.dump() << std::endl;
        }
        return false;
    }

    json disconnect_start_log;
    disconnect_start_log["type"] = "verbose";
    disconnect_start_log["message"] = "OpenVPN disconnect started";
    disconnect_start_log["config_file"] = config_file_;
    if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
        std::cout << disconnect_start_log.dump() << std::endl;
    }

    running_ = false; // Signal threads to stop
    connected_ = false;

    json flags_log;
    flags_log["type"] = "verbose";
    flags_log["message"] = "OpenVPN running and connected flags set to false";
    flags_log["config_file"] = config_file_;
    if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
        std::cout << flags_log.dump() << std::endl;
    }

    setState(ConnectionState::DISCONNECTED);
    emitEvent("disconnecting", "Stopping VPN connection");

    // Signal to stop
    if (bridge_ctx_) {
        json signal_log;
        signal_log["type"] = "verbose";
        signal_log["message"] = "OpenVPN sending SIGTERM signal to bridge";
        signal_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << signal_log.dump() << std::endl;
        }

        openvpn_bridge_signal(bridge_ctx_, SIGTERM);

        json signal_sent_log;
        signal_sent_log["type"] = "verbose";
        signal_sent_log["message"] = "OpenVPN SIGTERM signal sent successfully";
        signal_sent_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << signal_sent_log.dump() << std::endl;
        }
    }

    // Wait for worker thread
    if (worker_thread_ && worker_thread_->joinable()) {
        json worker_join_log;
        worker_join_log["type"] = "verbose";
        worker_join_log["message"] = "OpenVPN waiting for worker thread to join";
        worker_join_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << worker_join_log.dump() << std::endl;
        }

        worker_thread_->join();

        json worker_joined_log;
        worker_joined_log["type"] = "verbose";
        worker_joined_log["message"] = "OpenVPN worker thread joined successfully";
        worker_joined_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << worker_joined_log.dump() << std::endl;
        }
    } else {
        json no_worker_log;
        no_worker_log["type"] = "verbose";
        no_worker_log["message"] = "OpenVPN worker thread not joinable or null";
        no_worker_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << no_worker_log.dump() << std::endl;
        }
    }

    // Wait for stats thread
    if (stats_thread_ && stats_thread_->joinable()) {
        json stats_join_log;
        stats_join_log["type"] = "verbose";
        stats_join_log["message"] = "OpenVPN waiting for stats thread to join";
        stats_join_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << stats_join_log.dump() << std::endl;
        }

        stats_thread_->join();

        json stats_joined_log;
        stats_joined_log["type"] = "verbose";
        stats_joined_log["message"] = "OpenVPN stats thread joined successfully";
        stats_joined_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << stats_joined_log.dump() << std::endl;
        }
    } else {
        json no_stats_log;
        no_stats_log["type"] = "verbose";
        no_stats_log["message"] = "OpenVPN stats thread not joinable or null";
        no_stats_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << no_stats_log.dump() << std::endl;
        }
    }

    emitEvent("disconnected", "VPN connection stopped");

    json complete_log;
    complete_log["type"] = "verbose";
    complete_log["message"] = "OpenVPN disconnect completed successfully";
    complete_log["config_file"] = config_file_;
    if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
        std::cout << complete_log.dump() << std::endl;
    }

    return true;
}

bool OpenVPNWrapper::reconnect() {
    disconnect();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return connect();
}

ConnectionState OpenVPNWrapper::getState() const {
    if (!bridge_ctx_) {
        return state_.load();
    }

    openvpn_bridge_state_t bridge_state = openvpn_bridge_get_state(bridge_ctx_);
    return static_cast<ConnectionState>(bridge_state);
}

VPNStats OpenVPNWrapper::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return current_stats_;
}

bool OpenVPNWrapper::isConnected() const {
    if (!bridge_ctx_) {
        return false;
    }
    return openvpn_bridge_is_connected(bridge_ctx_);
}

void OpenVPNWrapper::setEventCallback(EventCallback callback) {
    event_callback_ = callback;
}

void OpenVPNWrapper::setStatsCallback(StatsCallback callback) {
    stats_callback_ = callback;
}

json OpenVPNWrapper::getStatusJson() const {
    json status;
    status["state"] = stateToString(getState());
    status["connected"] = isConnected();
    status["config_file"] = config_file_;

    if (!last_error_.empty()) {
        status["last_error"] = last_error_;
    }

    return status;
}

json OpenVPNWrapper::getStatsJson() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    json stats;
    stats["bytes_sent"] = current_stats_.bytes_sent;
    stats["bytes_received"] = current_stats_.bytes_received;
    stats["tun_read_bytes"] = current_stats_.tun_read_bytes;
    stats["tun_write_bytes"] = current_stats_.tun_write_bytes;
    stats["ping_ms"] = current_stats_.ping_ms;
    stats["local_ip"] = current_stats_.local_ip;
    stats["remote_ip"] = current_stats_.remote_ip;
    stats["server_ip"] = current_stats_.server_ip;
    stats["interface_name"] = current_stats_.interface_name;
    stats["routes"] = json::parse(current_stats_.routes.empty() ? "[]" : current_stats_.routes);

    // Real-time bandwidth rates
    stats["upload_rate_bps"] = current_stats_.upload_rate_bps;
    stats["download_rate_bps"] = current_stats_.download_rate_bps;

    if (current_stats_.connected_since > 0) {
        stats["connected_since"] = current_stats_.connected_since;
        stats["uptime_seconds"] = time(nullptr) - current_stats_.connected_since;
    }

    return stats;
}

json OpenVPNWrapper::getLastErrorJson() const {
    json error;
    error["error"] = last_error_;
    error["timestamp"] = time(nullptr);
    return error;
}

void OpenVPNWrapper::workerLoop() {
    try {
        if (!bridge_ctx_) {
            return;
        }

        // Initialize context level 1 via bridge
        if (openvpn_bridge_context_init_1(bridge_ctx_) != 0) {
            last_error_ = "Failed to initialize context";
            setState(ConnectionState::ERROR_STATE);
            running_ = false;
            return;
        }

        setState(ConnectionState::AUTHENTICATING);
        emitEvent("authenticating", "Authenticating with VPN server");

        // Run the OpenVPN tunnel in a separate thread to allow state monitoring
        std::thread tunnel_thread([this]() {
            openvpn_bridge_run_tunnel(bridge_ctx_);
        });

        // Monitor connection state while tunnel is running
        bool was_connected = false;
        while (running_) {
            bool is_currently_connected = openvpn_bridge_is_connected(bridge_ctx_);
            
            // Detect connection establishment
            if (is_currently_connected && !was_connected) {
                connected_ = true;
                setState(ConnectionState::CONNECTED);
                emitEvent("connected", "VPN tunnel established");
                
                // Set connection time on first connect
                std::lock_guard<std::mutex> lock(stats_mutex_);
                if (current_stats_.connected_since == 0) {
                    current_stats_.connected_since = time(nullptr);
                }
                was_connected = true;
            } else if (!is_currently_connected && was_connected) {
                // Connection lost
                connected_ = false;
                was_connected = false;
            }

            // Small sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Wait for tunnel thread to complete
        if (tunnel_thread.joinable()) {
            tunnel_thread.join();
        }

        // Check final state
        if (!connected_) {
            setState(ConnectionState::DISCONNECTED);
            emitEvent("disconnected", "VPN tunnel exited");
        }

    } catch (const std::exception& e) {
        last_error_ = std::string("Worker thread error: ") + e.what();
        setState(ConnectionState::ERROR_STATE);
        emitEvent("error", last_error_);
        running_ = false;
    } catch (...) {
        last_error_ = "Unknown worker thread error";
        setState(ConnectionState::ERROR_STATE);
        emitEvent("error", last_error_);
        running_ = false;
    }
}

void OpenVPNWrapper::statsLoop() {
    uint64_t last_bytes_sent = 0;
    uint64_t last_bytes_received = 0;
    auto last_update_time = std::chrono::steady_clock::now();

    while (running_) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_update_time).count();

        if (elapsed_seconds > 0) {
            // Use atomic check before potentially blocking operations
            if (!running_) break;

            updateStats();

            VPNStats stats_copy;
            // Calculate bandwidth rates (bytes per second)
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);

                uint64_t bytes_sent_diff = current_stats_.bytes_sent - last_bytes_sent;
                uint64_t bytes_received_diff = current_stats_.bytes_received - last_bytes_received;

                // Store instantaneous bandwidth (bytes/sec)
                current_stats_.upload_rate_bps = bytes_sent_diff / elapsed_seconds;
                current_stats_.download_rate_bps = bytes_received_diff / elapsed_seconds;

                last_bytes_sent = current_stats_.bytes_sent;
                last_bytes_received = current_stats_.bytes_received;

                stats_copy = current_stats_;
            }

            last_update_time = current_time;

            if (stats_callback_ && running_) {
                stats_callback_(stats_copy);
            }
        }

        // Interruptible sleep: check running_ every 100ms instead of sleeping for full second
        for (int i = 0; i < 10 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Log when stats loop exits
    json exit_log;
    exit_log["type"] = "verbose";
    exit_log["message"] = "OpenVPN statsLoop exiting";
    exit_log["config_file"] = config_file_;
    exit_log["running"] = running_.load();
    if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
        std::cout << exit_log.dump() << std::endl;
    }
}

void OpenVPNWrapper::updateStats() {
    if (!bridge_ctx_ || !running_) {
        return;
    }

    // Use try_lock to avoid blocking during shutdown
    std::unique_lock<std::mutex> lock(stats_mutex_, std::defer_lock);
    if (!lock.try_lock()) {
        // If we can't acquire lock immediately, skip this update
        return;
    }

    openvpn_bridge_stats_t bridge_stats;
    // Use async call with timeout protection
    auto future = std::async(std::launch::async, [this, &bridge_stats]() {
        // Ensure bridge_ctx_ is still valid and running_ is true within the async task
        if (!bridge_ctx_ || !running_) return -1; // Indicate an issue
        return openvpn_bridge_get_stats(bridge_ctx_, &bridge_stats);
    });

    // Wait for stats with timeout
    if (future.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready) {
        try {
            int result = future.get();
            if (result == 0) {
                current_stats_.bytes_sent = bridge_stats.bytes_sent;
                current_stats_.bytes_received = bridge_stats.bytes_received;
                current_stats_.tun_read_bytes = bridge_stats.tun_read_bytes;
                current_stats_.tun_write_bytes = bridge_stats.tun_write_bytes;
                current_stats_.ping_ms = bridge_stats.ping_ms;
                
                // Safely copy strings with validation
                if (bridge_stats.local_ip && bridge_stats.local_ip[0] != '\0') {
                    current_stats_.local_ip = std::string(bridge_stats.local_ip);
                } else {
                    current_stats_.local_ip = "";
                }
                
                if (bridge_stats.remote_ip && bridge_stats.remote_ip[0] != '\0') {
                    current_stats_.remote_ip = std::string(bridge_stats.remote_ip);
                } else {
                    current_stats_.remote_ip = "";
                }
                
                if (bridge_stats.server_ip && bridge_stats.server_ip[0] != '\0') {
                    current_stats_.server_ip = std::string(bridge_stats.server_ip);
                } else {
                    current_stats_.server_ip = "";
                }
                
                if (bridge_stats.interface_name && bridge_stats.interface_name[0] != '\0') {
                    current_stats_.interface_name = std::string(bridge_stats.interface_name);
                } else {
                    current_stats_.interface_name = "";
                }
                
                if (bridge_stats.routes && bridge_stats.routes[0] != '\0') {
                    current_stats_.routes = std::string(bridge_stats.routes);
                } else {
                    current_stats_.routes = "[]";
                }

                // Set connected_since if connected and not already set
                if (connected_ && current_stats_.connected_since == 0) {
                    current_stats_.connected_since = time(nullptr);
                }
            } else {
                 // Log failure if needed, or handle error code
                json fail_log;
                fail_log["type"] = "verbose";
                fail_log["message"] = "OpenVPN updateStats: openvpn_bridge_get_stats failed";
                fail_log["config_file"] = config_file_;
                fail_log["error_code"] = result;
                if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
                    std::cout << fail_log.dump() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            // Handle potential exceptions from future.get()
            json error_log;
            error_log["type"] = "verbose";
            error_log["message"] = "OpenVPN updateStats: Exception getting future result";
            error_log["config_file"] = config_file_;
            error_log["error"] = e.what();
            if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
                std::cout << error_log.dump() << std::endl;
            }
        }
    } else {
        // If timeout, stats update is skipped silently
        json timeout_log;
        timeout_log["type"] = "verbose";
        timeout_log["message"] = "OpenVPN updateStats: Timeout waiting for get_stats";
        timeout_log["config_file"] = config_file_;
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << timeout_log.dump() << std::endl;
        }
    }
}

void OpenVPNWrapper::emitEvent(const std::string& type, const std::string& message, const json& data) {
    VPNEvent event;
    event.type = type;
    event.message = message;
    event.state = state_.load();
    event.timestamp = time(nullptr);
    event.data = data;

    if (event_callback_) {
        event_callback_(event);
    }
}

void OpenVPNWrapper::setState(ConnectionState new_state) {
    ConnectionState old_state = state_.exchange(new_state);

    if (old_state != new_state) {
        json state_data;
        state_data["old_state"] = stateToString(old_state);
        state_data["new_state"] = stateToString(new_state);
        emitEvent("state_change", "Connection state changed", state_data);
    }
}

std::string OpenVPNWrapper::stateToString(ConnectionState state) const {
    switch (state) {
        case ConnectionState::INITIAL: return "INITIAL";
        case ConnectionState::CONNECTING: return "CONNECTING";
        case ConnectionState::WAIT: return "WAIT";
        case ConnectionState::AUTHENTICATING: return "AUTHENTICATING";
        case ConnectionState::GET_CONFIG: return "GET_CONFIG";
        case ConnectionState::ASSIGN_IP: return "ASSIGN_IP";
        case ConnectionState::ADD_ROUTES: return "ADD_ROUTES";
        case ConnectionState::CONNECTED: return "CONNECTED";
        case ConnectionState::RECONNECTING: return "RECONNECTING";
        case ConnectionState::EXITING: return "EXITING";
        case ConnectionState::DISCONNECTED: return "DISCONNECTED";
        case ConnectionState::ERROR_STATE: return "ERROR";
        default: return "UNKNOWN";
    }
}

json OpenVPNWrapper::RouteRule::to_json() const {
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

OpenVPNWrapper::RouteRule OpenVPNWrapper::RouteRule::from_json(const json& j) {
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

bool OpenVPNWrapper::addRouteRule(const RouteRule& rule) {
    if (!routing_ctx_) {
        return false;
    }
    
    std::string rule_json = rule.to_json().dump();
    return openvpn_bridge_routing_add_rule_json(routing_ctx_, rule_json.c_str()) == 0;
}

bool OpenVPNWrapper::removeRouteRule(const std::string& rule_id) {
    if (!routing_ctx_) {
        return false;
    }
    
    return openvpn_bridge_routing_remove_rule(routing_ctx_, rule_id.c_str()) == 0;
}

std::vector<OpenVPNWrapper::RouteRule> OpenVPNWrapper::getRouteRules() const {
    std::vector<RouteRule> rules;
    
    if (!routing_ctx_) {
        return rules;
    }
    
    char* json_str = openvpn_bridge_routing_get_all_json(routing_ctx_);
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
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << error_log.dump() << std::endl;
        }
    }
    
    free(json_str);
    return rules;
}

OpenVPNWrapper::RouteRule OpenVPNWrapper::getRouteRule(const std::string& rule_id) const {
    auto rules = getRouteRules();
    for (const auto& rule : rules) {
        if (rule.id == rule_id) {
            return rule;
        }
    }
    return RouteRule();
}

bool OpenVPNWrapper::applyPreConnectionRoutes() {
    if (!routing_ctx_) {
        return false;
    }
    
    return openvpn_bridge_routing_apply_pre_connect(routing_ctx_) >= 0;
}

bool OpenVPNWrapper::detectPostConnectionRoutes() {
    if (!routing_ctx_) {
        return false;
    }
    
    return openvpn_bridge_routing_detect_post_connect(routing_ctx_) >= 0;
}

void OpenVPNWrapper::setRouteEventCallback(RouteEventCallback callback) {
    route_event_callback_ = callback;
}

void OpenVPNWrapper::route_callback_wrapper(
    const char* event_type,
    const char* rule_json,
    const char* error_msg,
    void* user_data
) {
    auto* wrapper = static_cast<OpenVPNWrapper*>(user_data);
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
        if (logger_is_source_enabled(LOG_SOURCE_OPENVPN_LIBRARY)) {
            std::cout << error_log.dump() << std::endl;
        }
    }
}

} // namespace openvpn