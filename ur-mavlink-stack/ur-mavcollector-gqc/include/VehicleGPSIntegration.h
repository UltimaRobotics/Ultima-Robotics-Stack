#pragma once

#include "VehicleGPSFactGroupRedesigned.h"
#include "VehicleGPS2FactGroupRedesigned.h"
#include "VehicleGPSFactGroup.h"

/// Integration layer for the redesigned GPS system
/// Provides seamless integration with existing Vehicle architecture
class VehicleGPSIntegration
{
public:
    explicit VehicleGPSIntegration();
    virtual ~VehicleGPSIntegration() = default;

    // Get the redesigned GPS fact groups
    std::shared_ptr<VehicleGPSFactGroupRedesigned> gpsFactGroup() { return _gpsFactGroup; }
    std::shared_ptr<VehicleGPS2FactGroupRedesigned> gps2FactGroup() { return _gps2FactGroup; }
    
    // Integration methods for existing Vehicle class
    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message);
    void initializeFactGroups();
    
    // Migration helpers for existing code
    static std::shared_ptr<VehicleGPSFactGroupRedesigned> migrateFromLegacy(
        std::shared_ptr<VehicleGPSFactGroup> legacyGroup);

private:
    std::shared_ptr<VehicleGPSFactGroupRedesigned> _gpsFactGroup;
    std::shared_ptr<VehicleGPS2FactGroupRedesigned> _gps2FactGroup;
    
    // Message routing helpers
    void _routeMessage(Vehicle *vehicle, const mavlink_message_t &message);
    bool _isGpsMessage(uint32_t messageId);
};
