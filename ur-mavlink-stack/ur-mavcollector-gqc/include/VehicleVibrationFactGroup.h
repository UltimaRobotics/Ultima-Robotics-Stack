#pragma once

#include "FactGroup.h"

/// Vibration FactGroup containing vibration telemetry data.
/// This is a Qt-free port of QGroundControl's VehicleVibrationFactGroup.
class VehicleVibrationFactGroup : public FactGroup
{
public:
    explicit VehicleVibrationFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleVibrationFactGroup() = default;

    // Fact accessors
    std::shared_ptr<Fact> vibrationX() { return getFact("vibrationX"); }
    std::shared_ptr<Fact> vibrationY() { return getFact("vibrationY"); }
    std::shared_ptr<Fact> vibrationZ() { return getFact("vibrationZ"); }
    std::shared_ptr<Fact> clipping0() { return getFact("clipping0"); }
    std::shared_ptr<Fact> clipping1() { return getFact("clipping1"); }
    std::shared_ptr<Fact> clipping2() { return getFact("clipping2"); }
    std::shared_ptr<Fact> clipping3() { return getFact("clipping3"); }

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

protected:
    void _handleVibration(const mavlink_message_t &message);
};
