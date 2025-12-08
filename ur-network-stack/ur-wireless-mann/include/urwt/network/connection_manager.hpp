#ifndef URWT_NETWORK_CONNECTION_MANAGER_HPP
#define URWT_NETWORK_CONNECTION_MANAGER_HPP

#include <memory>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include "urwt/api.hpp"
#include "urwt/config/wireless_config_types.hpp"
#include "urwt/models/connection_test_result.hpp"
#include "urwt/utils/result.hpp"

namespace urwt {
namespace network {

using namespace config;

enum class ConnectionMethod {
    NetworkManager,
    WpaSupplicant,
    IwConnect
};

struct ConnectionProgress {
    std::string ssid;
    std::string state;
    int progress_percentage{0};
    std::string message;
    std::chrono::system_clock::time_point started_at;

    std::chrono::seconds elapsed() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - started_at);
    }
};

class ConnectionManager {
public:
    explicit ConnectionManager(std::shared_ptr<WirelessToolsAPI> api);
    ~ConnectionManager();

    Result<bool, std::string> connect(const NetworkProfile& profile,
                                     const WifiInterface& interface);
    Result<bool, std::string> disconnect(const WifiInterface& interface);
    Result<bool, std::string> reconnect(const WifiInterface& interface);

    Result<bool, std::string> connectWithTimeout(
        const NetworkProfile& profile,
        const WifiInterface& interface,
        std::chrono::seconds timeout);

    Result<ConnectionTestResult, std::string> testConnection(
        const NetworkProfile& profile,
        const WifiInterface& interface);

    bool isConnected(const WifiInterface& interface) const;
    std::optional<std::string> getCurrentSSID(const WifiInterface& interface) const;
    std::optional<ConnectionProgress> getConnectionProgress() const;

    void setConnectionMethod(ConnectionMethod method);
    ConnectionMethod getConnectionMethod() const;
    void setTimeout(std::chrono::seconds timeout);

private:
    std::shared_ptr<WirelessToolsAPI> api_;

    mutable std::mutex progress_mutex_;
    std::optional<ConnectionProgress> current_progress_;
    std::atomic<bool> connection_in_progress_{false};

    ConnectionMethod method_{ConnectionMethod::NetworkManager};
    std::chrono::seconds timeout_{std::chrono::seconds(30)};

    Result<bool, std::string> connectNetworkManager(
        const NetworkProfile& profile,
        const WifiInterface& interface);
    Result<bool, std::string> connectWpaSupplicant(
        const NetworkProfile& profile,
        const WifiInterface& interface);
    Result<bool, std::string> connectIw(
        const NetworkProfile& profile,
        const WifiInterface& interface);

    void updateProgress(const std::string& state, int percentage, 
                       const std::string& message);
    void clearProgress();
    Result<std::string, std::string> buildWpaSupplicantConfig(
        const NetworkProfile& profile);
    Result<bool, std::string> waitForConnection(
        const WifiInterface& interface,
        std::chrono::seconds timeout);
    Result<bool, std::string> obtainIPAddress(const WifiInterface& interface);
};

} // namespace network
} // namespace urwt

#endif // URWT_NETWORK_CONNECTION_MANAGER_HPP
