#include "VehicleGPSFactGroup.h"
#include "Vehicle.h"

// Include specific MAVLink GPS headers (use v1 to match existing codebase)
#include "../thirdparty/c_library_v2/common/mavlink_msg_gps_raw_int.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_gps2_raw.h"
#include "../thirdparty/c_library_v2/standard/mavlink_msg_global_position_int.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_high_latency2.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_gps_status.h"

#include <iostream>     // For debug output logging (std::cout)
#include <memory>      // For std::shared_ptr used in fact creation
#include <string>      // For string operations in satellite fact names
#include <iomanip>     // For output formatting (std::setw, std::setprecision)
#include <algorithm>   // For potential future algorithms (std::max_element)
#include <cstring>     // For memcpy in packed structure handling

VehicleGPSFactGroup::VehicleGPSFactGroup(bool ignoreCamelCase)
    : FactGroup(1000, ignoreCamelCase) // Update every 1 second
{
    // Add all basic GPS facts
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
    
    // Add GPS status facts for detailed satellite information
    _addFact(std::make_shared<Fact>(0, "gpsStatusSatellitesVisible", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "gpsStatusSatellitesUsed", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "gpsStatusAvgSNR", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "gpsStatusMaxSNR", FactMetaData::valueTypeUint8));
    
    // Initialize GPS status facts to default values
    gpsStatusSatellitesVisible()->setRawValue(static_cast<uint8_t>(0));
    gpsStatusSatellitesUsed()->setRawValue(static_cast<uint8_t>(0));
    gpsStatusAvgSNR()->setRawValue(0.0f);
    gpsStatusMaxSNR()->setRawValue(static_cast<uint8_t>(0));
}

void VehicleGPSFactGroup::handleMessage([[maybe_unused]] Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_GPS_RAW_INT:
            std::cout << "[GPS] Handling GPS_RAW_INT message" << std::endl;
            _handleGPSRawInt(message);
            break;
            
        case MAVLINK_MSG_ID_GPS2_RAW:
            std::cout << "[GPS] Handling GPS2_RAW message" << std::endl;
            _handleGPS2Raw(message);
            break;
            
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
            std::cout << "[GPS] Handling GLOBAL_POSITION_INT message" << std::endl;
            _handleGlobalPositionInt(message);
            break;
            
        case MAVLINK_MSG_ID_HIGH_LATENCY2:
            std::cout << "[GPS] Handling HIGH_LATENCY2 message" << std::endl;
            _handleHighLatency2(message);
            break;
            
        case MAVLINK_MSG_ID_GPS_STATUS:
            std::cout << "[GPS] Handling GPS_STATUS message - detailed satellite data" << std::endl;
            _handleGPSStatus(message);
            break;
            
        default:
            break;
    }
}

