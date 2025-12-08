#include "urwt/config/startup_configurator.hpp"
#include "urwt/config/wireless_config_manager.hpp"
#include "urwt/utils/process_executor.hpp"
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>
#include <optional> // Required for std::nullopt

namespace urwt {
namespace config {

StartupConfigurator::StartupConfigurator(
    std::shared_ptr<WirelessToolsAPI> api,
    std::shared_ptr<mode::ModeController> modeController,
    std::shared_ptr<WirelessConfigManager> wirelessConfigManager)
    : api_(api)
    , mode_controller_(modeController)
    , wireless_config_manager_(wirelessConfigManager) {
}

StartupConfigurator::~StartupConfigurator() = default;

Result<bool, std::string> StartupConfigurator::applyConfiguration(
    const WirelessConfig& config) {

    std::cout << "Applying wireless configuration from file..." << std::endl;

    auto wifiStateResult = applyWiFiState(config.enabled);
    if (wifiStateResult.isError()) {
        return Result<bool, std::string>::error(
            "Failed to apply WiFi state: " + wifiStateResult.error());
    }

    if (!config.enabled) {
        std::cout << "WiFi is disabled per configuration" << std::endl;
        return Result<bool, std::string>::ok(true);
    }

    auto modeResult = applyMode(config.mode);
    if (modeResult.isError()) {
        return Result<bool, std::string>::error(
            "Failed to apply mode: " + modeResult.error());
    }

    if (config.mode == WirelessMode::STA) {
        auto staResult = applySTAConfiguration(config.sta_mode);
        if (staResult.isError()) {
            std::cerr << "Warning: Failed to apply STA configuration: "
                      << staResult.error() << std::endl;
        }
    } else if (config.mode == WirelessMode::AP) {
        auto apResult = applyAPConfiguration(config.ap_mode);
        if (apResult.isError()) {
            std::cerr << "Warning: Failed to apply AP configuration: "
                      << apResult.error() << std::endl;
        }
    }

    std::cout << "Wireless configuration applied successfully" << std::endl;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::applyWiFiState(bool enabled) {
    auto interfacesResult = api_->listInterfaces();
    if (interfacesResult.isError()) {
        std::cerr << "[StartupConfigurator] Failed to list interfaces: " << interfacesResult.error() << std::endl;
        return Result<bool, std::string>::error(interfacesResult.error());
    }

    if (interfacesResult.value().empty()) {
        std::cerr << "[StartupConfigurator] No wireless interfaces found" << std::endl;
        return Result<bool, std::string>::error("No wireless interfaces found");
    }

    std::string interface = interfacesResult.value()[0].name();
    ProcessExecutor executor;

    // Check current interface state
    auto linkCheckCmd = "ip link show " + interface + " 2>&1";
    std::cout << "[StartupConfigurator] Checking interface state: " << linkCheckCmd << std::endl;
    auto linkCheck = executor.executeShell(linkCheckCmd, std::chrono::seconds(5));
    
    bool isCurrentlyUp = false;
    if (linkCheck.isOk()) {
        std::cout << "[StartupConfigurator] Interface state output: " << linkCheck.value().stdout_output << std::endl;
        isCurrentlyUp = linkCheck.value().stdout_output.find("state UP") != std::string::npos;
        std::cout << "[StartupConfigurator] Interface " << interface << " is currently " << (isCurrentlyUp ? "UP" : "DOWN") << std::endl;
    }

    if (enabled) {
        std::cout << "[StartupConfigurator] Enabling WiFi on " << interface << std::endl;

        // Unblock RF-kill
        auto rfkillCmd = "rfkill unblock wifi 2>&1";
        std::cout << "[StartupConfigurator] Executing: " << rfkillCmd << std::endl;
        auto rfkillResult = executor.executeShell(rfkillCmd, std::chrono::seconds(5));
        if (rfkillResult.isOk()) {
            std::cout << "[StartupConfigurator] rfkill result (exit=" << rfkillResult.value().exit_code << "): " << rfkillResult.value().stdout_output << std::endl;
        }

        // Only bring interface up if it's not already up
        if (!isCurrentlyUp) {
            auto upCmd = "ip link set " + interface + " up 2>&1";
            std::cout << "[StartupConfigurator] Executing: " << upCmd << std::endl;
            auto result = executor.executeShell(upCmd, std::chrono::seconds(5));

            if (result.isError()) {
                std::cerr << "[StartupConfigurator] Failed to execute command: " << result.error() << std::endl;
                return Result<bool, std::string>::error("Failed to enable WiFi: " + result.error());
            }

            std::cout << "[StartupConfigurator] Command result (exit=" << result.value().exit_code << "): " << result.value().stdout_output << std::endl;
            
            if (result.value().exit_code != 0) {
                std::cerr << "[StartupConfigurator] Command failed with exit code " << result.value().exit_code << std::endl;
                std::cerr << "[StartupConfigurator] Command output: " << result.value().stdout_output << std::endl;
                return Result<bool, std::string>::error(
                    "Failed to bring interface up: Command failed with exit code " + 
                    std::to_string(result.value().exit_code));
            }
            std::cout << "[StartupConfigurator] Interface " << interface << " brought up successfully" << std::endl;
        } else {
            std::cout << "[StartupConfigurator] Interface " << interface << " is already up, skipping" << std::endl;
        }
    } else {
        std::cout << "[StartupConfigurator] Disabling WiFi on " << interface << std::endl;

        // Kill running services
        auto killWpaCmd = "killall wpa_supplicant 2>/dev/null";
        std::cout << "[StartupConfigurator] Executing: " << killWpaCmd << std::endl;
        executor.executeShell(killWpaCmd, std::chrono::seconds(5));
        
        auto killHostapdCmd = "killall hostapd 2>/dev/null";
        std::cout << "[StartupConfigurator] Executing: " << killHostapdCmd << std::endl;
        executor.executeShell(killHostapdCmd, std::chrono::seconds(5));

        // Only bring interface down if it's currently up
        if (isCurrentlyUp) {
            auto downCmd = "ip link set " + interface + " down 2>&1";
            std::cout << "[StartupConfigurator] Executing: " << downCmd << std::endl;
            auto result = executor.executeShell(downCmd, std::chrono::seconds(5));

            if (result.isError()) {
                std::cerr << "[StartupConfigurator] Failed to execute command: " << result.error() << std::endl;
                return Result<bool, std::string>::error("Failed to disable WiFi: " + result.error());
            }

            std::cout << "[StartupConfigurator] Command result (exit=" << result.value().exit_code << "): " << result.value().stdout_output << std::endl;
            
            if (result.value().exit_code != 0) {
                std::cerr << "[StartupConfigurator] Command failed with exit code " << result.value().exit_code << std::endl;
                return Result<bool, std::string>::error(
                    "Failed to bring interface down: Command failed with exit code " + 
                    std::to_string(result.value().exit_code));
            }
            std::cout << "[StartupConfigurator] Interface " << interface << " brought down successfully" << std::endl;
        } else {
            std::cout << "[StartupConfigurator] Interface " << interface << " is already down, skipping" << std::endl;
        }
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::applyMode(WirelessMode mode) {
    std::cout << "[StartupConfigurator] Applying mode: " << wirelessModeToString(mode) << std::endl;

    // Check current mode
    auto currentMode = mode_controller_->getCurrentMode();
    std::cout << "[StartupConfigurator] Current mode: " << wirelessModeToString(currentMode) << std::endl;
    
    if (currentMode == mode) {
        std::cout << "[StartupConfigurator] Already in target mode " << wirelessModeToString(mode) << ", skipping mode switch" << std::endl;
        return Result<bool, std::string>::ok(true);
    }

    // Get the full config
    auto config = wireless_config_manager_->getConfig();

    if (mode == WirelessMode::STA) {
        if (config.sta_mode.interface.empty()) {
            std::cerr << "[StartupConfigurator] No interface configured for STA mode" << std::endl;
            return Result<bool, std::string>::error(
                "No interface configured for STA mode");
        }
        std::cout << "[StartupConfigurator] Switching to STA mode on interface " << config.sta_mode.interface << std::endl;
        auto modeResult = mode_controller_->setMode(mode, config.sta_mode, std::nullopt);
        if (modeResult.isError()) {
            std::cerr << "[StartupConfigurator] Failed to switch to STA mode: " << modeResult.error() << std::endl;
            return Result<bool, std::string>::error(
                "Failed to apply mode: " + modeResult.error());
        }
        std::cout << "[StartupConfigurator] Successfully switched to STA mode" << std::endl;
    } else if (mode == WirelessMode::AP) {
        if (config.ap_mode.interface.empty()) {
            std::cerr << "[StartupConfigurator] No interface configured for AP mode" << std::endl;
            return Result<bool, std::string>::error(
                "No interface configured for AP mode");
        }
        std::cout << "[StartupConfigurator] Switching to AP mode on interface " << config.ap_mode.interface << std::endl;
        auto modeResult = mode_controller_->setMode(mode, std::nullopt, config.ap_mode);
        if (modeResult.isError()) {
            std::cerr << "[StartupConfigurator] Failed to switch to AP mode: " << modeResult.error() << std::endl;
            return Result<bool, std::string>::error(
                "Failed to apply mode: " + modeResult.error());
        }
        std::cout << "[StartupConfigurator] Successfully switched to AP mode" << std::endl;
    } else {
        std::cerr << "[StartupConfigurator] Invalid wireless mode" << std::endl;
        return Result<bool, std::string>::error("Invalid wireless mode");
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::applySTAConfiguration(
    const STAModeConfig& config) {

    std::cout << "Applying STA configuration..." << std::endl;

    if (!config.regulatory_domain.empty()) {
        ProcessExecutor executor;
        executor.executeShell("iw reg set " + config.regulatory_domain + " 2>&1",
                              std::chrono::seconds(5));
    }

    ProcessExecutor executor;
    std::string powerSave = config.power_save ? "on" : "off";
    executor.executeShell("iw dev " + config.interface + " set power_save " +
                          powerSave + " 2>&1",
                          std::chrono::seconds(5));

    if (!config.saved_networks.empty()) {
        auto networksResult = configureNetworks(config.saved_networks);
        if (networksResult.isError()) {
            std::cerr << "Warning: Failed to configure networks: "
                      << networksResult.error() << std::endl;
        }
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::applyAPConfiguration(
    const APModeConfig& config) {

    std::cout << "Applying AP configuration..." << std::endl;

    ProcessExecutor executor;

    auto ipResult = executor.executeShell(
        "ip addr add " + config.ip_address + "/24 dev " +
        config.interface + " 2>&1",
        std::chrono::seconds(5));

    if (ipResult.isError()) {
        return Result<bool, std::string>::error(
            "Failed to set AP IP address: " + ipResult.error());
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> StartupConfigurator::configureNetworks(
    const std::vector<NetworkProfile>& networks) {

    std::cout << "Configuring " << networks.size() << " saved networks..."
              << std::endl;

    std::stringstream conf;
    conf << "ctrl_interface=/var/run/wpa_supplicant\n";
    conf << "update_config=1\n\n";

    for (const auto& network : networks) {
        conf << "network={\n";
        conf << "  ssid=\"" << network.ssid << "\"\n";

        if (network.security == SecurityType::Open) {
            conf << "  key_mgmt=NONE\n";
        } else if (network.security == SecurityType::WPA2) {
            conf << "  key_mgmt=WPA-PSK\n";
            conf << "  psk=\"" << network.password << "\"\n";
        } else if (network.security == SecurityType::WPA2Enterprise) {
            conf << "  key_mgmt=WPA-EAP\n";
            if (network.identity) {
                conf << "  identity=\"" << *network.identity << "\"\n";
            }
            if (network.ca_cert) {
                conf << "  ca_cert=\"" << *network.ca_cert << "\"\n";
            }
            if (network.client_cert) {
                conf << "  client_cert=\"" << *network.client_cert << "\"\n";
            }
            if (network.private_key) {
                conf << "  private_key=\"" << *network.private_key << "\"\n";
            }
        }

        conf << "  priority=" << network.priority << "\n";

        if (network.hidden) {
            conf << "  scan_ssid=1\n";
        }

        if (network.bssid) {
            conf << "  bssid=" << *network.bssid << "\n";
        }

        conf << "}\n\n";
    }

    std::ofstream confFile("/tmp/wpa_supplicant.conf");
    if (!confFile.is_open()) {
        return Result<bool, std::string>::error(
            "Failed to create wpa_supplicant.conf");
    }

    confFile << conf.str();
    confFile.close();

    ProcessExecutor executor;
    auto result = executor.executeShell(
        "wpa_supplicant -B -i wlan0 -c /tmp/wpa_supplicant.conf 2>&1",
        std::chrono::seconds(10));

    if (result.isError()) {
        return Result<bool, std::string>::error(
            "Failed to start wpa_supplicant: " + result.error());
    }

    return Result<bool, std::string>::ok(true);
}

} // namespace config
} // namespace urwt