#pragma once

#include "FactGroup.h"

/// Estimator Status FactGroup containing estimator status telemetry data.
/// This is a Qt-free port of QGroundControl's VehicleEstimatorStatusFactGroup.
class VehicleEstimatorStatusFactGroup : public FactGroup
{
public:
    explicit VehicleEstimatorStatusFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleEstimatorStatusFactGroup() = default;

    // Fact accessors
    std::shared_ptr<Fact> flags() { return getFact("flags"); }
    std::shared_ptr<Fact> velocityRatio() { return getFact("velocityRatio"); }
    std::shared_ptr<Fact> posHorizRatio() { return getFact("posHorizRatio"); }
    std::shared_ptr<Fact> posVertRatio() { return getFact("posVertRatio"); }
    std::shared_ptr<Fact> magRatio() { return getFact("magRatio"); }
    std::shared_ptr<Fact> haglRatio() { return getFact("haglRatio"); }
    std::shared_ptr<Fact> tasRatio() { return getFact("tasRatio"); }
    std::shared_ptr<Fact> posHorizAccuracy() { return getFact("posHorizAccuracy"); }
    std::shared_ptr<Fact> posVertAccuracy() { return getFact("posVertAccuracy"); }
    std::shared_ptr<Fact> flagsAttitude() { return getFact("flagsAttitude"); }
    std::shared_ptr<Fact> flagsVelocityHoriz() { return getFact("flagsVelocityHoriz"); }
    std::shared_ptr<Fact> flagsVelocityVert() { return getFact("flagsVelocityVert"); }
    std::shared_ptr<Fact> flagsPosHorizRel() { return getFact("flagsPosHorizRel"); }
    std::shared_ptr<Fact> flagsPosHorizAbs() { return getFact("flagsPosHorizAbs"); }
    std::shared_ptr<Fact> flagsPosVertAbs() { return getFact("flagsPosVertAbs"); }
    std::shared_ptr<Fact> flagsPosVertAGL() { return getFact("flagsPosVertAGL"); }
    std::shared_ptr<Fact> flagsConstPosMode() { return getFact("flagsConstPosMode"); }
    std::shared_ptr<Fact> flagsPredPosHorizRel() { return getFact("flagsPredPosHorizRel"); }
    std::shared_ptr<Fact> flagsPredPosHorizAbs() { return getFact("flagsPredPosHorizAbs"); }
    std::shared_ptr<Fact> flagsExpMode() { return getFact("flagsExpMode"); }
    std::shared_ptr<Fact> flagsVelHorizSource() { return getFact("flagsVelHorizSource"); }
    std::shared_ptr<Fact> flagsVelVertSource() { return getFact("flagsVelVertSource"); }
    std::shared_ptr<Fact> flagsPosHorizSource() { return getFact("flagsPosHorizSource"); }
    std::shared_ptr<Fact> flagsPosVertSource() { return getFact("flagsPosVertSource"); }
    std::shared_ptr<Fact> flagsMagFieldSource() { return getFact("flagsMagFieldSource"); }
    std::shared_ptr<Fact> flagsTerrainAltSource() { return getFact("flagsTerrainAltSource"); }
    std::shared_ptr<Fact> flagsYawAlignSource() { return getFact("flagsYawAlignSource"); }

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

protected:
    void _handleEKFStatusReport(const mavlink_message_t &message);
    void _handleEstimatorStatus(const mavlink_message_t &message);
};
