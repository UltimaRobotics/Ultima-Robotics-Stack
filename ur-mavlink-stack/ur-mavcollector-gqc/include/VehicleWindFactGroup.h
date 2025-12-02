#pragma once

#include "FactGroup.h"

/// Wind FactGroup containing wind telemetry data.
/// This is a Qt-free port of QGroundControl's VehicleWindFactGroup.
class VehicleWindFactGroup : public FactGroup
{
public:
    explicit VehicleWindFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleWindFactGroup() = default;

    // Fact accessors
    std::shared_ptr<Fact> direction() { return getFact("direction"); }
    std::shared_ptr<Fact> speed() { return getFact("speed"); }
    std::shared_ptr<Fact> speedZ() { return getFact("speedZ"); }

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

private:
    // Wind handler methods removed since wind message types are not available in this MAVLink version
};
