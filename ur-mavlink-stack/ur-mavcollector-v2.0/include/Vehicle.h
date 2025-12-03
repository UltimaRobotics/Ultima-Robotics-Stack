#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>
#include <cstdint>
#include "MavlinkUdpConnection.h"

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
    
    std::string getBoardIdentification() const;
    std::string getBoardClass() const;
    std::string getBoardName() const;
    
    bool hasAutopilotVersion() const;

private:
    MavlinkAutopilotVersionInfo _autopilotVersion;
    bool _hasAutopilotVersion;
    MavlinkGPSData _gpsData;
    bool _hasGPSData;
    MavlinkSystemStatus _systemStatus;
    bool _hasSystemStatus;
    
    std::string parseVersionString(uint32_t version) const;
    std::string parseCustomVersionString(const uint8_t version[8]) const;
};

#endif // VEHICLE_H
