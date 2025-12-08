#include "urwt/managers/interface_detector.hpp"
#include <regex>
#include <sstream>
#include <fstream>
#include <iostream> // Include for std::cerr

namespace urwt {

InterfaceDetector::InterfaceDetector(std::shared_ptr<ProcessExecutor> executor)
    : executor_(executor ? executor : std::make_shared<ProcessExecutor>()) {}

Result<std::vector<WifiInterface>, std::string> InterfaceDetector::detectInterfaces() {
    auto names_result = listWirelessInterfaces();
    if (names_result.isError()) {
        return Result<std::vector<WifiInterface>, std::string>::error(names_result.error());
    }

    std::vector<WifiInterface> interfaces;
    for (const auto& name : names_result.value()) {
        auto iface_result = getInterfaceInfo(name);
        if (iface_result.isOk()) {
            interfaces.push_back(iface_result.value());
        }
    }

    return Result<std::vector<WifiInterface>, std::string>::ok(interfaces);
}

Result<WifiInterface, std::string> InterfaceDetector::getInterfaceInfo(const std::string& name) {
    auto result = executor_->execute("iw", {"dev", name, "info"});

    if (result.isError()) {
        return Result<WifiInterface, std::string>::error(result.error());
    }

    if (result.value().exit_code != 0) {
        return Result<WifiInterface, std::string>::error(
            "Failed to get interface info for " + name
        );
    }

    WifiInterface iface = parseInterfaceInfo(name, result.value().stdout_output);
    iface.setStatus(detectStatus(name));

    return Result<WifiInterface, std::string>::ok(iface);
}

Result<std::vector<std::string>, std::string> InterfaceDetector::listWirelessInterfaces() {
    auto result = executor_->execute("iw", {"dev"});

    if (result.isError()) {
        return Result<std::vector<std::string>, std::string>::error(result.error());
    }

    if (result.value().exit_code != 0) {
        return Result<std::vector<std::string>, std::string>::error(
            "Failed to list interfaces"
        );
    }

    std::vector<std::string> names;
    std::istringstream stream(result.value().stdout_output);
    std::string line;
    std::regex interface_regex("Interface\\s+(\\S+)");

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, interface_regex)) {
            names.push_back(match[1].str());
        }
    }

    return Result<std::vector<std::string>, std::string>::ok(names);
}

WifiInterface InterfaceDetector::parseInterfaceInfo(const std::string& name, const std::string& iwOutput) {
    WifiInterface iface(name);

    MacAddress mac = parseMacAddress(iwOutput);
    iface.setMac(mac);

    auto ssid = parseSSID(iwOutput);
    if (ssid) {
        iface.setSsid(*ssid);
    }

    auto freq = parseFrequency(iwOutput);
    if (freq) {
        iface.setFrequency(*freq);
    }

    auto signal = parseSignalStrength(iwOutput);
    if (signal) {
        iface.setSignalStrength(*signal);
    }

    return iface;
}

MacAddress InterfaceDetector::parseMacAddress(const std::string& output) {
    std::regex mac_regex("addr\\s+([0-9a-fA-F:]{17})");
    std::smatch match;

    if (std::regex_search(output, match, mac_regex)) {
        return MacAddress(match[1].str());
    }

    return MacAddress("00:00:00:00:00:00");
}

std::optional<std::string> InterfaceDetector::parseSSID(const std::string& output) {
    std::regex ssid_regex("ssid\\s+(.+)");
    std::smatch match;

    if (std::regex_search(output, match, ssid_regex)) {
        return match[1].str();
    }

    return std::nullopt;
}

std::optional<int> InterfaceDetector::parseFrequency(const std::string& output) {
    std::regex freq_regex("channel\\s+\\d+\\s+\\((\\d+)\\s+MHz\\)");
    std::smatch match;

    if (std::regex_search(output, match, freq_regex)) {
        return std::stoi(match[1].str());
    }

    return std::nullopt;
}

std::optional<int> InterfaceDetector::parseSignalStrength(const std::string& output) {
    return std::nullopt;
}

InterfaceStatus InterfaceDetector::detectStatus(const std::string& name) {
    std::string path = "/sys/class/net/" + name + "/operstate";
    std::ifstream file(path);

    if (!file.is_open()) {
        return InterfaceStatus::Unknown;
    }

    std::string state;
    std::getline(file, state);

    if (state == "up") {
        return InterfaceStatus::Up;
    } else if (state == "down") {
        return InterfaceStatus::Down;
    }

    return InterfaceStatus::Unknown;
}

}