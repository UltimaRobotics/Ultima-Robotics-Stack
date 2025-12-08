#ifndef URWT_MONITOR_NETWORK_MONITOR_HPP
#define URWT_MONITOR_NETWORK_MONITOR_HPP

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include "urwt/api.hpp"
#include "urwt/network/saved_network_manager.hpp"
#include "urwt/network/connection_manager.hpp"
#include "urwt/monitor/scan_result_cache.hpp"
#include "urwt/monitor/monitor_config.hpp"
#include "urwt/state/wireless_state.hpp"

namespace urwt {

namespace events {
class EventPublisher;
}

namespace monitor {

using ScanCallback = std::function<void(const ScanResult&)>;
using ConnectionCallback = std::function<void(const std::string& ssid, bool success)>;
using StateChangeCallback = std::function<void(const state::SystemWirelessState&)>;

class NetworkMonitor {
public:
    NetworkMonitor(std::shared_ptr<WirelessToolsAPI> api,
                  std::shared_ptr<network::SavedNetworkManager> saved_networks,
                  std::shared_ptr<network::ConnectionManager> connection_mgr,
                  std::shared_ptr<events::EventPublisher> event_publisher,
                  const MonitorConfig& config);
    ~NetworkMonitor();

    // Lifecycle
    Result<bool, std::string> start();
    Result<bool, std::string> stop();
    bool isRunning() const;

    // Configuration
    void setConfig(const MonitorConfig& config);
    MonitorConfig getConfig() const;

    // Control
    void pauseMonitoring();
    void resumeMonitoring();
    bool isPaused() const;

    void triggerImmediateScan();

    // Callbacks
    void setScanCallback(ScanCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    void setStateChangeCallback(StateChangeCallback callback);

    // Status
    state::SystemWirelessState getCurrentState() const;
    std::optional<ScanResult> getLastScanResult() const;
    std::chrono::system_clock::time_point getLastScanTime() const;
    size_t getConnectionAttempts() const;

private:
    // Thread management
    void monitorLoop();
    void scanNetworks();
    void processAvailableNetworks(const ScanResult& result);
    void attemptConnection(const config::NetworkProfile& profile);
    void handleConnectionResult(const std::string& ssid, bool success);
    void updateState();
    void publishStateChange();

    // Helper methods
    std::optional<config::NetworkProfile> findBestNetwork(
        const std::vector<NetworkInfo>& available);
    bool isNetworkInRange(const std::string& ssid, 
                         const std::vector<NetworkInfo>& available);
    bool shouldAttemptReconnection();
    void resetConnectionAttempts();
    void incrementConnectionAttempts();

    // State
    std::shared_ptr<WirelessToolsAPI> api_;
    std::shared_ptr<network::SavedNetworkManager> saved_networks_;
    std::shared_ptr<network::ConnectionManager> connection_mgr_;
    std::shared_ptr<events::EventPublisher> event_publisher_;
    std::shared_ptr<ScanResultCache> scan_cache_;

    MonitorConfig config_;

    // Threading
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> immediate_scan_requested_{false};
    mutable std::mutex state_mutex_;
    std::condition_variable cv_;

    // Callbacks
    mutable std::mutex callback_mutex_;
    ScanCallback scan_callback_;
    ConnectionCallback connection_callback_;
    StateChangeCallback state_change_callback_;

    // State tracking
    state::SystemWirelessState current_state_;
    std::optional<ScanResult> last_scan_result_;
    std::chrono::system_clock::time_point last_scan_time_;
    std::atomic<size_t> connection_attempts_{0};
    std::string last_connected_ssid_;
    std::chrono::system_clock::time_point last_connection_attempt_;
};

} // namespace monitor
} // namespace urwt

#endif // URWT_MONITOR_NETWORK_MONITOR_HPP
