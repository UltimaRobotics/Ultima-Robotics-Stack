#include "VehicleGPSFactGroupRedesigned.h"
#include "Vehicle.h"

// Include specific MAVLink GPS headers (use v2 to match existing codebase)
#include "../thirdparty/c_library_v2/common/mavlink_msg_gps_raw_int.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_gps2_raw.h"
#include "../thirdparty/c_library_v2/standard/mavlink_msg_global_position_int.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_high_latency.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_high_latency2.h"
#include "../thirdparty/c_library_v2/common/mavlink_msg_gps_status.h"

#include <iostream>
#include <memory>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <cmath>

VehicleGPSFactGroupRedesigned::VehicleGPSFactGroupRedesigned(bool ignoreCamelCase)
    : FactGroup(1000, ignoreCamelCase) // Update every 1 second
{
    // Add all basic GPS facts (following QGC naming conventions)
    _addFact(std::make_shared<Fact>(0, "lat", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "lon", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "mgrs", FactMetaData::valueTypeString));
    _addFact(std::make_shared<Fact>(0, "hdop", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "vdop", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "courseOverGround", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "yaw", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "count", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "lock", FactMetaData::valueTypeInt32));
    
    // Additional GPS facts for comprehensive data
    _addFact(std::make_shared<Fact>(0, "alt", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "altEllipsoid", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "groundSpeed", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "speedAccuracy", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "horizAccuracy", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "vertAccuracy", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "heading", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "utcDate", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "utcTime", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "timeUtc", FactMetaData::valueTypeInt64));
    _addFact(std::make_shared<Fact>(0, "fixType", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "eph", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "epv", FactMetaData::valueTypeDouble));
    
    // GPS status and satellite data (enhanced beyond QGC)
    _addFact(std::make_shared<Fact>(0, "gpsStatusSatellitesVisible", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "gpsStatusSatellitesUsed", FactMetaData::valueTypeInt32));
    _addFact(std::make_shared<Fact>(0, "gpsStatusAvgSNR", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "gpsStatusMaxSNR", FactMetaData::valueTypeInt32));
}

void VehicleGPSFactGroupRedesigned::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
    case MAVLINK_MSG_ID_GPS_RAW_INT:
        _handleGpsRawInt(message);
        break;
    case MAVLINK_MSG_ID_GPS2_RAW:
        _handleGps2Raw(message);
        break;
    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
        _handleGlobalPositionInt(message);
        break;
    case MAVLINK_MSG_ID_HIGH_LATENCY:
        _handleHighLatency(message);
        break;
    case MAVLINK_MSG_ID_HIGH_LATENCY2:
        _handleHighLatency2(message);
        break;
    case MAVLINK_MSG_ID_GPS_STATUS:
        _handleGpsStatus(message);
        break;
    default:
        break;
    }
}

void VehicleGPSFactGroupRedesigned::_handleGpsRawInt(const mavlink_message_t &message)
{
    // Use MAVLink helper functions for safe data extraction
    mavlink_gps_raw_int_t gpsRaw;
    mavlink_msg_gps_raw_int_decode(&message, &gpsRaw);
    
    _updateGpsFactsFromRawInt(gpsRaw);
    _setTelemetryAvailable(true);
}

void VehicleGPSFactGroupRedesigned::_handleGps2Raw(const mavlink_message_t &message)
{
    // Use MAVLink helper functions for safe data extraction
    mavlink_gps2_raw_t gps2Raw;
    mavlink_msg_gps2_raw_decode(&message, &gps2Raw);
    
    _updateGpsFactsFromGps2Raw(gps2Raw);
    _setTelemetryAvailable(true);
}

void VehicleGPSFactGroupRedesigned::_handleGlobalPositionInt(const mavlink_message_t &message)
{
    // Use MAVLink helper functions for safe data extraction
    mavlink_global_position_int_t globalPos;
    mavlink_msg_global_position_int_decode(&message, &globalPos);
    
    _updateGpsFactsFromGlobalPositionInt(globalPos);
    _setTelemetryAvailable(true);
}

void VehicleGPSFactGroupRedesigned::_handleHighLatency(const mavlink_message_t &message)
{
    // Use MAVLink helper functions for safe data extraction
    mavlink_high_latency_t highLatency;
    mavlink_msg_high_latency_decode(&message, &highLatency);
    
    _updateGpsFactsFromHighLatency(highLatency);
    _setTelemetryAvailable(true);
}

