#include "VehicleFactGroup.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>
#include <cmath>

VehicleFactGroup::VehicleFactGroup(bool ignoreCamelCase)
    : FactGroup(100, ignoreCamelCase) // Update every 100ms
{
    // Add all vehicle facts
    _addFact(std::make_shared<Fact>(0, "roll", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "pitch", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "heading", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "rollRate", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "pitchRate", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "yawRate", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "groundSpeed", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "airSpeed", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "airSpeedSetpoint", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "climbRate", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "altitudeRelative", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "altitudeAMSL", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "altitudeAboveTerr", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "altitudeTuning", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "altitudeTuningSetpoint", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "xTrackError", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "rangeFinderDist", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "flightDistance", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "distanceToHome", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "timeToHome", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "missionItemIndex", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "headingToNextWP", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "distanceToNextWP", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "headingToHome", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "headingFromHome", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "headingFromGCS", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "distanceToGCS", FactMetaData::valueTypeDouble));
    _addFact(std::make_shared<Fact>(0, "hobbs", FactMetaData::valueTypeString));
    _addFact(std::make_shared<Fact>(0, "throttlePct", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "imuTemp", FactMetaData::valueTypeInt16));
}

void VehicleFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_ATTITUDE:
            _handleAttitude(vehicle, message);
            break;
            
        case MAVLINK_MSG_ID_ATTITUDE_QUATERNION:
            _handleAttitudeQuaternion(vehicle, message);
            break;
            
        case MAVLINK_MSG_ID_ALTITUDE:
            _handleAltitude(message);
            break;
            
        case MAVLINK_MSG_ID_VFR_HUD:
            _handleVfrHud(message);
            break;
            
        case MAVLINK_MSG_ID_RAW_IMU:
        case MAVLINK_MSG_ID_SCALED_IMU2:
        case MAVLINK_MSG_ID_SCALED_IMU3:
            _handleRawImuTemp(message);
            break;
            
        case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT:
            _handleNavControllerOutput(message);
            break;
            
        default:
            break;
    }
}

void VehicleFactGroup::_handleAttitude(Vehicle *vehicle, const mavlink_message_t &message)
{
    mavlink_attitude_t attitude;
    mavlink_msg_attitude_decode(&message, &attitude);
    
    _handleAttitudeWorker(attitude.roll, attitude.pitch, attitude.yaw);
    
    // Update rates
    rollRate()->setRawValue(static_cast<double>(attitude.rollspeed * 180.0 / M_PI));
    pitchRate()->setRawValue(static_cast<double>(attitude.pitchspeed * 180.0 / M_PI));
    yawRate()->setRawValue(static_cast<double>(attitude.yawspeed * 180.0 / M_PI));
    
    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleAttitudeQuaternion(Vehicle *vehicle, const mavlink_message_t &message)
{
    mavlink_attitude_quaternion_t attitudeQuat;
    mavlink_msg_attitude_quaternion_decode(&message, &attitudeQuat);
    
    // Convert quaternion to Euler angles
    double q1 = attitudeQuat.q1;
    double q2 = attitudeQuat.q2;
    double q3 = attitudeQuat.q3;
    double q4 = attitudeQuat.q4;
    
    double roll = std::atan2(2.0 * (q2*q3 + q1*q4), 1.0 - 2.0 * (q3*q3 + q4*q4));
    double pitch = std::asin(2.0 * (q1*q3 - q4*q2));
    double yaw = std::atan2(2.0 * (q1*q2 + q3*q4), 1.0 - 2.0 * (q2*q2 + q3*q3));
    
    _handleAttitudeWorker(roll, pitch, yaw);
    
    // Update rates
    rollRate()->setRawValue(static_cast<double>(attitudeQuat.rollspeed * 180.0 / M_PI));
    pitchRate()->setRawValue(static_cast<double>(attitudeQuat.pitchspeed * 180.0 / M_PI));
    yawRate()->setRawValue(static_cast<double>(attitudeQuat.yawspeed * 180.0 / M_PI));
    
    _receivingAttitudeQuaternion = true;
    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleAltitude(const mavlink_message_t &message)
{
    mavlink_altitude_t altitude;
    mavlink_msg_altitude_decode(&message, &altitude);
    
    altitudeAMSL()->setRawValue(static_cast<double>(altitude.altitude_amsl));
    altitudeRelative()->setRawValue(static_cast<double>(altitude.altitude_local));
    altitudeAboveTerr()->setRawValue(static_cast<double>(altitude.altitude_relative));
    
    _altitudeMessageAvailable = true;
    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleVfrHud(const mavlink_message_t &message)
{
    mavlink_vfr_hud_t vfrHud;
    mavlink_msg_vfr_hud_decode(&message, &vfrHud);
    
    groundSpeed()->setRawValue(static_cast<double>(vfrHud.groundspeed));
    airSpeed()->setRawValue(static_cast<double>(vfrHud.airspeed));
    climbRate()->setRawValue(static_cast<double>(vfrHud.climb));
    throttlePct()->setRawValue(static_cast<uint16_t>(vfrHud.throttle * 100.0));
    
    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleRawImuTemp(const mavlink_message_t &message)
{
    // Temperature field is not available in this MAVLink version
    // Just set telemetry as available
    
    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleNavControllerOutput(const mavlink_message_t &message)
{
    mavlink_nav_controller_output_t navController;
    mavlink_msg_nav_controller_output_decode(&message, &navController);
    
    headingToNextWP()->setRawValue(static_cast<double>(navController.target_bearing));
    distanceToNextWP()->setRawValue(static_cast<double>(navController.wp_dist));
    xTrackError()->setRawValue(static_cast<double>(navController.xtrack_error));
    
    _setTelemetryAvailable(true);
}

void VehicleFactGroup::_handleAttitudeWorker(double rollRadians, double pitchRadians, double yawRadians)
{
    // Convert radians to degrees
    double rollDegrees = rollRadians * 180.0 / M_PI;
    double pitchDegrees = pitchRadians * 180.0 / M_PI;
    double yawDegrees = yawRadians * 180.0 / M_PI;
    
    // Normalize heading to 0-360
    if (yawDegrees < 0.0) {
        yawDegrees += 360.0;
    } else if (yawDegrees >= 360.0) {
        yawDegrees -= 360.0;
    }
    
    roll()->setRawValue(rollDegrees);
    pitch()->setRawValue(pitchDegrees);
    heading()->setRawValue(yawDegrees);
}
