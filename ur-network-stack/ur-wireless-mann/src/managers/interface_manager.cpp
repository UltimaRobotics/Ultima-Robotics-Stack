#include "urwt/managers/interface_manager.hpp"
#include "urwt/managers/interface_detector.hpp"
#include <algorithm>
#include <iostream> // Required for std::cerr

namespace urwt {

InterfaceManager::InterfaceManager(std::shared_ptr<InterfaceDetector> detector)
    : detector_(detector ? detector : std::make_shared<InterfaceDetector>()) {}

Result<std::vector<WifiInterface>, std::string> InterfaceManager::listInterfaces() {
    std::cerr << "[InterfaceManager] Calling detector->detectInterfaces()" << std::endl;
    auto result = detector_->detectInterfaces();
    if (!result.isOk()) {
        std::cerr << "[InterfaceManager] Detector failed: " << result.error() << std::endl;
    } else {
        std::cerr << "[InterfaceManager] Detector found " << result.value().size() << " interfaces" << std::endl;
    }
    return result;
}

Result<WifiInterface, std::string> InterfaceManager::getInterface(const std::string& name) {
    auto interfaces = listInterfaces();
    if (!interfaces.isOk()) {
        std::cerr << "[InterfaceManager] Failed to list interfaces: " << interfaces.error() << std::endl;
        return Result<WifiInterface, std::string>::error(interfaces.error());
    }

    std::cerr << "[InterfaceManager] Found " << interfaces.value().size() << " interfaces" << std::endl;
    for (const auto& iface : interfaces.value()) {
        std::cerr << "[InterfaceManager] Checking interface: " << iface.name() << std::endl;
        if (iface.name() == name) {
            std::cerr << "[InterfaceManager] Found matching interface: " << name << std::endl;
            return Result<WifiInterface, std::string>::ok(iface);
        }
    }

    std::cerr << "[InterfaceManager] Interface not found: " << name << std::endl;
    return Result<WifiInterface, std::string>::error("Interface not found: " + name);
}

Result<WifiInterface, std::string> InterfaceManager::selectBestInterface() {
    auto result = listInterfaces();

    if (result.isError()) {
        return Result<WifiInterface, std::string>::error(result.error());
    }

    auto& interfaces = result.value();

    if (interfaces.empty()) {
        return Result<WifiInterface, std::string>::error("No wireless interfaces found");
    }

    auto it = std::find_if(interfaces.begin(), interfaces.end(),
        [](const WifiInterface& iface) {
            return iface.isUp();
        });

    if (it != interfaces.end()) {
        return Result<WifiInterface, std::string>::ok(*it);
    }

    return Result<WifiInterface, std::string>::ok(interfaces[0]);
}

}