void VehicleGPSFactGroupRedesigned::_handleHighLatency2(const mavlink_message_t &message)
{
    // Use MAVLink helper functions for safe data extraction
    mavlink_high_latency2_t highLatency2;
    mavlink_msg_high_latency2_decode(&message, &highLatency2);
    
    _updateGpsFactsFromHighLatency2(highLatency2);
    _setTelemetryAvailable(true);
}

void VehicleGPSFactGroupRedesigned::_handleGpsStatus(const mavlink_message_t &message)
{
    // Use MAVLink helper functions for safe data extraction
    mavlink_gps_status_t gpsStatus;
    mavlink_msg_gps_status_decode(&message, &gpsStatus);
    
    _updateGpsStatusFacts(gpsStatus);
    _setTelemetryAvailable(true);
}

void VehicleGPSFactGroupRedesigned::_updateGpsFactsFromRawInt(const mavlink_gps_raw_int_t& gpsRaw)
{
    // Convert from raw MAVLink values to proper units
    double latitude = static_cast<double>(gpsRaw.lat) / 1e7;
    double longitude = static_cast<double>(gpsRaw.lon) / 1e7;
    double altitude = static_cast<double>(gpsRaw.alt) / 1e3; // mm to meters
    double hdop = static_cast<double>(gpsRaw.eph) / 100.0; // cm to meters
    double vdop = static_cast<double>(gpsRaw.epv) / 100.0; // cm to meters
    double groundSpeed = static_cast<double>(gpsRaw.vel) / 100.0; // cm/s to m/s
    double courseOverGround = static_cast<double>(gpsRaw.cog) / 100.0; // deg*100 to degrees
    
    // Update basic GPS facts with validation
    if (_isValidCoordinate(latitude, longitude)) {
        lat()->setRawValue(latitude);
        lon()->setRawValue(longitude);
        
        // Generate MGRS coordinate if lat/lon are valid
        mgrs()->setRawValue(_convertGeoToMGRS(latitude, longitude));
    }
    
    if (!std::isnan(altitude)) {
        alt()->setRawValue(altitude);
    }
    
    if (_getValidAccuracy(hdop)) {
        this->hdop()->setRawValue(hdop);
        eph()->setRawValue(hdop); // EPH is essentially HDOP
    }
    
    if (_getValidAccuracy(vdop)) {
        this->vdop()->setRawValue(vdop);
        epv()->setRawValue(vdop); // EPV is essentially VDOP
    }
    
    if (_getValidSpeed(static_cast<uint16_t>(groundSpeed * 100))) {
        this->groundSpeed()->setRawValue(groundSpeed);
    }
    
    if (_getValidAngle(static_cast<uint16_t>(courseOverGround * 100))) {
        this->courseOverGround()->setRawValue(courseOverGround);
        heading()->setRawValue(courseOverGround); // Use course as heading
    }
    
    if (_isValidSatelliteCount(gpsRaw.satellites_visible)) {
        count()->setRawValue(static_cast<int32_t>(gpsRaw.satellites_visible));
        gpsStatusSatellitesVisible()->setRawValue(static_cast<int32_t>(gpsRaw.satellites_visible));
    }
    
    if (_isValidFixType(gpsRaw.fix_type)) {
        lock()->setRawValue(static_cast<int32_t>(gpsRaw.fix_type));
        fixType()->setRawValue(static_cast<int32_t>(gpsRaw.fix_type));
    }
    
    // Time information
    if (gpsRaw.time_usec != 0) {
        timeUtc()->setRawValue(static_cast<int64_t>(gpsRaw.time_usec));
    }
}

