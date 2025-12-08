#ifndef URWT_MANAGERS_INTERFACE_DETECTOR_HPP
#define URWT_MANAGERS_INTERFACE_DETECTOR_HPP

#include <vector>
#include <memory>
#include "../models/wifi_interface.hpp"
#include "../utils/result.hpp"
#include "../utils/process_executor.hpp"

namespace urwt {

class InterfaceDetector {
public:
    explicit InterfaceDetector(std::shared_ptr<ProcessExecutor> executor = nullptr);

    Result<std::vector<WifiInterface>, std::string> detectInterfaces();
    Result<WifiInterface, std::string> getInterfaceInfo(const std::string& name);

private:
    std::shared_ptr<ProcessExecutor> executor_;
    
    Result<std::vector<std::string>, std::string> listWirelessInterfaces();
    WifiInterface parseInterfaceInfo(const std::string& name, const std::string& iwOutput);
    MacAddress parseMacAddress(const std::string& output);
    std::optional<std::string> parseSSID(const std::string& output);
    std::optional<int> parseFrequency(const std::string& output);
    std::optional<int> parseSignalStrength(const std::string& output);
    InterfaceStatus detectStatus(const std::string& name);
};

}

#endif
