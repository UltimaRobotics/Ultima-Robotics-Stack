#pragma once

#include "FactGroup.h"

/// Temperature FactGroup containing temperature telemetry data.
/// This is a Qt-free port of QGroundControl's VehicleTemperatureFactGroup.
class VehicleTemperatureFactGroup : public FactGroup
{
public:
    explicit VehicleTemperatureFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleTemperatureFactGroup() = default;

    // Fact accessors
    std::shared_ptr<Fact> temperature1() { return getFact("temperature1"); }
    std::shared_ptr<Fact> temperature2() { return getFact("temperature2"); }
    std::shared_ptr<Fact> temperature3() { return getFact("temperature3"); }
    std::shared_ptr<Fact> temperatureCalibrated() { return getFact("temperatureCalibrated"); }

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

protected:
    void _handleScaledPressure(const mavlink_message_t &message);
    void _handleScaledPressure2(const mavlink_message_t &message);
    void _handleScaledPressure3(const mavlink_message_t &message);
    void _handleHighLatency2(const mavlink_message_t &message);
};
