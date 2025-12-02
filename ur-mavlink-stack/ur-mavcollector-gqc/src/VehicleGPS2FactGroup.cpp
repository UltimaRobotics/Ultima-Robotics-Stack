#include "VehicleGPS2FactGroup.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>

VehicleGPS2FactGroup::VehicleGPS2FactGroup(bool ignoreCamelCase)
    : FactGroup(1000, ignoreCamelCase)
{
    _addFact(std::make_shared<Fact>(0, "lat", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "lon", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "alt", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "altEllipsoid", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "hdop", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "vdop", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "course", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "groundSpeed", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "count", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "lock", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "satellitesVisible", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "utcDate", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "utcTime", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "timeUtc", FactMetaData::valueTypeUint64));
    _addFact(std::make_shared<Fact>(0, "fixType", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "eph", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "epv", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "heading", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "speedAccuracy", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "horizAccuracy", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "vertAccuracy", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "yaw", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "yawAccuracy", FactMetaData::valueTypeFloat));
}

void VehicleGPS2FactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_GPS2_RAW:
            _handleGPS2Raw(message);
            break;
    }
}

void VehicleGPS2FactGroup::_handleGPS2Raw(const mavlink_message_t &message)
{
    // Use MAVLink helper functions to extract GPS2 data safely
    int32_t latitude = mavlink_msg_gps2_raw_get_lat(&message);
    int32_t longitude = mavlink_msg_gps2_raw_get_lon(&message);
    int32_t altitude = mavlink_msg_gps2_raw_get_alt(&message);
    uint16_t eph = mavlink_msg_gps2_raw_get_eph(&message);
    uint16_t epv = mavlink_msg_gps2_raw_get_epv(&message);
    uint16_t cog = mavlink_msg_gps2_raw_get_cog(&message);
    uint16_t vel = mavlink_msg_gps2_raw_get_vel(&message);
    uint8_t satellites_visible = mavlink_msg_gps2_raw_get_satellites_visible(&message);
    uint8_t fix_type = mavlink_msg_gps2_raw_get_fix_type(&message);
    
    lat()->setRawValue(latitude);
    lon()->setRawValue(longitude);
    alt()->setRawValue(altitude);
    
    // Only update accuracy data if valid (UINT16_MAX = 65535 indicates invalid)
    if (eph != UINT16_MAX) {
        hdop()->setRawValue(static_cast<uint16_t>(eph / 10)); // Convert to dm
    }
    if (epv != UINT16_MAX) {
        vdop()->setRawValue(static_cast<uint16_t>(epv / 10)); // Convert to dm
    }
    
    // Only update course if valid (UINT16_MAX = 65535 indicates invalid)
    if (cog != UINT16_MAX) {
        course()->setRawValue(static_cast<float>(cog / 100.0f)); // Convert to degrees
    }
    
    // Only update ground speed if valid (UINT16_MAX = 65535 indicates invalid)
    if (vel != UINT16_MAX) {
        groundSpeed()->setRawValue(static_cast<float>(vel / 100.0f)); // Convert to m/s
    }
    
    satellitesVisible()->setRawValue(satellites_visible);
    fixType()->setRawValue(fix_type);
    
    _setTelemetryAvailable(true);
}
