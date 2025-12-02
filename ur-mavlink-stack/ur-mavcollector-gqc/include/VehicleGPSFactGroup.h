#pragma once

#include "FactGroup.h"

// Forward declare GPS status structure to avoid header conflicts
struct __mavlink_gps_status_t;

/// GPS FactGroup containing GPS telemetry data.
/// This is a Qt-free port of QGroundControl's VehicleGPSFactGroup.
/// Enhanced with comprehensive GPS status and satellite data.
class VehicleGPSFactGroup : public FactGroup
{
public:
    explicit VehicleGPSFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleGPSFactGroup() = default;

    // Basic GPS fact accessors
    std::shared_ptr<Fact> lat() { return getFact("lat"); }
    std::shared_ptr<Fact> lon() { return getFact("lon"); }
    std::shared_ptr<Fact> alt() { return getFact("alt"); }
    std::shared_ptr<Fact> altEllipsoid() { return getFact("altEllipsoid"); }
    std::shared_ptr<Fact> hdop() { return getFact("hdop"); }
    std::shared_ptr<Fact> vdop() { return getFact("vdop"); }
    std::shared_ptr<Fact> course() { return getFact("course"); }
    std::shared_ptr<Fact> groundSpeed() { return getFact("groundSpeed"); }
    std::shared_ptr<Fact> count() { return getFact("count"); }
    std::shared_ptr<Fact> lock() { return getFact("lock"); }
    std::shared_ptr<Fact> satellitesVisible() { return getFact("satellitesVisible"); }
    std::shared_ptr<Fact> utcDate() { return getFact("utcDate"); }
    std::shared_ptr<Fact> utcTime() { return getFact("utcTime"); }
    std::shared_ptr<Fact> timeUtc() { return getFact("timeUtc"); }
    std::shared_ptr<Fact> fixType() { return getFact("fixType"); }
    std::shared_ptr<Fact> eph() { return getFact("eph"); }
    std::shared_ptr<Fact> epv() { return getFact("epv"); }
    std::shared_ptr<Fact> heading() { return getFact("heading"); }
    std::shared_ptr<Fact> speedAccuracy() { return getFact("speedAccuracy"); }
    std::shared_ptr<Fact> horizAccuracy() { return getFact("horizAccuracy"); }
    std::shared_ptr<Fact> vertAccuracy() { return getFact("vertAccuracy"); }
    std::shared_ptr<Fact> yaw() { return getFact("yaw"); }
    std::shared_ptr<Fact> yawAccuracy() { return getFact("yawAccuracy"); }
    
    // GPS status and satellite data accessors
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

protected:
    // GPS message handlers
    void _handleGPSRawInt(const mavlink_message_t &message);
    void _handleGPS2Raw(const mavlink_message_t &message);
    void _handleGlobalPositionInt(const mavlink_message_t &message);
    void _handleHighLatency2(const mavlink_message_t &message);
    void _handleGPSStatus(const mavlink_message_t &message);
    
    // Helper methods for GPS status processing
    void _updateGPSStatusFacts(const __mavlink_gps_status_t& gpsStatus);
    void _addSatelliteFacts(uint8_t satelliteCount);
    void _updateSatelliteFacts(const __mavlink_gps_status_t& gpsStatus);
    
private:
    // Track maximum number of satellites for dynamic fact creation
    uint8_t _maxSatellites = 0;
};
