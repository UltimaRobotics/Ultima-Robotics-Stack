#include "urwt/monitor/network_monitor.hpp"
#include <algorithm>

namespace urwt {
namespace monitor {

NetworkMonitor::NetworkMonitor(
    std::shared_ptr<WirelessToolsAPI> api,
    std::shared_ptr<network::SavedNetworkManager> saved_networks,
    std::shared_ptr<network::ConnectionManager> connection_mgr,
    std::shared_ptr<events::EventPublisher> event_publisher,
    const MonitorConfig& config)
    : api_(api)
    , saved_networks_(saved_networks)
    , connection_mgr_(connection_mgr)
    , event_publisher_(event_publisher)
    , scan_cache_(std::make_shared<ScanResultCache>(config.cache_duration))
    , config_(config)
    , last_scan_time_(std::chrono::system_clock::now()) {
}

NetworkMonitor::~NetworkMonitor() {
    if (running_) {
        stop();
    }
}

Result<bool, std::string> NetworkMonitor::start() {
    if (running_) {
        return Result<bool, std::string>::error("Monitor already running");
    }

    running_ = true;
    paused_ = false;
    
    try {
        monitor_thread_ = std::make_unique<std::thread>(
            &NetworkMonitor::monitorLoop, this);
        return Result<bool, std::string>::ok(true);
    } catch (const std::exception& e) {
        running_ = false;
        return Result<bool, std::string>::error(
            "Failed to start monitor thread: " + std::string(e.what()));
    }
}

Result<bool, std::string> NetworkMonitor::stop() {
    if (!running_) {
        return Result<bool, std::string>::error("Monitor not running");
    }

    running_ = false;
    cv_.notify_all();

    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }

    monitor_thread_.reset();
    return Result<bool, std::string>::ok(true);
}

bool NetworkMonitor::isRunning() const {
    return running_;
}

void NetworkMonitor::setConfig(const MonitorConfig& config) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    config_ = config;
    scan_cache_->setTTL(config.cache_duration);
}

MonitorConfig NetworkMonitor::getConfig() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return config_;
}

void NetworkMonitor::pauseMonitoring() {
    paused_ = true;
}

void NetworkMonitor::resumeMonitoring() {
    paused_ = false;
    cv_.notify_one();
}

bool NetworkMonitor::isPaused() const {
    return paused_;
}

void NetworkMonitor::triggerImmediateScan() {
    immediate_scan_requested_ = true;
    cv_.notify_one();
}

void NetworkMonitor::setScanCallback(ScanCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    scan_callback_ = callback;
}

void NetworkMonitor::setConnectionCallback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

void NetworkMonitor::setStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_change_callback_ = callback;
}

state::SystemWirelessState NetworkMonitor::getCurrentState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_;
}

std::optional<ScanResult> NetworkMonitor::getLastScanResult() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return last_scan_result_;
}

std::chrono::system_clock::time_point NetworkMonitor::getLastScanTime() const {
    return last_scan_time_;
}

size_t NetworkMonitor::getConnectionAttempts() const {
    return connection_attempts_;
}

void NetworkMonitor::monitorLoop() {
    while (running_) {
        if (paused_) {
            std::unique_lock<std::mutex> lock(state_mutex_);
            cv_.wait(lock, [this] { return !paused_ || !running_; });
            continue;
        }

        bool should_scan = false;
        
        if (immediate_scan_requested_) {
            immediate_scan_requested_ = false;
            should_scan = true;
        } else if (config_.continuous_scanning) {
            auto now = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_scan_time_);
            should_scan = (elapsed >= config_.scan_interval);
        }

        if (should_scan) {
            scanNetworks();
        }

        std::unique_lock<std::mutex> lock(state_mutex_);
        cv_.wait_for(lock, config_.scan_interval, 
            [this] { return !running_ || immediate_scan_requested_; });
    }
}

void NetworkMonitor::scanNetworks() {
    auto interface_result = api_->getInterface(config_.interface_name);
    if (!interface_result.isOk()) {
        return;
    }

    auto interface = interface_result.value();
    auto scan_result = api_->scan(interface);
    
    if (!scan_result.isOk()) {
        return;
    }

    auto result = scan_result.value();
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_scan_result_ = result;
        last_scan_time_ = std::chrono::system_clock::now();
    }

    scan_cache_->store(result);

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (scan_callback_ && config_.publish_scan_events) {
            scan_callback_(result);
        }
    }

    if (config_.auto_connect_enabled) {
        processAvailableNetworks(result);
    }
}

