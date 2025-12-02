#pragma once

#include "VehicleGPSFactGroupRedesigned.h"

/// GPS2 FactGroup redesigned following QGroundControl's inheritance pattern
/// Inherits from GPS FactGroup and overrides GPS2_RAW message handling
class VehicleGPS2FactGroupRedesigned : public VehicleGPSFactGroupRedesigned
{
public:
    explicit VehicleGPS2FactGroupRedesigned(bool ignoreCamelCase = false);
    virtual ~VehicleGPS2FactGroupRedesigned() = default;

    // Override handleMessage to handle GPS2 specific messages (QGC pattern)
    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

protected:
    // GPS2 specific message handler (QGC naming)
    void _handleGps2Raw(const mavlink_message_t &message);
};
