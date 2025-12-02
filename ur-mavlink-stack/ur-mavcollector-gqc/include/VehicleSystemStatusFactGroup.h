#pragma once

#include "FactGroup.h"

/// System Status FactGroup containing system status telemetry data.
/// This is a Qt-free port of QGroundControl's system status telemetry.
class VehicleSystemStatusFactGroup : public FactGroup
{
public:
    explicit VehicleSystemStatusFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleSystemStatusFactGroup() = default;

    // Fact accessors
    std::shared_ptr<Fact> onboardControlSensorsPresent() { return getFact("onboardControlSensorsPresent"); }
    std::shared_ptr<Fact> onboardControlSensorsEnabled() { return getFact("onboardControlSensorsEnabled"); }
    std::shared_ptr<Fact> onboardControlSensorsHealth() { return getFact("onboardControlSensorsHealth"); }
    std::shared_ptr<Fact> load() { return getFact("load"); }
    std::shared_ptr<Fact> voltageBattery() { return getFact("voltageBattery"); }
    std::shared_ptr<Fact> currentBattery() { return getFact("currentBattery"); }
    std::shared_ptr<Fact> batteryRemaining() { return getFact("batteryRemaining"); }
    std::shared_ptr<Fact> dropRateComm() { return getFact("dropRateComm"); }
    std::shared_ptr<Fact> errorsComm() { return getFact("errorsComm"); }
    std::shared_ptr<Fact> errorsCount1() { return getFact("errorsCount1"); }
    std::shared_ptr<Fact> errorsCount2() { return getFact("errorsCount2"); }
    std::shared_ptr<Fact> errorsCount3() { return getFact("errorsCount3"); }
    std::shared_ptr<Fact> errorsCount4() { return getFact("errorsCount4"); }
    std::shared_ptr<Fact> sensorsPresent3dGyro() { return getFact("sensorsPresent3dGyro"); }
    std::shared_ptr<Fact> sensorsPresent3dAccel() { return getFact("sensorsPresent3dAccel"); }
    std::shared_ptr<Fact> sensorsPresent3dMag() { return getFact("sensorsPresent3dMag"); }
    std::shared_ptr<Fact> sensorsPresentAbsPressure() { return getFact("sensorsPresentAbsPressure"); }
    std::shared_ptr<Fact> sensorsPresentDiffPressure() { return getFact("sensorsPresentDiffPressure"); }
    std::shared_ptr<Fact> sensorsPresentGps() { return getFact("sensorsPresentGps"); }
    std::shared_ptr<Fact> sensorsEnabled3dGyro() { return getFact("sensorsEnabled3dGyro"); }
    std::shared_ptr<Fact> sensorsEnabled3dAccel() { return getFact("sensorsEnabled3dAccel"); }
    std::shared_ptr<Fact> sensorsEnabled3dMag() { return getFact("sensorsEnabled3dMag"); }
    std::shared_ptr<Fact> sensorsEnabledAbsPressure() { return getFact("sensorsEnabledAbsPressure"); }
    std::shared_ptr<Fact> sensorsEnabledDiffPressure() { return getFact("sensorsEnabledDiffPressure"); }
    std::shared_ptr<Fact> sensorsEnabledGps() { return getFact("sensorsEnabledGps"); }
    std::shared_ptr<Fact> sensorsHealth3dGyro() { return getFact("sensorsHealth3dGyro"); }
    std::shared_ptr<Fact> sensorsHealth3dAccel() { return getFact("sensorsHealth3dAccel"); }
    std::shared_ptr<Fact> sensorsHealth3dMag() { return getFact("sensorsHealth3dMag"); }
    std::shared_ptr<Fact> sensorsHealthAbsPressure() { return getFact("sensorsHealthAbsPressure"); }
    std::shared_ptr<Fact> sensorsHealthDiffPressure() { return getFact("sensorsHealthDiffPressure"); }
    std::shared_ptr<Fact> sensorsHealthGps() { return getFact("sensorsHealthGps"); }

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

protected:
    void _handleSysStatus(const mavlink_message_t &message);
};