void NetworkMonitor::processAvailableNetworks(const ScanResult& result) {
    if (!config_.auto_connect_enabled) {
        return;
    }

    auto available_networks = result.networks();
    auto best_network = findBestNetwork(available_networks);

    if (!best_network) {
        return;
    }

    auto interface_result = api_->getInterface(config_.interface_name);
    if (!interface_result.isOk()) {
        return;
    }

    auto interface = interface_result.value();
    bool already_connected = connection_mgr_->isConnected(interface);
    
    if (already_connected) {
        auto current_ssid = connection_mgr_->getCurrentSSID(interface);
        if (current_ssid && *current_ssid == best_network->ssid) {
            resetConnectionAttempts();
            return;
        }
    }

    if (connection_attempts_ >= config_.max_connection_attempts) {
        if (!shouldAttemptReconnection()) {
            return;
        }
        resetConnectionAttempts();
    }

    attemptConnection(*best_network);
}

void NetworkMonitor::attemptConnection(const config::NetworkProfile& profile) {
    auto interface_result = api_->getInterface(config_.interface_name);
    if (!interface_result.isOk()) {
        handleConnectionResult(profile.ssid, false);
        return;
    }

    auto interface = interface_result.value();
    
    incrementConnectionAttempts();
    last_connected_ssid_ = profile.ssid;
    last_connection_attempt_ = std::chrono::system_clock::now();

    auto result = connection_mgr_->connectWithTimeout(
        profile, interface, config_.connection_timeout);

    bool success = result.isOk() && result.value();
    handleConnectionResult(profile.ssid, success);
}

void NetworkMonitor::handleConnectionResult(const std::string& ssid, bool success) {
    if (success) {
        resetConnectionAttempts();
    }

    updateState();
    publishStateChange();

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (connection_callback_ && config_.publish_connection_events) {
            connection_callback_(ssid, success);
        }
    }
}

void NetworkMonitor::updateState() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    auto interface_result = api_->getInterface(config_.interface_name);
    if (!interface_result.isOk()) {
        current_state_.interface_state = state::InterfaceState::Unknown;
        return;
    }

    auto interface = interface_result.value();
    
    current_state_.interface_name = interface.name();
    current_state_.is_monitoring_active = running_;
    current_state_.last_updated = std::chrono::system_clock::now();

    if (connection_mgr_->isConnected(interface)) {
        current_state_.interface_state = state::InterfaceState::Connected;
        current_state_.connection_state = state::ConnectionState::Connected;
        
        auto ssid = connection_mgr_->getCurrentSSID(interface);
        if (ssid) {
            state::CurrentConnection conn;
            conn.ssid = *ssid;
            conn.connected_at = std::chrono::system_clock::now();
            current_state_.connection = conn;
        }
    } else {
        current_state_.interface_state = state::InterfaceState::Up;
        current_state_.connection_state = state::ConnectionState::Disconnected;
        current_state_.connection = std::nullopt;
    }
}

void NetworkMonitor::publishStateChange() {
    if (!config_.publish_state_changes || !event_publisher_) {
        return;
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (state_change_callback_) {
        state_change_callback_(current_state_);
    }
}

std::optional<config::NetworkProfile> NetworkMonitor::findBestNetwork(
    const std::vector<NetworkInfo>& available) {
    
    auto saved = saved_networks_->getSortedByPriority();
    
    for (const auto& profile : saved) {
        if (!profile.auto_connect) {
            continue;
        }

        for (const auto& network : available) {
            if (network.ssid() == profile.ssid) {
                if (network.signalStrength() >= config_.minimum_signal_strength) {
                    return profile;
                }
            }
        }
    }

    return std::nullopt;
}

bool NetworkMonitor::isNetworkInRange(
    const std::string& ssid,
    const std::vector<NetworkInfo>& available) {
    
    auto it = std::find_if(available.begin(), available.end(),
        [&ssid](const NetworkInfo& net) {
            return net.ssid() == ssid;
        });
    
    return it != available.end();
}

bool NetworkMonitor::shouldAttemptReconnection() {
    if (!config_.auto_reconnect_enabled) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_connection_attempt_);

    return elapsed >= config_.reconnect_delay;
}

