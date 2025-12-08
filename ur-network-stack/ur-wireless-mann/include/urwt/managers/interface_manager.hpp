#ifndef URWT_MANAGERS_INTERFACE_MANAGER_HPP
#define URWT_MANAGERS_INTERFACE_MANAGER_HPP

#include <vector>
#include <memory>
#include <string>
#include "../models/wifi_interface.hpp"
#include "../utils/result.hpp"

namespace urwt {

class InterfaceDetector;

class InterfaceManager {
public:
    explicit InterfaceManager(std::shared_ptr<InterfaceDetector> detector = nullptr);

    Result<std::vector<WifiInterface>, std::string> listInterfaces();
    Result<WifiInterface, std::string> getInterface(const std::string& name);
    Result<WifiInterface, std::string> selectBestInterface();

private:
    std::shared_ptr<InterfaceDetector> detector_;
};

}

#endif
