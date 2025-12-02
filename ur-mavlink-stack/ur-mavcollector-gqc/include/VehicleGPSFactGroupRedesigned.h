#pragma once

#include "FactGroup.h"

/// GPS FactGroup redesigned following QGroundControl's proven approach
/// Enhanced with proper data validation, MGRS coordinate conversion, and robust error handling
class VehicleGPSFactGroupRedesigned : public FactGroup
{
public:
    explicit VehicleGPSFactGroupRedesigned(bool ignoreCamelCase = false);
    virtual ~VehicleGPSFactGroupRedesigned() = default;

    // Core GPS fact accessors (following QGC naming conventions)
    std::shared_ptr<Fact> lat() { return getFact("lat"); }
    std::shared_ptr<Fact> lon() { return getFact("lon"); }
    std::shared_ptr<Fact> mgrs() { return getFact("mgrs"); }           // MGRS coordinate (QGC enhancement)
    std::shared_ptr<Fact> hdop() { return getFact("hdop"); }
    std::shared_ptr<Fact> vdop() { return getFact("vdop"); }
    std::shared_ptr<Fact> courseOverGround() { return getFact("courseOverGround"); }  // QGC naming
    std::shared_ptr<Fact> yaw() { return getFact("yaw"); }             // QGC enhancement
    std::shared_ptr<Fact> count() { return getFact("count"); }         // QGC naming (satellites visible)
    std::shared_ptr<Fact> lock() { return getFact("lock"); }           // QGC naming (fix type)
    
    // Additional GPS facts for comprehensive data
    std::shared_ptr<Fact> alt() { return getFact("alt"); }
    std::shared_ptr<Fact> altEllipsoid() { return getFact("altEllipsoid"); }
    std::shared_ptr<Fact> groundSpeed() { return getFact("groundSpeed"); }
    std::shared_ptr<Fact> speedAccuracy() { return getFact("speedAccuracy"); }
    std::shared_ptr<Fact> horizAccuracy() { return getFact("horizAccuracy"); }
    std::shared_ptr<Fact> vertAccuracy() { return getFact("vertAccuracy"); }
    std::shared_ptr<Fact> heading() { return getFact("heading"); }
    std::shared_ptr<Fact> utcDate() { return getFact("utcDate"); }
    std::shared_ptr<Fact> utcTime() { return getFact("utcTime"); }
    std::shared_ptr<Fact> timeUtc() { return getFact("timeUtc"); }
    std::shared_ptr<Fact> fixType() { return getFact("fixType"); }
    std::shared_ptr<Fact> eph() { return getFact("eph"); }
    std::shared_ptr<Fact> epv() { return getFact("epv"); }
    
    // GPS status and satellite data (enhanced beyond QGC)
    std::shared_ptr<Fact> gpsStatusSatellitesVisible() { return getFact("gpsStatusSatellitesVisible"); }
    std::shared_ptr<Fact> gpsStatusSatellitesUsed() { return getFact("gpsStatusSatellitesUsed"); }
    std::shared_ptr<Fact> gpsStatusAvgSNR() { return getFact("gpsStatusAvgSNR"); }
    std::shared_ptr<Fact> gpsStatusMaxSNR() { return getFact("gpsStatusMaxSNR"); }
    
    // Individual satellite data accessors (up to 20 satellites)
    std::shared_ptr<Fact> satellitePRN(uint8_t index);
    std::shared_ptr<Fact> satelliteUsed(uint8_t index);
    std::shared_ptr<Fact> satelliteElevation(uint8_t index);
    std::shared_ptr<Fact> satelliteAzimuth(uint8_t index);
    std::shared_ptr<Fact> satelliteSNR(uint8_t index);

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;
    
    // Public MGRS conversion method for integration
    std::string _convertGeoToMGRS(double latitude, double longitude);

protected:
    // GPS message handlers (following QGC patterns)
    void _handleGpsRawInt(const mavlink_message_t &message);      // QGC naming
    void _handleGps2Raw(const mavlink_message_t &message);       // QGC naming
    void _handleGlobalPositionInt(const mavlink_message_t &message);
    void _handleHighLatency(const mavlink_message_t &message);   // QGC addition
    void _handleHighLatency2(const mavlink_message_t &message);
    void _handleGpsStatus(const mavlink_message_t &message);     // Enhanced beyond QGC
    
    // Helper methods for GPS data processing (QGC-inspired)
    void _updateGpsFactsFromRawInt(const mavlink_gps_raw_int_t& gpsRaw);
    void _updateGpsFactsFromGps2Raw(const mavlink_gps2_raw_t& gps2Raw);
    void _updateGpsFactsFromGlobalPositionInt(const mavlink_global_position_int_t& globalPos);
    void _updateGpsFactsFromHighLatency(const mavlink_high_latency_t& highLatency);
    void _updateGpsFactsFromHighLatency2(const mavlink_high_latency2_t& highLatency2);
    void _updateGpsStatusFacts(const mavlink_gps_status_t& gpsStatus);
    
    // Satellite data management
    void _addSatelliteFacts(uint8_t satelliteCount);
    void _updateSatelliteFacts(const mavlink_gps_status_t& gpsStatus);
    
    // Data validation helpers (QGC-inspired robustness)
    bool _isValidCoordinate(double lat, double lon);
    bool _isValidFixType(uint8_t fixType);
    bool _isValidSatelliteCount(uint8_t count);
    double _getValidAccuracy(uint16_t rawValue);
    double _getValidAngle(uint16_t rawValue);
    double _getValidSpeed(uint16_t rawValue);
    
private:
    // Track maximum number of satellites for dynamic fact creation
    uint8_t _maxSatellites = 0;
    
    // Data validation constants (QGC-inspired)
    static constexpr double INVALID_COORDINATE = std::numeric_limits<double>::quiet_NaN();
    static constexpr double INVALID_ACCURACY = std::numeric_limits<double>::quiet_NaN();
    static constexpr double INVALID_ANGLE = std::numeric_limits<double>::quiet_NaN();
    static constexpr double INVALID_SPEED = std::numeric_limits<double>::quiet_NaN();
    static constexpr uint8_t INVALID_SATELLITE_COUNT = 0;
    static constexpr uint8_t INVALID_FIX_TYPE = 0;
};
