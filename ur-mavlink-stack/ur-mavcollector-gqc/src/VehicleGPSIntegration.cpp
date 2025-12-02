#include "VehicleGPSIntegration.h"
#include "Vehicle.h"
#include "VehicleGPSFactGroup.h"

VehicleGPSIntegration::VehicleGPSIntegration()
{
    // Initialize the redesigned GPS fact groups
    _gpsFactGroup = std::make_shared<VehicleGPSFactGroupRedesigned>();
    _gps2FactGroup = std::make_shared<VehicleGPS2FactGroupRedesigned>();
}

void VehicleGPSIntegration::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    if (!_isGpsMessage(message.msgid)) {
        return;
    }
    
    // Route GPS messages to appropriate fact groups
    switch (message.msgid) {
    case MAVLINK_MSG_ID_GPS_RAW_INT:
    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
    case MAVLINK_MSG_ID_HIGH_LATENCY:
    case MAVLINK_MSG_ID_HIGH_LATENCY2:
    case MAVLINK_MSG_ID_GPS_STATUS:
        _gpsFactGroup->handleMessage(vehicle, message);
        break;
        
    case MAVLINK_MSG_ID_GPS2_RAW:
        _gps2FactGroup->handleMessage(vehicle, message);
        break;
        
    default:
        break;
    }
}

void VehicleGPSIntegration::initializeFactGroups()
{
    // Initialize both GPS fact groups with default values
    _gpsFactGroup->handleMessage(nullptr, mavlink_message_t()); // Reset to defaults
    _gps2FactGroup->handleMessage(nullptr, mavlink_message_t()); // Reset to defaults
}

std::shared_ptr<VehicleGPSFactGroupRedesigned> VehicleGPSIntegration::migrateFromLegacy(
    std::shared_ptr<VehicleGPSFactGroup> legacyGroup)
{
    if (!legacyGroup) {
        return std::make_shared<VehicleGPSFactGroupRedesigned>();
    }
    
    auto redesignedGroup = std::make_shared<VehicleGPSFactGroupRedesigned>();
    
    // Migrate core GPS data
    if (legacyGroup->lat()) {
        redesignedGroup->lat()->setRawValue(legacyGroup->lat()->rawValue());
    }
    if (legacyGroup->lon()) {
        redesignedGroup->lon()->setRawValue(legacyGroup->lon()->rawValue());
    }
    if (legacyGroup->alt()) {
        redesignedGroup->alt()->setRawValue(legacyGroup->alt()->rawValue());
    }
    if (legacyGroup->hdop()) {
        redesignedGroup->hdop()->setRawValue(legacyGroup->hdop()->rawValue());
    }
    if (legacyGroup->vdop()) {
        redesignedGroup->vdop()->setRawValue(legacyGroup->vdop()->rawValue());
    }
    if (legacyGroup->course()) {
        redesignedGroup->courseOverGround()->setRawValue(legacyGroup->course()->rawValue());
    }
    if (legacyGroup->groundSpeed()) {
        redesignedGroup->groundSpeed()->setRawValue(legacyGroup->groundSpeed()->rawValue());
    }
    if (legacyGroup->satellitesVisible()) {
        redesignedGroup->count()->setRawValue(legacyGroup->satellitesVisible()->rawValue());
    }
    if (legacyGroup->fixType()) {
        redesignedGroup->lock()->setRawValue(legacyGroup->fixType()->rawValue());
        redesignedGroup->fixType()->setRawValue(legacyGroup->fixType()->rawValue());
    }
    
    // Generate MGRS coordinate if lat/lon are valid
    auto latFact = legacyGroup->lat();
    auto lonFact = legacyGroup->lon();
    if (latFact && lonFact) {
        double lat = std::get<double>(latFact->rawValue());
        double lon = std::get<double>(lonFact->rawValue());
        if (!std::isnan(lat) && !std::isnan(lon)) {
            redesignedGroup->mgrs()->setRawValue(redesignedGroup->_convertGeoToMGRS(lat, lon));
        }
    }
    
    return redesignedGroup;
}

void VehicleGPSIntegration::_routeMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    // Internal message routing logic
    handleMessage(vehicle, message);
}

bool VehicleGPSIntegration::_isGpsMessage(uint32_t messageId)
{
    switch (messageId) {
    case MAVLINK_MSG_ID_GPS_RAW_INT:
    case MAVLINK_MSG_ID_GPS2_RAW:
    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
    case MAVLINK_MSG_ID_HIGH_LATENCY:
    case MAVLINK_MSG_ID_HIGH_LATENCY2:
    case MAVLINK_MSG_ID_GPS_STATUS:
        return true;
    default:
        return false;
    }
}