void VehicleGPSFactGroupRedesigned::_updateGpsFactsFromGps2Raw(const mavlink_gps2_raw_t& gps2Raw)
{
    // Similar to GPS_RAW_INT but for secondary GPS
    double latitude = static_cast<double>(gps2Raw.lat) / 1e7;
    double longitude = static_cast<double>(gps2Raw.lon) / 1e7;
    double altitude = static_cast<double>(gps2Raw.alt) / 1e3;
    double hdop = static_cast<double>(gps2Raw.eph) / 100.0; // cm to meters
    double vdop = static_cast<double>(gps2Raw.epv) / 100.0; // cm to meters
    double groundSpeed = static_cast<double>(gps2Raw.vel) / 100.0; // cm/s to m/s
    double courseOverGround = static_cast<double>(gps2Raw.cog) / 100.0; // deg*100 to degrees
    
    // Update GPS facts (GPS2 can override or supplement primary GPS)
    if (_isValidCoordinate(latitude, longitude)) {
        lat()->setRawValue(latitude);
        lon()->setRawValue(longitude);
        mgrs()->setRawValue(_convertGeoToMGRS(latitude, longitude));
    }
    
    if (!std::isnan(altitude)) {
        alt()->setRawValue(altitude);
    }
    
    if (_getValidAccuracy(hdop)) {
        this->hdop()->setRawValue(hdop);
    }
    
    if (_getValidAccuracy(vdop)) {
        this->vdop()->setRawValue(vdop);
    }
    
    if (_getValidSpeed(static_cast<uint16_t>(groundSpeed * 100))) {
        this->groundSpeed()->setRawValue(groundSpeed);
    }
    
    if (_getValidAngle(static_cast<uint16_t>(courseOverGround * 100))) {
        this->courseOverGround()->setRawValue(courseOverGround);
    }
    
    if (_isValidSatelliteCount(gps2Raw.satellites_visible)) {
        count()->setRawValue(static_cast<int32_t>(gps2Raw.satellites_visible));
    }
}

void VehicleGPSFactGroupRedesigned::_updateGpsFactsFromGlobalPositionInt(const mavlink_global_position_int_t& globalPos)
{
    // Global position provides relative altitude and velocity information
    double latitude = static_cast<double>(globalPos.lat) / 1e7;
    double longitude = static_cast<double>(globalPos.lon) / 1e7;
    double altitude = static_cast<double>(globalPos.alt) / 1e3; // mm to meters
    double relativeAltitude = static_cast<double>(globalPos.relative_alt) / 1e3;
    double groundSpeed = static_cast<double>(globalPos.vx) / 100.0; // cm/s to m/s
    double velocityY = static_cast<double>(globalPos.vy) / 100.0;
    double velocityZ = static_cast<double>(globalPos.vz) / 100.0;
    
    // Update position information
    if (_isValidCoordinate(latitude, longitude)) {
        lat()->setRawValue(latitude);
        lon()->setRawValue(longitude);
        mgrs()->setRawValue(_convertGeoToMGRS(latitude, longitude));
    }
    
    if (!std::isnan(altitude)) {
        alt()->setRawValue(altitude);
    }
    
    // Calculate ground speed from velocity components
    double calculatedSpeed = std::sqrt(groundSpeed * groundSpeed + velocityY * velocityY);
    if (_getValidSpeed(static_cast<uint16_t>(calculatedSpeed * 100))) {
        this->groundSpeed()->setRawValue(calculatedSpeed);
    }
    
    // Calculate heading from velocity components
    if (std::abs(groundSpeed) > 0.1 || std::abs(velocityY) > 0.1) {
        double headingDeg = std::atan2(groundSpeed, velocityY) * 180.0 / M_PI;
        if (headingDeg < 0) headingDeg += 360.0;
        this->heading()->setRawValue(headingDeg);
        this->courseOverGround()->setRawValue(headingDeg);
    }
}

void VehicleGPSFactGroupRedesigned::_updateGpsFactsFromHighLatency(const mavlink_high_latency_t& highLatency)
{
    // High latency message provides simplified GPS information
    double latitude = static_cast<double>(highLatency.latitude) / 1e7;
    double longitude = static_cast<double>(highLatency.longitude) / 1e7;
    double altitude = static_cast<double>(highLatency.altitude_amsl); // Already in meters
    int groundSpeedVal = static_cast<int>(highLatency.groundspeed);
    int headingVal = static_cast<int>(highLatency.heading / 100.0); // cdeg to deg
    
    if (_isValidCoordinate(latitude, longitude)) {
        lat()->setRawValue(latitude);
        lon()->setRawValue(longitude);
        mgrs()->setRawValue(_convertGeoToMGRS(latitude, longitude));
    }
    
    if (!std::isnan(altitude)) {
        alt()->setRawValue(altitude);
    }
    
    if (groundSpeedVal >= 0) {
        this->groundSpeed()->setRawValue(static_cast<double>(groundSpeedVal));
    }
    
    if (headingVal >= 0 && headingVal <= 360) {
        this->heading()->setRawValue(static_cast<double>(headingVal));
        this->courseOverGround()->setRawValue(static_cast<double>(headingVal));
    }
}

