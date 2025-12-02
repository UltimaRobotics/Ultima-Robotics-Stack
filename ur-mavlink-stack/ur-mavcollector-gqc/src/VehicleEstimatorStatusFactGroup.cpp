#include "VehicleEstimatorStatusFactGroup.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>

VehicleEstimatorStatusFactGroup::VehicleEstimatorStatusFactGroup(bool ignoreCamelCase)
    : FactGroup(1000, ignoreCamelCase)
{
    _addFact(std::make_shared<Fact>(0, "flags", FactMetaData::valueTypeUint64));
    _addFact(std::make_shared<Fact>(0, "velocityRatio", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "posHorizRatio", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "posVertRatio", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "magRatio", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "haglRatio", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "tasRatio", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "posHorizAccuracy", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "posVertAccuracy", FactMetaData::valueTypeFloat));
    
    // Individual flag facts
    _addFact(std::make_shared<Fact>(0, "flagsAttitude", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsVelocityHoriz", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsVelocityVert", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsPosHorizRel", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsPosHorizAbs", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsPosVertAbs", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsPosVertAGL", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsConstPosMode", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsPredPosHorizRel", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsPredPosHorizAbs", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsExpMode", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsVelHorizSource", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsVelVertSource", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsPosHorizSource", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsPosVertSource", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsMagFieldSource", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsTerrainAltSource", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "flagsYawAlignSource", FactMetaData::valueTypeBool));
}

void VehicleEstimatorStatusFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_ESTIMATOR_STATUS:
            _handleEstimatorStatus(message);
            break;
        default:
            break;
    }
}

void VehicleEstimatorStatusFactGroup::_handleEKFStatusReport(const mavlink_message_t &message)
{
    // EKF status report types are not available in this MAVLink version
    // Just set telemetry as available
    _setTelemetryAvailable(true);
}

void VehicleEstimatorStatusFactGroup::_handleEstimatorStatus(const mavlink_message_t &message)
{
    mavlink_estimator_status_t estimatorStatus;
    mavlink_msg_estimator_status_decode(&message, &estimatorStatus);
    
    flags()->setRawValue(static_cast<uint64_t>(estimatorStatus.flags));
    velocityRatio()->setRawValue(static_cast<float>(estimatorStatus.vel_ratio));
    posHorizRatio()->setRawValue(static_cast<float>(estimatorStatus.pos_horiz_ratio));
    posVertRatio()->setRawValue(static_cast<float>(estimatorStatus.pos_vert_ratio));
    magRatio()->setRawValue(static_cast<float>(estimatorStatus.mag_ratio));
    haglRatio()->setRawValue(static_cast<float>(estimatorStatus.hagl_ratio));
    tasRatio()->setRawValue(static_cast<float>(estimatorStatus.tas_ratio));
    posHorizAccuracy()->setRawValue(static_cast<float>(estimatorStatus.pos_horiz_accuracy));
    posVertAccuracy()->setRawValue(static_cast<float>(estimatorStatus.pos_vert_accuracy));
    
    // Update individual flag facts
    flagsAttitude()->setRawValue((estimatorStatus.flags & ESTIMATOR_ATTITUDE) != 0);
    flagsVelocityHoriz()->setRawValue((estimatorStatus.flags & ESTIMATOR_VELOCITY_HORIZ) != 0);
    flagsVelocityVert()->setRawValue((estimatorStatus.flags & ESTIMATOR_VELOCITY_VERT) != 0);
    flagsPosHorizRel()->setRawValue((estimatorStatus.flags & ESTIMATOR_POS_HORIZ_REL) != 0);
    flagsPosHorizAbs()->setRawValue((estimatorStatus.flags & ESTIMATOR_POS_HORIZ_ABS) != 0);
    flagsPosVertAbs()->setRawValue((estimatorStatus.flags & ESTIMATOR_POS_VERT_ABS) != 0);
    flagsPosVertAGL()->setRawValue((estimatorStatus.flags & ESTIMATOR_POS_VERT_AGL) != 0);
    flagsConstPosMode()->setRawValue((estimatorStatus.flags & ESTIMATOR_CONST_POS_MODE) != 0);
    flagsPredPosHorizRel()->setRawValue((estimatorStatus.flags & ESTIMATOR_PRED_POS_HORIZ_REL) != 0);
    flagsPredPosHorizAbs()->setRawValue((estimatorStatus.flags & ESTIMATOR_PRED_POS_HORIZ_ABS) != 0);
    // Commented out unavailable estimator status constants for this MAVLink version
    // flagsExpMode()->setRawValue((estimatorStatus.flags & ESTIMATOR_EXP_MODE) != 0);
    // flagsVelHorizSource()->setRawValue((estimatorStatus.flags & ESTIMATOR_VELOCITY_HORIZ_SOURCE) != 0);
    // flagsVelVertSource()->setRawValue((estimatorStatus.flags & ESTIMATOR_VELOCITY_VERT_SOURCE) != 0);
    // flagsPosHorizSource()->setRawValue((estimatorStatus.flags & ESTIMATOR_POS_HORIZ_SOURCE) != 0);
    // flagsPosVertSource()->setRawValue((estimatorStatus.flags & ESTIMATOR_POS_VERT_SOURCE) != 0);
    // flagsMagFieldSource()->setRawValue((estimatorStatus.flags & ESTIMATOR_MAG_FIELD_SOURCE) != 0);
    // flagsTerrainAltSource()->setRawValue((estimatorStatus.flags & ESTIMATOR_TERRAIN_ALT_SOURCE) != 0);
    // flagsYawAlignSource()->setRawValue((estimatorStatus.flags & ESTIMATOR_YAW_ALIGN_SOURCE) != 0);
    
    _setTelemetryAvailable(true);
}
