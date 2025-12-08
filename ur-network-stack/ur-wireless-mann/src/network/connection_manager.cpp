#include "urwt/network/connection_manager.hpp"
#include "urwt/utils/process_executor.hpp"
#include <sstream>
#include <fstream>
#include <thread>
#include <iomanip>

namespace urwt {
namespace network {

ConnectionManager::ConnectionManager(std::shared_ptr<WirelessToolsAPI> api)
    : api_(api) {
}

ConnectionManager::~ConnectionManager() {
}

Result<bool, std::string> ConnectionManager::connect(
    const NetworkProfile& profile,
    const WifiInterface& interface) {
    
    return connectWithTimeout(profile, interface, timeout_);
}

Result<bool, std::string> ConnectionManager::disconnect(const WifiInterface& interface) {
    if (connection_in_progress_) {
        return Result<bool, std::string>::error("Connection operation in progress");
    }

    ProcessExecutor executor;
    Result<ProcessResult, std::string> result = Result<ProcessResult, std::string>::error("Not executed");

    switch (method_) {
        case ConnectionMethod::NetworkManager: {
            result = executor.execute("nmcli", {"device", "disconnect", interface.name()});
            break;
        }
        case ConnectionMethod::WpaSupplicant: {
            result = executor.execute("killall", {"wpa_supplicant"});
            break;
        }
        case ConnectionMethod::IwConnect: {
            result = executor.execute("iw", {"dev", interface.name(), "disconnect"});
            break;
        }
    }

    if (result.isError()) {
        return Result<bool, std::string>::error("Failed to disconnect: " + result.error());
    }

    if (result.value().exit_code != 0) {
        return Result<bool, std::string>::error("Disconnect failed: " + result.value().stderr_output);
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ConnectionManager::reconnect(const WifiInterface& interface) {
    auto current_ssid = getCurrentSSID(interface);
    if (!current_ssid.has_value()) {
        return Result<bool, std::string>::error("No current connection to reconnect");
    }

    return Result<bool, std::string>::error("Reconnect not implemented - need saved network profile");
}

Result<bool, std::string> ConnectionManager::connectWithTimeout(
    const NetworkProfile& profile,
    const WifiInterface& interface,
    std::chrono::seconds timeout) {
    
    if (connection_in_progress_) {
        return Result<bool, std::string>::error("Connection already in progress");
    }

    connection_in_progress_ = true;
    clearProgress();

    auto start_time = std::chrono::system_clock::now();
    updateProgress("connecting", 0, "Starting connection to " + profile.ssid);

    Result<bool, std::string> result = Result<bool, std::string>::error("Not executed");

    switch (method_) {
        case ConnectionMethod::NetworkManager:
            result = connectNetworkManager(profile, interface);
            break;
        case ConnectionMethod::WpaSupplicant:
            result = connectWpaSupplicant(profile, interface);
            break;
        case ConnectionMethod::IwConnect:
            result = connectIw(profile, interface);
            break;
    }

    if (result.isOk()) {
        updateProgress("authenticating", 50, "Waiting for authentication");
        
        auto wait_result = waitForConnection(interface, timeout);
        if (wait_result.isError()) {
            connection_in_progress_ = false;
            clearProgress();
            return wait_result;
        }

        updateProgress("obtaining_ip", 75, "Obtaining IP address");
        
        auto ip_result = obtainIPAddress(interface);
        if (ip_result.isError()) {
            connection_in_progress_ = false;
            clearProgress();
            return ip_result;
        }

        updateProgress("connected", 100, "Successfully connected to " + profile.ssid);
    }

    connection_in_progress_ = false;
    clearProgress();
    
    return result;
}

Result<ConnectionTestResult, std::string> ConnectionManager::testConnection(
    const NetworkProfile& profile,
    const WifiInterface& interface) {
    
    if (!api_) {
        return Result<ConnectionTestResult, std::string>::error("WirelessToolsAPI not initialized");
    }

    return api_->testConnection(interface, profile.ssid);
}

bool ConnectionManager::isConnected(const WifiInterface& interface) const {
    return interface.isConnected();
}

std::optional<std::string> ConnectionManager::getCurrentSSID(const WifiInterface& interface) const {
    return interface.ssid();
}

std::optional<ConnectionProgress> ConnectionManager::getConnectionProgress() const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return current_progress_;
}

void ConnectionManager::setConnectionMethod(ConnectionMethod method) {
    method_ = method;
}

ConnectionMethod ConnectionManager::getConnectionMethod() const {
    return method_;
}

void ConnectionManager::setTimeout(std::chrono::seconds timeout) {
    timeout_ = timeout;
}

Result<bool, std::string> ConnectionManager::connectNetworkManager(
    const NetworkProfile& profile,
    const WifiInterface& interface) {
    
    ProcessExecutor executor;
    std::vector<std::string> args = {"device", "wifi", "connect", profile.ssid};

    if (!profile.password.empty()) {
        args.push_back("password");
        args.push_back(profile.password);
    }

    if (profile.bssid.has_value()) {
        args.push_back("bssid");
        args.push_back(*profile.bssid);
    }

    args.push_back("ifname");
    args.push_back(interface.name());

    if (profile.hidden) {
        args.push_back("hidden");
        args.push_back("yes");
    }

    auto result = executor.execute("nmcli", args);

    if (result.isError()) {
        return Result<bool, std::string>::error("Failed to execute nmcli: " + result.error());
    }

    if (result.value().exit_code != 0) {
        return Result<bool, std::string>::error("nmcli failed: " + result.value().stderr_output);
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ConnectionManager::connectWpaSupplicant(
    const NetworkProfile& profile,
    const WifiInterface& interface) {
    
    auto config_result = buildWpaSupplicantConfig(profile);
    if (config_result.isError()) {
        return Result<bool, std::string>::error("Failed to build config: " + config_result.error());
    }

    std::string config_path = "/tmp/wpa_supplicant_" + interface.name() + ".conf";
    std::ofstream config_file(config_path);
    if (!config_file.is_open()) {
        return Result<bool, std::string>::error("Failed to create config file");
    }
    config_file << config_result.value();
    config_file.close();

    ProcessExecutor executor;
    
    executor.execute("killall", {"wpa_supplicant"});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto result = executor.execute("wpa_supplicant", 
        {"-B", "-i", interface.name(), "-c", config_path});

    if (result.isError()) {
        return Result<bool, std::string>::error("Failed to start wpa_supplicant: " + result.error());
    }

    if (result.value().exit_code != 0) {
        return Result<bool, std::string>::error("wpa_supplicant failed: " + result.value().stderr_output);
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ConnectionManager::connectIw(
    const NetworkProfile& profile,
    const WifiInterface& interface) {
    
    ProcessExecutor executor;

    auto disconnect_result = executor.execute("iw", {"dev", interface.name(), "disconnect"});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::vector<std::string> args = {"dev", interface.name(), "connect", profile.ssid};

    if (profile.security == config::SecurityType::Open) {
    } else if (profile.security == config::SecurityType::WEP) {
        args.push_back("key");
        args.push_back("0:" + profile.password);
    } else {
        return Result<bool, std::string>::error("iw method only supports Open and WEP security");
    }

    auto result = executor.execute("iw", args);

    if (result.isError()) {
        return Result<bool, std::string>::error("Failed to execute iw: " + result.error());
    }

    if (result.value().exit_code != 0) {
        return Result<bool, std::string>::error("iw connect failed: " + result.value().stderr_output);
    }

    return Result<bool, std::string>::ok(true);
}

void ConnectionManager::updateProgress(const std::string& state, int percentage, 
                                      const std::string& message) {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    
    if (!current_progress_.has_value()) {
        current_progress_ = ConnectionProgress();
        current_progress_->started_at = std::chrono::system_clock::now();
    }
    
    current_progress_->state = state;
    current_progress_->progress_percentage = percentage;
    current_progress_->message = message;
}

void ConnectionManager::clearProgress() {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    current_progress_.reset();
}

Result<std::string, std::string> ConnectionManager::buildWpaSupplicantConfig(
    const NetworkProfile& profile) {
    
    std::ostringstream config;
    
    config << "network={\n";
    config << "    ssid=\"" << profile.ssid << "\"\n";

    if (profile.hidden) {
        config << "    scan_ssid=1\n";
    }

    switch (profile.security) {
        case config::SecurityType::Open:
            config << "    key_mgmt=NONE\n";
            break;
            
        case config::SecurityType::WEP:
            config << "    key_mgmt=NONE\n";
            config << "    wep_key0=\"" << profile.password << "\"\n";
            config << "    wep_tx_keyidx=0\n";
            break;
            
        case config::SecurityType::WPA:
        case config::SecurityType::WPA2:
            config << "    key_mgmt=WPA-PSK\n";
            config << "    psk=\"" << profile.password << "\"\n";
            break;
            
        case config::SecurityType::WPA3:
            config << "    key_mgmt=SAE\n";
            config << "    psk=\"" << profile.password << "\"\n";
            break;
            
        case config::SecurityType::WPA2Enterprise:
            config << "    key_mgmt=WPA-EAP\n";
            if (profile.identity.has_value()) {
                config << "    identity=\"" << *profile.identity << "\"\n";
            }
            if (profile.password.empty() == false) {
                config << "    password=\"" << profile.password << "\"\n";
            }
            if (profile.ca_cert.has_value()) {
                config << "    ca_cert=\"" << *profile.ca_cert << "\"\n";
            }
            if (profile.client_cert.has_value()) {
                config << "    client_cert=\"" << *profile.client_cert << "\"\n";
            }
            if (profile.private_key.has_value()) {
                config << "    private_key=\"" << *profile.private_key << "\"\n";
            }
            break;
            
        default:
            return Result<std::string, std::string>::error("Unsupported security type");
    }

    if (profile.bssid.has_value()) {
        config << "    bssid=" << *profile.bssid << "\n";
    }

    config << "    priority=" << profile.priority << "\n";
    config << "}\n";

    return Result<std::string, std::string>::ok(config.str());
}

Result<bool, std::string> ConnectionManager::waitForConnection(
    const WifiInterface& interface,
    std::chrono::seconds timeout) {
    
    ProcessExecutor executor;
    auto start_time = std::chrono::system_clock::now();
    
    while (true) {
        auto elapsed = std::chrono::system_clock::now() - start_time;
        if (elapsed >= timeout) {
            return Result<bool, std::string>::error("Connection timeout");
        }

        auto result = executor.execute("iw", {"dev", interface.name(), "link"});
        
        if (result.isOk() && result.value().exit_code == 0) {
            std::string output = result.value().stdout_output;
            if (output.find("Connected to") != std::string::npos || 
                output.find("SSID:") != std::string::npos) {
                return Result<bool, std::string>::ok(true);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

Result<bool, std::string> ConnectionManager::obtainIPAddress(const WifiInterface& interface) {
    ProcessExecutor executor;

    if (method_ == ConnectionMethod::NetworkManager) {
        return Result<bool, std::string>::ok(true);
    }

    auto dhclient_result = executor.execute("dhclient", {interface.name()});
    
    if (dhclient_result.isError()) {
        return Result<bool, std::string>::error("Failed to run dhclient: " + dhclient_result.error());
    }

    if (dhclient_result.value().exit_code != 0) {
        auto dhcpcd_result = executor.execute("dhcpcd", {interface.name()});
        if (dhcpcd_result.isError() || dhcpcd_result.value().exit_code != 0) {
            return Result<bool, std::string>::error("Failed to obtain IP address");
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto ip_result = executor.execute("ip", {"addr", "show", interface.name()});
    if (ip_result.isOk() && ip_result.value().exit_code == 0) {
        if (ip_result.value().stdout_output.find("inet ") != std::string::npos) {
            return Result<bool, std::string>::ok(true);
        }
    }

    return Result<bool, std::string>::error("Failed to verify IP address");
}

} // namespace network
} // namespace urwt
