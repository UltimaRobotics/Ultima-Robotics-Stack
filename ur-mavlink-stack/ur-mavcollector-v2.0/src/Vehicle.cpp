#include "Vehicle.h"
#include "BoardIdentifier.h"
#include <sstream>
#include <iomanip>
#include <bitset>
#include <cstring>

Vehicle::Vehicle() : _hasAutopilotVersion(false), _hasGPSData(false), _hasSystemStatus(false) {
    memset(&_autopilotVersion, 0, sizeof(_autopilotVersion));
}

void Vehicle::setAutopilotVersionInfo(const MavlinkAutopilotVersionInfo& info) {
    _autopilotVersion = info;
    _hasAutopilotVersion = true;
}

const MavlinkAutopilotVersionInfo& Vehicle::getAutopilotVersionInfo() const {
    return _autopilotVersion;
}

std::string Vehicle::getBoardIdentification() const {
    if (!_hasAutopilotVersion) {
        return "Unknown (no autopilot version received)";
    }
    
    return BoardIdentifier::instance().identifyBoard(_autopilotVersion.vendor_id, _autopilotVersion.product_id);
}

std::string Vehicle::getBoardClass() const {
    if (!_hasAutopilotVersion) {
        return "Unknown";
    }
    
    return BoardIdentifier::instance().getBoardClass(_autopilotVersion.vendor_id, _autopilotVersion.product_id);
}

std::string Vehicle::getBoardName() const {
    if (!_hasAutopilotVersion) {
        return "Unknown Board";
    }
    
    return BoardIdentifier::instance().getBoardName(_autopilotVersion.vendor_id, _autopilotVersion.product_id);
}

bool Vehicle::hasAutopilotVersion() const {
    return _hasAutopilotVersion;
}

void Vehicle::setGPSData(const MavlinkGPSData& data) {
    _gpsData = data;
    _hasGPSData = true;
}

const MavlinkGPSData& Vehicle::getGPSData() const {
    return _gpsData;
}

bool Vehicle::hasGPSData() const {
    return _hasGPSData;
}

void Vehicle::setSystemStatus(const MavlinkSystemStatus& data) {
    _systemStatus = data;
    _hasSystemStatus = true;
}

const MavlinkSystemStatus& Vehicle::getSystemStatus() const {
    return _systemStatus;
}

bool Vehicle::hasSystemStatus() const {
    return _hasSystemStatus;
}