void VehicleGPSFactGroup::_handleGPSRawInt(const mavlink_message_t &message)
{
    // Use MAVLink helper functions to extract GPS data safely
    int32_t latitude = mavlink_msg_gps_raw_int_get_lat(&message);
    int32_t longitude = mavlink_msg_gps_raw_int_get_lon(&message);
    int32_t altitude = mavlink_msg_gps_raw_int_get_alt(&message);
    uint16_t eph = mavlink_msg_gps_raw_int_get_eph(&message);
    uint16_t epv = mavlink_msg_gps_raw_int_get_epv(&message);
    uint16_t cog = mavlink_msg_gps_raw_int_get_cog(&message);
    uint16_t vel = mavlink_msg_gps_raw_int_get_vel(&message);
    uint8_t satellites_visible = mavlink_msg_gps_raw_int_get_satellites_visible(&message);
    uint8_t fix_type = mavlink_msg_gps_raw_int_get_fix_type(&message);
    
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

void VehicleGPSFactGroup::_handleGPS2Raw(const mavlink_message_t &message)
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

void VehicleGPSFactGroup::_handleGlobalPositionInt(const mavlink_message_t &message)
{
    // Use MAVLink helper functions to extract global position data safely
    int32_t latitude = mavlink_msg_global_position_int_get_lat(&message);
    int32_t longitude = mavlink_msg_global_position_int_get_lon(&message);
    int32_t altitude = mavlink_msg_global_position_int_get_alt(&message);
    int32_t relative_alt = mavlink_msg_global_position_int_get_relative_alt(&message);
    uint16_t hdg = mavlink_msg_global_position_int_get_hdg(&message);
    
    lat()->setRawValue(latitude);
    lon()->setRawValue(longitude);
    alt()->setRawValue(altitude);
    altEllipsoid()->setRawValue(relative_alt);
    
    // Only update heading if valid (UINT16_MAX = 65535 indicates invalid)
    if (hdg != UINT16_MAX) {
        heading()->setRawValue(static_cast<float>(hdg / 100.0f)); // Convert to degrees
    }
    
    _setTelemetryAvailable(true);
}

void VehicleGPSFactGroup::_handleHighLatency2(const mavlink_message_t &message)
{
    // Use MAVLink helper functions to extract high latency GPS data safely
    float latitude = mavlink_msg_high_latency2_get_latitude(&message);
    float longitude = mavlink_msg_high_latency2_get_longitude(&message);
    float altitude = mavlink_msg_high_latency2_get_altitude(&message);
    float groundspeed = mavlink_msg_high_latency2_get_groundspeed(&message);
    float heading_val = mavlink_msg_high_latency2_get_heading(&message);
    
    // Validate latitude/longitude range (-90 to 90 for lat, -180 to 180 for lon)
    if (latitude >= -90.0f && latitude <= 90.0f) {
        lat()->setRawValue(static_cast<int32_t>(latitude * 1e7));
    }
    if (longitude >= -180.0f && longitude <= 180.0f) {
        lon()->setRawValue(static_cast<int32_t>(longitude * 1e7));
    }
    
    // Validate altitude (reasonable range: -1000m to 50000m)
    if (altitude >= -1000.0f && altitude <= 50000.0f) {
        alt()->setRawValue(static_cast<int32_t>(altitude * 1000.0f)); // Convert to mm
    }
    
    // Validate ground speed (0-100 m/s is reasonable range)
    if (groundspeed >= 0.0f && groundspeed <= 100.0f) {
        groundSpeed()->setRawValue(groundspeed);
    }
    
    // Validate heading (0-360 degrees)
    if (heading_val >= 0.0f && heading_val <= 360.0f) {
        heading()->setRawValue(heading_val);
    }
    
    _setTelemetryAvailable(true);
}

// GPS status handler for detailed satellite information
void VehicleGPSFactGroup::_handleGPSStatus(const mavlink_message_t &message)
{
    // Use MAVLink helper functions to extract GPS status data safely
    uint8_t satellites_visible = mavlink_msg_gps_status_get_satellites_visible(&message);
    
    std::cout << "[GPS] === GPS_STATUS Message Decoded ===" << std::endl;
    std::cout << "[GPS] Satellites Visible: " << static_cast<int>(satellites_visible) << std::endl;
    
    // Validate satellite count to prevent array overflow
    uint8_t satelliteCount = satellites_visible;
    if (satelliteCount > 20) {
        std::cout << "[GPS] WARNING: Satellite count " << static_cast<int>(satelliteCount) 
                  << " exceeds maximum 20, clamping to 20" << std::endl;
        satelliteCount = 20;
    }
    
    // Extract satellite data using MAVLink helper functions
    uint8_t satellite_prn[20];
    uint8_t satellite_used[20];
    uint8_t satellite_elevation[20];
    uint8_t satellite_azimuth[20];
    uint8_t satellite_snr[20];
    
    mavlink_msg_gps_status_get_satellite_prn(&message, satellite_prn);
    mavlink_msg_gps_status_get_satellite_used(&message, satellite_used);
    mavlink_msg_gps_status_get_satellite_elevation(&message, satellite_elevation);
    mavlink_msg_gps_status_get_satellite_azimuth(&message, satellite_azimuth);
    mavlink_msg_gps_status_get_satellite_snr(&message, satellite_snr);
    
    // Count used satellites and calculate SNR statistics
    uint8_t usedCount = 0;
    uint16_t totalSNR = 0;
    uint8_t maxSNR = 0;
    
    for (uint8_t i = 0; i < satelliteCount; i++) {
        if (satellite_used[i] > 0) {
            usedCount++;
        }
        if (satellite_snr[i] > 0) {
            totalSNR += satellite_snr[i];
            if (satellite_snr[i] > maxSNR) {
                maxSNR = satellite_snr[i];
            }
        }
        
        std::cout << "[GPS] Sat " << std::setw(2) << (i+1) 
                  << " PRN: " << std::setw(3) << static_cast<int>(satellite_prn[i])
                  << " Used: " << (satellite_used[i] ? "Yes" : "No ")
                  << " Elev: " << std::setw(3) << static_cast<int>(satellite_elevation[i]) << "°"
                  << " Azim: " << std::setw(3) << static_cast<int>(satellite_azimuth[i]) << "°"
                  << " SNR: " << std::setw(3) << static_cast<int>(satellite_snr[i]) << "dB"
                  << std::endl;
    }
    
    // Calculate average SNR
    float avgSNR = 0.0f;
    if (satelliteCount > 0) {
        avgSNR = static_cast<float>(totalSNR) / static_cast<float>(satelliteCount);
    }
    
    std::cout << "[GPS] Used Satellites: " << static_cast<int>(usedCount) << std::endl;
    std::cout << "[GPS] Average SNR: " << std::fixed << std::setprecision(1) << avgSNR << "dB" << std::endl;
    std::cout << "[GPS] Max SNR: " << static_cast<int>(maxSNR) << "dB" << std::endl;
    std::cout << "[GPS] =====================================" << std::endl;
    
    // Create GPS status struct for fact updates
    mavlink_gps_status_t gpsStatus;
    gpsStatus.satellites_visible = satelliteCount;
    memcpy(gpsStatus.satellite_prn, satellite_prn, sizeof(satellite_prn));
    memcpy(gpsStatus.satellite_used, satellite_used, sizeof(satellite_used));
    memcpy(gpsStatus.satellite_elevation, satellite_elevation, sizeof(satellite_elevation));
    memcpy(gpsStatus.satellite_azimuth, satellite_azimuth, sizeof(satellite_azimuth));
    memcpy(gpsStatus.satellite_snr, satellite_snr, sizeof(satellite_snr));
    
    // Update GPS status facts
    _updateGPSStatusFacts(gpsStatus);
    
    // Update individual satellite facts
    _updateSatelliteFacts(gpsStatus);
    
    _setTelemetryAvailable(true);
}

// Update GPS status facts with satellite statistics
void VehicleGPSFactGroup::_updateGPSStatusFacts(const __mavlink_gps_status_t& gpsStatus)
{
    // Create a typed copy for easier access
    mavlink_gps_status_t gpsStatusTyped;
    std::memcpy(&gpsStatusTyped, &gpsStatus, sizeof(gpsStatusTyped));
    
    // Update basic GPS status facts with validated satellite count
    uint8_t satelliteCount = gpsStatusTyped.satellites_visible;
    if (satelliteCount > 20) {
        satelliteCount = 20; // Clamp to maximum to prevent issues
    }
    gpsStatusSatellitesVisible()->setRawValue(satelliteCount);
    
    // Count used satellites
    uint8_t usedCount = 0;
    for (uint8_t i = 0; i < satelliteCount; i++) {
        if (gpsStatusTyped.satellite_used[i] > 0) {
            usedCount++;
        }
    }
    gpsStatusSatellitesUsed()->setRawValue(usedCount);
    
    // Calculate SNR statistics
    uint16_t totalSNR = 0;
    uint8_t maxSNR = 0;
    uint8_t validSNRCount = 0;
    
    for (uint8_t i = 0; i < satelliteCount; i++) {
        if (gpsStatusTyped.satellite_snr[i] > 0) {
            totalSNR += gpsStatusTyped.satellite_snr[i];
            validSNRCount++;
            if (gpsStatusTyped.satellite_snr[i] > maxSNR) {
                maxSNR = gpsStatusTyped.satellite_snr[i];
            }
        }
    }
    
    // Calculate average SNR
    float avgSNR = 0.0f;
    if (validSNRCount > 0) {
        avgSNR = static_cast<float>(totalSNR) / static_cast<float>(validSNRCount);
    }
    
    gpsStatusAvgSNR()->setRawValue(avgSNR);
    gpsStatusMaxSNR()->setRawValue(maxSNR);
}

// Add satellite facts for the specified number of satellites
void VehicleGPSFactGroup::_addSatelliteFacts(uint8_t satelliteCount)
{
    for (uint8_t i = 0; i < satelliteCount && i < 20; i++) {
        std::string prnFactName = "satellite" + std::to_string(i) + "PRN";
        std::string usedFactName = "satellite" + std::to_string(i) + "Used";
        std::string elevFactName = "satellite" + std::to_string(i) + "Elevation";
        std::string azimFactName = "satellite" + std::to_string(i) + "Azimuth";
        std::string snrFactName = "satellite" + std::to_string(i) + "SNR";
        
        // Only add if they don't already exist
        if (!getFact(prnFactName)) {
            _addFact(std::make_shared<Fact>(0, prnFactName, FactMetaData::valueTypeUint8));
        }
        if (!getFact(usedFactName)) {
            _addFact(std::make_shared<Fact>(0, usedFactName, FactMetaData::valueTypeUint8));
        }
        if (!getFact(elevFactName)) {
            _addFact(std::make_shared<Fact>(0, elevFactName, FactMetaData::valueTypeUint8));
        }
        if (!getFact(azimFactName)) {
            _addFact(std::make_shared<Fact>(0, azimFactName, FactMetaData::valueTypeUint8));
        }
        if (!getFact(snrFactName)) {
            _addFact(std::make_shared<Fact>(0, snrFactName, FactMetaData::valueTypeUint8));
        }
    }
    
    // Update max satellite count
    if (satelliteCount > _maxSatellites) {
        _maxSatellites = satelliteCount;
    }
}

// Update individual satellite facts
void VehicleGPSFactGroup::_updateSatelliteFacts(const __mavlink_gps_status_t& gpsStatus)
{
    // Create a typed copy for easier access
    mavlink_gps_status_t gpsStatusTyped;
    std::memcpy(&gpsStatusTyped, &gpsStatus, sizeof(gpsStatusTyped));
    
    // Validate and clamp satellite count
    uint8_t satelliteCount = gpsStatusTyped.satellites_visible;
    if (satelliteCount > 20) {
        satelliteCount = 20; // Clamp to maximum to prevent array overflow
    }
    
    // Add facts for all visible satellites
    _addSatelliteFacts(satelliteCount);
    
    // Update individual satellite data
    for (uint8_t i = 0; i < satelliteCount; i++) {
        std::string prnFactName = "satellite" + std::to_string(i) + "PRN";
        std::string usedFactName = "satellite" + std::to_string(i) + "Used";
        std::string elevFactName = "satellite" + std::to_string(i) + "Elevation";
        std::string azimFactName = "satellite" + std::to_string(i) + "Azimuth";
        std::string snrFactName = "satellite" + std::to_string(i) + "SNR";
        
        auto prnFact = getFact(prnFactName);
        auto usedFact = getFact(usedFactName);
        auto elevFact = getFact(elevFactName);
        auto azimFact = getFact(azimFactName);
        auto snrFact = getFact(snrFactName);
        
        if (prnFact) prnFact->setRawValue(gpsStatusTyped.satellite_prn[i]);
        if (usedFact) usedFact->setRawValue(gpsStatusTyped.satellite_used[i]);
        if (elevFact) elevFact->setRawValue(gpsStatusTyped.satellite_elevation[i]);
        if (azimFact) azimFact->setRawValue(gpsStatusTyped.satellite_azimuth[i]);
        if (snrFact) snrFact->setRawValue(gpsStatusTyped.satellite_snr[i]);
    }
}

// Public methods to access individual satellite data
std::shared_ptr<Fact> VehicleGPSFactGroup::satellitePRN(uint8_t index)
{
    if (index >= 20) return nullptr;
    std::string factName = "satellite" + std::to_string(index) + "PRN";
    return getFact(factName);
}

std::shared_ptr<Fact> VehicleGPSFactGroup::satelliteUsed(uint8_t index)
{
    if (index >= 20) return nullptr;
    std::string factName = "satellite" + std::to_string(index) + "Used";
    return getFact(factName);
}

std::shared_ptr<Fact> VehicleGPSFactGroup::satelliteElevation(uint8_t index)
{
    if (index >= 20) return nullptr;
    std::string factName = "satellite" + std::to_string(index) + "Elevation";
    return getFact(factName);
}

std::shared_ptr<Fact> VehicleGPSFactGroup::satelliteAzimuth(uint8_t index)
{
    if (index >= 20) return nullptr;
    std::string factName = "satellite" + std::to_string(index) + "Azimuth";
    return getFact(factName);
}

std::shared_ptr<Fact> VehicleGPSFactGroup::satelliteSNR(uint8_t index)
{
    if (index >= 20) return nullptr;
    std::string factName = "satellite" + std::to_string(index) + "SNR";
    return getFact(factName);
}
