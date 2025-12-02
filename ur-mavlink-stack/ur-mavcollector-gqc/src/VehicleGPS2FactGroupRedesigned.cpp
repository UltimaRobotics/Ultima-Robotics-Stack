#include "VehicleGPS2FactGroupRedesigned.h"
#include "Vehicle.h"

VehicleGPS2FactGroupRedesigned::VehicleGPS2FactGroupRedesigned(bool ignoreCamelCase)
    : VehicleGPSFactGroupRedesigned(ignoreCamelCase)
{
    // GPS2 inherits all GPS facts and behaviors
    // No additional initialization needed
}

void VehicleGPS2FactGroupRedesigned::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
    case MAVLINK_MSG_ID_GPS2_RAW:
        _handleGps2Raw(message);
        break;
    default:
        break;
    }
}

void VehicleGPS2FactGroupRedesigned::_handleGps2Raw(const mavlink_message_t &message)
{
    // Use MAVLink helper functions for safe data extraction
    mavlink_gps2_raw_t gps2Raw;
    mavlink_msg_gps2_raw_decode(&message, &gps2Raw);
    
    _updateGpsFactsFromGps2Raw(gps2Raw);
    _setTelemetryAvailable(true);
}
