
#pragma once
#include "DeviceInfo.hpp"
#include <string>

class HTTPClient {
public:
    HTTPClient(const std::string& endpoint, int timeoutMs);
    
    bool postDeviceInfo(const DeviceInfo& info);
    
private:
    std::string endpoint_;
    int timeoutMs_;
    
    std::string deviceInfoToJSON(const DeviceInfo& info);
};
