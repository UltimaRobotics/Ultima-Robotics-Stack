#pragma once

#include "FactGroup.h"

/// Main vehicle FactGroup containing core telemetry data.
/// This is a Qt-free port of QGroundControl's VehicleFactGroup.
class VehicleFactGroup : public FactGroup
{
public:
    explicit VehicleFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleFactGroup() = default;

    // Fact accessors
    std::shared_ptr<Fact> roll() { return getFact("roll"); }
    std::shared_ptr<Fact> pitch() { return getFact("pitch"); }
    std::shared_ptr<Fact> heading() { return getFact("heading"); }
    std::shared_ptr<Fact> rollRate() { return getFact("rollRate"); }
    std::shared_ptr<Fact> pitchRate() { return getFact("pitchRate"); }
    std::shared_ptr<Fact> yawRate() { return getFact("yawRate"); }
    std::shared_ptr<Fact> airSpeed() { return getFact("airSpeed"); }
    std::shared_ptr<Fact> airSpeedSetpoint() { return getFact("airSpeedSetpoint"); }
    std::shared_ptr<Fact> groundSpeed() { return getFact("groundSpeed"); }
    std::shared_ptr<Fact> climbRate() { return getFact("climbRate"); }
    std::shared_ptr<Fact> altitudeRelative() { return getFact("altitudeRelative"); }
    std::shared_ptr<Fact> altitudeAMSL() { return getFact("altitudeAMSL"); }
    std::shared_ptr<Fact> altitudeAboveTerr() { return getFact("altitudeAboveTerr"); }
    std::shared_ptr<Fact> altitudeTuning() { return getFact("altitudeTuning"); }
    std::shared_ptr<Fact> altitudeTuningSetpoint() { return getFact("altitudeTuningSetpoint"); }
    std::shared_ptr<Fact> xTrackError() { return getFact("xTrackError"); }
    std::shared_ptr<Fact> rangeFinderDist() { return getFact("rangeFinderDist"); }
    std::shared_ptr<Fact> flightDistance() { return getFact("flightDistance"); }
    std::shared_ptr<Fact> distanceToHome() { return getFact("distanceToHome"); }
    std::shared_ptr<Fact> timeToHome() { return getFact("timeToHome"); }
    std::shared_ptr<Fact> missionItemIndex() { return getFact("missionItemIndex"); }
    std::shared_ptr<Fact> headingToNextWP() { return getFact("headingToNextWP"); }
    std::shared_ptr<Fact> distanceToNextWP() { return getFact("distanceToNextWP"); }
    std::shared_ptr<Fact> headingToHome() { return getFact("headingToHome"); }
    std::shared_ptr<Fact> headingFromHome() { return getFact("headingFromHome"); }
    std::shared_ptr<Fact> headingFromGCS() { return getFact("headingFromGCS"); }
    std::shared_ptr<Fact> distanceToGCS() { return getFact("distanceToGCS"); }
    std::shared_ptr<Fact> hobbs() { return getFact("hobbs"); }
    std::shared_ptr<Fact> throttlePct() { return getFact("throttlePct"); }
    std::shared_ptr<Fact> imuTemp() { return getFact("imuTemp"); }

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

protected:
    void _handleAttitude(Vehicle *vehicle, const mavlink_message_t &message);
    void _handleAttitudeQuaternion(Vehicle *vehicle, const mavlink_message_t &message);
    void _handleAltitude(const mavlink_message_t &message);
    void _handleVfrHud(const mavlink_message_t &message);
    void _handleRawImuTemp(const mavlink_message_t &message);
    void _handleNavControllerOutput(const mavlink_message_t &message);

private:
    void _handleAttitudeWorker(double rollRadians, double pitchRadians, double yawRadians);

    float _altitudeTuningOffset = std::numeric_limits<float>::quiet_NaN();
    bool _altitudeMessageAvailable = false;
    bool _receivingAttitudeQuaternion = false;
};
