#include "urwt/mode/mode_controller.hpp"
#include "urwt/utils/process_executor.hpp"
#include <thread>
#include <chrono>
#include <optional>

namespace urwt {
namespace mode {

ModeController::ModeController(
    std::shared_ptr<WirelessToolsAPI> api,
    std::shared_ptr<STAModeManager> sta_manager,
    std::shared_ptr<APModeManager> ap_manager)
    : api_(api)
    , sta_manager_(sta_manager)
    , ap_manager_(ap_manager) {
}

ModeController::~ModeController() = default;

Result<bool, std::string> ModeController::switchMode(
    WirelessMode target_mode,
    const WirelessConfig& config) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto validate_result = validateModeSwitch(current_mode_, target_mode);
    if (!validate_result.isOk()) {
        return validate_result;
    }

    auto stop_result = stopCurrentMode();
    if (!stop_result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to stop current mode: " + stop_result.error());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (target_mode == WirelessMode::STA) {
        return switchToSTA(config.sta_mode);
    } else if (target_mode == WirelessMode::AP) {
        return switchToAP(config.ap_mode);
    }

    return Result<bool, std::string>::error("Invalid target mode");
}

Result<bool, std::string> ModeController::switchToSTA(const STAModeConfig& config) {
    auto cleanup_result = cleanupBeforeSwitch(config.interface);
    if (!cleanup_result.isOk()) {
        return cleanup_result;
    }

    auto enable_result = sta_manager_->enableSTAMode(config);
    if (!enable_result.isOk()) {
        return enable_result;
    }

    current_mode_ = WirelessMode::STA;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ModeController::switchToAP(const APModeConfig& config) {
    auto cleanup_result = cleanupBeforeSwitch(config.interface);
    if (!cleanup_result.isOk()) {
        return cleanup_result;
    }

    auto start_result = ap_manager_->startAP(config);
    if (!start_result.isOk()) {
        return start_result;
    }

    current_mode_ = WirelessMode::AP;
    return Result<bool, std::string>::ok(true);
}

WirelessMode ModeController::getCurrentMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_mode_;
}

Result<bool, std::string> ModeController::detectCurrentMode(const std::string& interface) {
    ProcessExecutor executor;
    auto result = executor.executeShell(
        "iw dev " + interface + " info 2>/dev/null",
        std::chrono::milliseconds(3000));

    if (!result.isOk()) {
        current_mode_ = WirelessMode::Unknown;
        return Result<bool, std::string>::ok(true);
    }

    std::string output = result.value().stdout_output;

    if (output.find("type AP") != std::string::npos) {
        current_mode_ = WirelessMode::AP;
    } else if (output.find("type managed") != std::string::npos ||
               output.find("type station") != std::string::npos) {
        current_mode_ = WirelessMode::STA;
    } else {
        current_mode_ = WirelessMode::Unknown;
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ModeController::validateModeSwitch(
    WirelessMode from, 
    WirelessMode to) {

    if (from == to) {
        return Result<bool, std::string>::error(
            "Already in target mode");
    }

    if (to == WirelessMode::Unknown) {
        return Result<bool, std::string>::error(
            "Cannot switch to unknown mode");
    }

    return Result<bool, std::string>::ok(true);
}

bool ModeController::canSwitchMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_mode_ != WirelessMode::Unknown;
}

Result<bool, std::string> ModeController::stopCurrentMode() {
    if (current_mode_ == WirelessMode::AP) {
        auto stop_result = ap_manager_->stopAP();
        if (!stop_result.isOk()) {
            return stop_result;
        }
    }

    current_mode_ = WirelessMode::Unknown;
    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ModeController::bringToNeutralState(
    const std::string& interface) {

    auto stop_result = stopCurrentMode();
    if (!stop_result.isOk()) {
        return stop_result;
    }

    ProcessExecutor executor;

    auto down_result = executor.executeShell(
        "ip link set " + interface + " down 2>/dev/null",
        std::chrono::milliseconds(3000));

    if (!down_result.isOk() || down_result.value().exit_code != 0) {
        return Result<bool, std::string>::error(
            "Failed to bring interface down");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto up_result = executor.executeShell(
        "ip link set " + interface + " up 2>/dev/null",
        std::chrono::milliseconds(3000));

    if (!up_result.isOk() || up_result.value().exit_code != 0) {
        return Result<bool, std::string>::error(
            "Failed to bring interface up");
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ModeController::cleanupBeforeSwitch(
    const std::string& interface) {

    std::cout << "[ModeController] Cleaning up before mode switch on " << interface << std::endl;

    auto validate_result = validateInterface(interface);
    if (!validate_result.isOk()) {
        std::cerr << "[ModeController] Interface validation failed: " << validate_result.error() << std::endl;
        return validate_result;
    }

    ProcessExecutor executor;

    std::cout << "[ModeController] Stopping wpa_supplicant..." << std::endl;
    auto wpaResult = executor.executeShell(
        "killall wpa_supplicant 2>/dev/null",
        std::chrono::milliseconds(2000));
    if (wpaResult.isOk()) {
        std::cout << "[ModeController] wpa_supplicant kill result: exit=" << wpaResult.value().exit_code << std::endl;
    }

    std::cout << "[ModeController] Disabling NetworkManager wifi radio..." << std::endl;
    auto nmResult = executor.executeShell(
        "nmcli radio wifi off 2>/dev/null",
        std::chrono::milliseconds(2000));
    if (nmResult.isOk()) {
        std::cout << "[ModeController] nmcli result: exit=" << nmResult.value().exit_code << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[ModeController] Cleanup completed" << std::endl;

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ModeController::validateInterface(
    const std::string& interface) {

    auto interfaces_result = api_->listInterfaces();
    if (!interfaces_result.isOk()) {
        return Result<bool, std::string>::error(
            "Failed to list interfaces");
    }

    bool found = false;
    for (const auto& iface : interfaces_result.value()) {
        if (iface.name() == interface) {
            found = true;
            break;
        }
    }

    if (!found) {
        return Result<bool, std::string>::error(
            "Interface not found: " + interface);
    }

    return Result<bool, std::string>::ok(true);
}

Result<bool, std::string> ModeController::setMode(WirelessMode mode) {
    return setMode(mode, std::nullopt, std::nullopt);
}

Result<bool, std::string> ModeController::setMode(WirelessMode mode, 
                                                   std::optional<STAModeConfig> sta_config,
                                                   std::optional<APModeConfig> ap_config) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[ModeController] setMode called: current=" << static_cast<int>(current_mode_) 
              << ", target=" << static_cast<int>(mode) << std::endl;

    if (mode == current_mode_) {
        std::cout << "[ModeController] Already in target mode, no action needed" << std::endl;
        return Result<bool, std::string>::ok(true);
    }

    if (mode == WirelessMode::STA) {
        if (!sta_config.has_value()) {
            std::cerr << "[ModeController] STA mode config required but not provided" << std::endl;
            return Result<bool, std::string>::error("STA mode config required");
        }
        std::cout << "[ModeController] Switching to STA mode..." << std::endl;
        return switchToSTA(sta_config.value());
    } else if (mode == WirelessMode::AP) {
        if (!ap_config.has_value()) {
            std::cerr << "[ModeController] AP mode config required but not provided" << std::endl;
            return Result<bool, std::string>::error("AP mode config required");
        }
        std::cout << "[ModeController] Switching to AP mode..." << std::endl;
        return switchToAP(ap_config.value());
    }

    std::cerr << "[ModeController] Invalid wireless mode requested" << std::endl;
    return Result<bool, std::string>::error("Invalid wireless mode");
}

}
}