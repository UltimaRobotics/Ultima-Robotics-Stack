#include "VehicleRedesigned.h"
#include <limits>

VehicleRedesigned::VehicleRedesigned(MAVLinkInterface* mavlinkInterface)
    : Vehicle(mavlinkInterface)
    , _gpsIntegration(std::make_unique<VehicleGPSIntegration>())
{
    _initializeRedesignedGpsSystem();
}

void VehicleRedesigned::handleMessage(const mavlink_message_t &message)
{
    // First handle with base Vehicle class for non-GPS messages
    Vehicle::handleMessage(message);
    
    // Then route GPS messages to redesigned system
    _gpsIntegration->handleMessage(this, message);
}

// Convenience methods for accessing GPS data (QGC-style)
double VehicleRedesigned::latitude() const
{
    auto latFact = _gpsIntegration->gpsFactGroup()->lat();
    return latFact ? std::get<double>(latFact->rawValue()) : std::numeric_limits<double>::quiet_NaN();
}

double VehicleRedesigned::longitude() const
{
    auto lonFact = _gpsIntegration->gpsFactGroup()->lon();
    return lonFact ? std::get<double>(lonFact->rawValue()) : std::numeric_limits<double>::quiet_NaN();
}

double VehicleRedesigned::altitude() const
{
    auto altFact = _gpsIntegration->gpsFactGroup()->alt();
    return altFact ? std::get<double>(altFact->rawValue()) : std::numeric_limits<double>::quiet_NaN();
}

double VehicleRedesigned::groundSpeed() const
{
    auto speedFact = _gpsIntegration->gpsFactGroup()->groundSpeed();
    return speedFact ? std::get<double>(speedFact->rawValue()) : std::numeric_limits<double>::quiet_NaN();
}

double VehicleRedesigned::heading() const
{
    auto headingFact = _gpsIntegration->gpsFactGroup()->heading();
    return headingFact ? std::get<double>(headingFact->rawValue()) : std::numeric_limits<double>::quiet_NaN();
}

uint8_t VehicleRedesigned::satelliteCount() const
{
    auto countFact = _gpsIntegration->gpsFactGroup()->count();
    return countFact ? std::get<int32_t>(countFact->rawValue()) : 0;
}

uint8_t VehicleRedesigned::fixType() const
{
    auto fixFact = _gpsIntegration->gpsFactGroup()->fixType();
    return fixFact ? std::get<uint8_t>(fixFact->rawValue()) : 0;
}

std::string VehicleRedesigned::mgrsCoordinate() const
{
    auto mgrsFact = _gpsIntegration->gpsFactGroup()->mgrs();
    return mgrsFact ? std::get<std::string>(mgrsFact->rawValue()) : "";
}

// Enhanced GPS status methods
bool VehicleRedesigned::hasGpsFix() const
{
    uint8_t fix = fixType();
    return fix >= 1 && fix <= 8; // Any valid fix type
}

bool VehicleRedesigned::has3DFix() const
{
    uint8_t fix = fixType();
    return fix >= 3 && fix <= 8; // 3D fix or better
}

bool VehicleRedesigned::hasRTKFix() const
{
    uint8_t fix = fixType();
    return fix == 5 || fix == 6; // RTK float or RTK fixed
}

double VehicleRedesigned::hdop() const
{
    auto hdopFact = _gpsIntegration->gpsFactGroup()->hdop();
    return hdopFact ? std::get<double>(hdopFact->rawValue()) : std::numeric_limits<double>::quiet_NaN();
}

double VehicleRedesigned::vdop() const
{
    auto vdopFact = _gpsIntegration->gpsFactGroup()->vdop();
    return vdopFact ? std::get<double>(vdopFact->rawValue()) : std::numeric_limits<double>::quiet_NaN();
}

bool VehicleRedesigned::_isValidGpsData() const
{
    double lat = latitude();
    double lon = longitude();
    return !std::isnan(lat) && !std::isnan(lon) && 
           lat >= -90.0 && lat <= 90.0 && 
           lon >= -180.0 && lon <= 180.0;
}

void VehicleRedesigned::_initializeRedesignedGpsSystem()
{
    _gpsIntegration->initializeFactGroups();
}
