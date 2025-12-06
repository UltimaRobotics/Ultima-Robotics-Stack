#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>
#include <cstdint>
#include "MavlinkUdpConnection.h"

struct DeviceInfo {
    std::string devicePath;
    int baudrate;
    int systemId;
    int componentId;
    int mavlinkVersion;
    std::string state;
    std::string timestamp;
    std::string autopilotType;
    std::string boardClass;
    std::string boardName;
    std::string deviceName;
    std::string manufacturer;
    std::string serialNumber;
    std::string vendorId;
    std::string productId;
    bool isValid;
    
    DeviceInfo() : baudrate(0), systemId(0), componentId(0), mavlinkVersion(0), isValid(false) {}
};

class Vehicle {
public:
    Vehicle();
    ~Vehicle() = default;
    
    void setAutopilotVersionInfo(const MavlinkAutopilotVersionInfo& info);
    const MavlinkAutopilotVersionInfo& getAutopilotVersionInfo() const;
    
    void setGPSData(const MavlinkGPSData& data);
    const MavlinkGPSData& getGPSData() const;
    bool hasGPSData() const;
    
    void setSystemStatus(const MavlinkSystemStatus& data);
    const MavlinkSystemStatus& getSystemStatus() const;
    bool hasSystemStatus() const;
    
    void setDeviceInfo(const DeviceInfo& info);
    const DeviceInfo& getDeviceInfo() const;
    bool hasDeviceInfo() const;
    
    std::string getBoardIdentification() const;
    std::string getBoardClass() const;
    std::string getBoardName() const;
    
    bool hasAutopilotVersion() const;
    
    // New method to get complete vehicle info as JSON
    std::string getVehicleInfoJson() const;

private:
    MavlinkAutopilotVersionInfo _autopilotVersion;
    bool _hasAutopilotVersion;
    MavlinkGPSData _gpsData;
    bool _hasGPSData;
    MavlinkSystemStatus _systemStatus;
    bool _hasSystemStatus;
    DeviceInfo _deviceInfo;
    bool _hasDeviceInfo;
    
    std::string parseVersionString(uint32_t version) const;
    std::string parseCustomVersionString(const uint8_t version[8]) const;
};

#endif // VEHICLE_H
