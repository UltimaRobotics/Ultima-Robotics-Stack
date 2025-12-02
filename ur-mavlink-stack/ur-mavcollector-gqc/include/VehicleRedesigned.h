#pragma once

#include "Vehicle.h"
#include "VehicleGPSIntegration.h"

/// Enhanced Vehicle class with redesigned GPS system integrated
/// Follows QGroundControl's proven architecture patterns
class VehicleRedesigned : public Vehicle
{
public:
    explicit VehicleRedesigned(MAVLinkInterface* mavlinkInterface);
    virtual ~VehicleRedesigned() = default;

    // Enhanced GPS access methods (QGC-style)
    std::shared_ptr<VehicleGPSFactGroupRedesigned> gpsFactGroupRedesigned() { return _gpsIntegration->gpsFactGroup(); }
    std::shared_ptr<VehicleGPS2FactGroupRedesigned> gps2FactGroupRedesigned() { return _gpsIntegration->gps2FactGroup(); }
    
    // Convenience methods for accessing GPS data (QGC-style)
    double latitude() const;
    double longitude() const;
    double altitude() const;
    double groundSpeed() const;
    double heading() const;
    uint8_t satelliteCount() const;
    uint8_t fixType() const;
    std::string mgrsCoordinate() const;
    
    // Enhanced GPS status methods
    bool hasGpsFix() const;
    bool has3DFix() const;
    bool hasRTKFix() const;
    double hdop() const;
    double vdop() const;

protected:
    // Override message handling to use redesigned GPS system
    void handleMessage(const mavlink_message_t &message) override;

private:
    std::unique_ptr<VehicleGPSIntegration> _gpsIntegration;
    
    // GPS data validation helpers
    bool _isValidGpsData() const;
    void _initializeRedesignedGpsSystem();
};