void VehicleGPSFactGroupRedesigned::_updateGpsFactsFromHighLatency2(const mavlink_high_latency2_t& highLatency2)
{
    // High latency 2 message provides more detailed GPS information
    double latitude = static_cast<double>(highLatency2.latitude) / 1e7;
    double longitude = static_cast<double>(highLatency2.longitude) / 1e7;
    double altitude = static_cast<double>(highLatency2.altitude) / 1e3;
    double groundSpeed = static_cast<double>(highLatency2.groundspeed) / 10.0; // dm/s to m/s
    double heading = static_cast<double>(highLatency2.heading) / 100.0; // deg*100 to degrees
    
    if (_isValidCoordinate(latitude, longitude)) {
        lat()->setRawValue(latitude);
        lon()->setRawValue(longitude);
        mgrs()->setRawValue(_convertGeoToMGRS(latitude, longitude));
    }
    
    if (!std::isnan(altitude)) {
        alt()->setRawValue(altitude);
    }
    
    if (_getValidSpeed(static_cast<uint16_t>(groundSpeed * 100))) {
        this->groundSpeed()->setRawValue(groundSpeed);
    }
    
    if (_getValidAngle(static_cast<uint16_t>(heading * 100))) {
        this->heading()->setRawValue(heading);
        this->courseOverGround()->setRawValue(heading);
    }
}

void VehicleGPSFactGroupRedesigned::_updateGpsStatusFacts(const mavlink_gps_status_t& gpsStatus)
{
    // Update satellite information
    if (_isValidSatelliteCount(gpsStatus.satellites_visible)) {
        gpsStatusSatellitesVisible()->setRawValue(static_cast<int32_t>(gpsStatus.satellites_visible));
    }
    
    // Calculate satellite statistics
    uint8_t usedCount = 0;
    double totalSNR = 0.0;
    uint8_t maxSNR = 0;
    
    for (int i = 0; i < gpsStatus.satellites_visible && i < 20; ++i) {
        if (gpsStatus.satellite_used[i]) {
            usedCount++;
        }
        if (gpsStatus.satellite_snr[i] > 0) {
            totalSNR += gpsStatus.satellite_snr[i];
            if (gpsStatus.satellite_snr[i] > maxSNR) {
                maxSNR = gpsStatus.satellite_snr[i];
            }
        }
    }
    
    gpsStatusSatellitesUsed()->setRawValue(static_cast<int32_t>(usedCount));
    
    if (gpsStatus.satellites_visible > 0) {
        gpsStatusAvgSNR()->setRawValue(totalSNR / gpsStatus.satellites_visible);
    }
    
    gpsStatusMaxSNR()->setRawValue(static_cast<int32_t>(maxSNR));
    
    // Add individual satellite facts if needed
    _addSatelliteFacts(gpsStatus.satellites_visible);
    _updateSatelliteFacts(gpsStatus);
}

void VehicleGPSFactGroupRedesigned::_addSatelliteFacts(uint8_t satelliteCount)
{
    // Only add facts if we need more than we already have
    if (satelliteCount <= _maxSatellites) {
        return;
    }
    
    for (uint8_t i = _maxSatellites; i < satelliteCount && i < 20; ++i) {
        std::string prefix = "satellite" + std::to_string(i);
        _addFact(std::make_shared<Fact>(0, prefix + "PRN", FactMetaData::valueTypeInt32));
        _addFact(std::make_shared<Fact>(0, prefix + "Used", FactMetaData::valueTypeInt32));
        _addFact(std::make_shared<Fact>(0, prefix + "Elevation", FactMetaData::valueTypeInt32));
        _addFact(std::make_shared<Fact>(0, prefix + "Azimuth", FactMetaData::valueTypeInt32));
        _addFact(std::make_shared<Fact>(0, prefix + "SNR", FactMetaData::valueTypeInt32));
    }
    
    _maxSatellites = satelliteCount;
}