void NetworkMonitor::resetConnectionAttempts() {
    connection_attempts_ = 0;
}

void NetworkMonitor::incrementConnectionAttempts() {
    connection_attempts_++;
}

} // namespace monitor
} // namespace urwt

namespace urwt {
namespace monitor {

void to_json(nlohmann::json& j, const MonitorConfig& config) {
    j = nlohmann::json{
        {"scan_interval_seconds", config.scan_interval.count()},
        {"scan_timeout_seconds", config.scan_timeout.count()},
        {"continuous_scanning", config.continuous_scanning},
        {"auto_connect_enabled", config.auto_connect_enabled},
        {"auto_reconnect_enabled", config.auto_reconnect_enabled},
        {"reconnect_delay_seconds", config.reconnect_delay.count()},
        {"connection_timeout_seconds", config.connection_timeout.count()},
        {"max_connection_attempts", config.max_connection_attempts},
        {"cache_duration_seconds", config.cache_duration.count()},
        {"use_cached_results", config.use_cached_results},
        {"publish_scan_events", config.publish_scan_events},
        {"publish_connection_events", config.publish_connection_events},
        {"publish_state_changes", config.publish_state_changes},
        {"event_topic_prefix", config.event_topic_prefix},
        {"interface_name", config.interface_name},
        {"minimum_signal_strength", config.minimum_signal_strength},
        {"preferred_signal_strength", config.preferred_signal_strength}
    };
}

void from_json(const nlohmann::json& j, MonitorConfig& config) {
    if (j.contains("scan_interval_seconds")) {
        config.scan_interval = std::chrono::seconds(
            j.at("scan_interval_seconds").get<int>());
    }
    if (j.contains("scan_timeout_seconds")) {
        config.scan_timeout = std::chrono::seconds(
            j.at("scan_timeout_seconds").get<int>());
    }
    if (j.contains("continuous_scanning")) {
        config.continuous_scanning = j.at("continuous_scanning").get<bool>();
    }
    if (j.contains("auto_connect_enabled")) {
        config.auto_connect_enabled = j.at("auto_connect_enabled").get<bool>();
    }
    if (j.contains("auto_reconnect_enabled")) {
        config.auto_reconnect_enabled = j.at("auto_reconnect_enabled").get<bool>();
    }
    if (j.contains("reconnect_delay_seconds")) {
        config.reconnect_delay = std::chrono::seconds(
            j.at("reconnect_delay_seconds").get<int>());
    }
    if (j.contains("connection_timeout_seconds")) {
        config.connection_timeout = std::chrono::seconds(
            j.at("connection_timeout_seconds").get<int>());
    }
    if (j.contains("max_connection_attempts")) {
        config.max_connection_attempts = j.at("max_connection_attempts").get<size_t>();
    }
    if (j.contains("cache_duration_seconds")) {
        config.cache_duration = std::chrono::seconds(
            j.at("cache_duration_seconds").get<int>());
    }
    if (j.contains("use_cached_results")) {
        config.use_cached_results = j.at("use_cached_results").get<bool>();
    }
    if (j.contains("publish_scan_events")) {
        config.publish_scan_events = j.at("publish_scan_events").get<bool>();
    }
    if (j.contains("publish_connection_events")) {
        config.publish_connection_events = j.at("publish_connection_events").get<bool>();
    }
    if (j.contains("publish_state_changes")) {
        config.publish_state_changes = j.at("publish_state_changes").get<bool>();
    }
    if (j.contains("event_topic_prefix")) {
        config.event_topic_prefix = j.at("event_topic_prefix").get<std::string>();
    }
    if (j.contains("interface_name")) {
        config.interface_name = j.at("interface_name").get<std::string>();
    }
    if (j.contains("minimum_signal_strength")) {
        config.minimum_signal_strength = j.at("minimum_signal_strength").get<int>();
    }
    if (j.contains("preferred_signal_strength")) {
        config.preferred_signal_strength = j.at("preferred_signal_strength").get<int>();
    }
}

} // namespace monitor
} // namespace urwt