void VehicleGPSFactGroupRedesigned::_updateSatelliteFacts(const mavlink_gps_status_t& gpsStatus)
{
    for (int i = 0; i < gpsStatus.satellites_visible && i < 20; ++i) {
        std::string prefix = "satellite" + std::to_string(i);
        
        if (auto prnFact = getFact(prefix + "PRN")) {
            prnFact->setRawValue(static_cast<int32_t>(gpsStatus.satellite_prn[i]));
        }
        if (auto usedFact = getFact(prefix + "Used")) {
            usedFact->setRawValue(static_cast<int32_t>(gpsStatus.satellite_used[i]));
        }
        if (auto elevFact = getFact(prefix + "Elevation")) {
            elevFact->setRawValue(static_cast<int32_t>(gpsStatus.satellite_elevation[i]));
        }
        if (auto azimFact = getFact(prefix + "Azimuth")) {
            azimFact->setRawValue(static_cast<int32_t>(gpsStatus.satellite_azimuth[i]));
        }
        if (auto snrFact = getFact(prefix + "SNR")) {
            snrFact->setRawValue(static_cast<int32_t>(gpsStatus.satellite_snr[i]));
        }
    }
}

std::shared_ptr<Fact> VehicleGPSFactGroupRedesigned::satellitePRN(uint8_t index)
{
    std::string name = "satellite" + std::to_string(index) + "PRN";
    return getFact(name);
}

std::shared_ptr<Fact> VehicleGPSFactGroupRedesigned::satelliteUsed(uint8_t index)
{
    std::string name = "satellite" + std::to_string(index) + "Used";
    return getFact(name);
}

std::shared_ptr<Fact> VehicleGPSFactGroupRedesigned::satelliteElevation(uint8_t index)
{
    std::string name = "satellite" + std::to_string(index) + "Elevation";
    return getFact(name);
}

std::shared_ptr<Fact> VehicleGPSFactGroupRedesigned::satelliteAzimuth(uint8_t index)
{
    std::string name = "satellite" + std::to_string(index) + "Azimuth";
    return getFact(name);
}

std::shared_ptr<Fact> VehicleGPSFactGroupRedesigned::satelliteSNR(uint8_t index)
{
    std::string name = "satellite" + std::to_string(index) + "SNR";
    return getFact(name);
}

std::string VehicleGPSFactGroupRedesigned::_convertGeoToMGRS(double latitude, double longitude)
{
    // Simplified MGRS conversion - this is a basic implementation
    // In a production system, you would use a proper MGRS library
    
    if (!_isValidCoordinate(latitude, longitude)) {
        return "INVALID";
    }
    
    // For now, return a simplified representation
    // This should be replaced with a proper MGRS conversion library
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "MGRS-" << latitude << "," << longitude;
    return oss.str();
}

// Data validation helpers
bool VehicleGPSFactGroupRedesigned::_isValidCoordinate(double lat, double lon)
{
    return !std::isnan(lat) && !std::isnan(lon) && 
           lat >= -90.0 && lat <= 90.0 && 
           lon >= -180.0 && lon <= 180.0;
}

bool VehicleGPSFactGroupRedesigned::_isValidFixType(uint8_t fixType)
{
    return fixType >= 0 && fixType <= 8; // MAVLink GPS fix types
}

bool VehicleGPSFactGroupRedesigned::_isValidSatelliteCount(uint8_t count)
{
    return count > 0 && count <= 50; // Reasonable satellite count range
}

double VehicleGPSFactGroupRedesigned::_getValidAccuracy(uint16_t rawValue)
{
    double accuracy = static_cast<double>(rawValue) / 100.0; // Convert from cm to meters
    return (accuracy >= 0.0 && accuracy <= 9999.0) ? accuracy : INVALID_ACCURACY;
}

double VehicleGPSFactGroupRedesigned::_getValidAngle(uint16_t rawValue)
{
    double angle = static_cast<double>(rawValue) / 100.0; // Convert from deg*100 to degrees
    return (angle >= 0.0 && angle <= 360.0) ? angle : INVALID_ANGLE;
}

double VehicleGPSFactGroupRedesigned::_getValidSpeed(uint16_t rawValue)
{
    double speed = static_cast<double>(rawValue) / 100.0; // Convert from cm/s to m/s
    return (speed >= 0.0 && speed <= 1000.0) ? speed : INVALID_SPEED;
}