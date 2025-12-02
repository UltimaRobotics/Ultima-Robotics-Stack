#include "VehicleSystemStatusFactGroup.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>

VehicleSystemStatusFactGroup::VehicleSystemStatusFactGroup(bool ignoreCamelCase)
    : FactGroup(1000, ignoreCamelCase) // Update every 1 second
{
    // Add all system status facts
    _addFact(std::make_shared<Fact>(0, "onboardControlSensorsPresent", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "onboardControlSensorsEnabled", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "onboardControlSensorsHealth", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "load", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "voltageBattery", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "currentBattery", FactMetaData::valueTypeInt16));
    _addFact(std::make_shared<Fact>(0, "batteryRemaining", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "dropRateComm", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "errorsComm", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "errorsCount1", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "errorsCount2", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "errorsCount3", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "errorsCount4", FactMetaData::valueTypeUint32));
    
    // Individual sensor facts
    _addFact(std::make_shared<Fact>(0, "sensorsPresent3dGyro", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsPresent3dAccel", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsPresent3dMag", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsPresentAbsPressure", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsPresentDiffPressure", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsPresentGps", FactMetaData::valueTypeBool));
    
    _addFact(std::make_shared<Fact>(0, "sensorsEnabled3dGyro", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsEnabled3dAccel", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsEnabled3dMag", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsEnabledAbsPressure", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsEnabledDiffPressure", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsEnabledGps", FactMetaData::valueTypeBool));
    
    _addFact(std::make_shared<Fact>(0, "sensorsHealth3dGyro", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsHealth3dAccel", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsHealth3dMag", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsHealthAbsPressure", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsHealthDiffPressure", FactMetaData::valueTypeBool));
    _addFact(std::make_shared<Fact>(0, "sensorsHealthGps", FactMetaData::valueTypeBool));
}

void VehicleSystemStatusFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_SYS_STATUS:
            _handleSysStatus(message);
            break;
    }
}

void VehicleSystemStatusFactGroup::_handleSysStatus(const mavlink_message_t &message)
{
    mavlink_sys_status_t sysStatus;
    mavlink_msg_sys_status_decode(&message, &sysStatus);
    
    onboardControlSensorsPresent()->setRawValue(static_cast<uint32_t>(sysStatus.onboard_control_sensors_present));
    onboardControlSensorsEnabled()->setRawValue(static_cast<uint32_t>(sysStatus.onboard_control_sensors_enabled));
    onboardControlSensorsHealth()->setRawValue(static_cast<uint32_t>(sysStatus.onboard_control_sensors_health));
    load()->setRawValue(static_cast<uint16_t>(sysStatus.load));
    voltageBattery()->setRawValue(static_cast<uint16_t>(sysStatus.voltage_battery));
    currentBattery()->setRawValue(static_cast<int16_t>(sysStatus.current_battery));
    batteryRemaining()->setRawValue(static_cast<uint8_t>(sysStatus.battery_remaining));
    dropRateComm()->setRawValue(static_cast<uint16_t>(sysStatus.drop_rate_comm));
    errorsComm()->setRawValue(static_cast<uint16_t>(sysStatus.errors_comm));
    errorsCount1()->setRawValue(static_cast<uint32_t>(sysStatus.errors_count1));
    errorsCount2()->setRawValue(static_cast<uint32_t>(sysStatus.errors_count2));
    errorsCount3()->setRawValue(static_cast<uint32_t>(sysStatus.errors_count3));
    errorsCount4()->setRawValue(static_cast<uint32_t>(sysStatus.errors_count4));
    
    // Update individual sensor status
    sensorsPresent3dGyro()->setRawValue((sysStatus.onboard_control_sensors_present & MAV_SYS_STATUS_SENSOR_3D_GYRO) != 0);
    sensorsPresent3dAccel()->setRawValue((sysStatus.onboard_control_sensors_present & MAV_SYS_STATUS_SENSOR_3D_ACCEL) != 0);
    sensorsPresent3dMag()->setRawValue((sysStatus.onboard_control_sensors_present & MAV_SYS_STATUS_SENSOR_3D_MAG) != 0);
    sensorsPresentAbsPressure()->setRawValue((sysStatus.onboard_control_sensors_present & MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE) != 0);
    sensorsPresentDiffPressure()->setRawValue((sysStatus.onboard_control_sensors_present & MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE) != 0);
    sensorsPresentGps()->setRawValue((sysStatus.onboard_control_sensors_present & MAV_SYS_STATUS_SENSOR_GPS) != 0);
    
    sensorsEnabled3dGyro()->setRawValue((sysStatus.onboard_control_sensors_enabled & MAV_SYS_STATUS_SENSOR_3D_GYRO) != 0);
    sensorsEnabled3dAccel()->setRawValue((sysStatus.onboard_control_sensors_enabled & MAV_SYS_STATUS_SENSOR_3D_ACCEL) != 0);
    sensorsEnabled3dMag()->setRawValue((sysStatus.onboard_control_sensors_enabled & MAV_SYS_STATUS_SENSOR_3D_MAG) != 0);
    sensorsEnabledAbsPressure()->setRawValue((sysStatus.onboard_control_sensors_enabled & MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE) != 0);
    sensorsEnabledDiffPressure()->setRawValue((sysStatus.onboard_control_sensors_enabled & MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE) != 0);
    sensorsEnabledGps()->setRawValue((sysStatus.onboard_control_sensors_enabled & MAV_SYS_STATUS_SENSOR_GPS) != 0);
    
    sensorsHealth3dGyro()->setRawValue((sysStatus.onboard_control_sensors_health & MAV_SYS_STATUS_SENSOR_3D_GYRO) != 0);
    sensorsHealth3dAccel()->setRawValue((sysStatus.onboard_control_sensors_health & MAV_SYS_STATUS_SENSOR_3D_ACCEL) != 0);
    sensorsHealth3dMag()->setRawValue((sysStatus.onboard_control_sensors_health & MAV_SYS_STATUS_SENSOR_3D_MAG) != 0);
    sensorsHealthAbsPressure()->setRawValue((sysStatus.onboard_control_sensors_health & MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE) != 0);
    sensorsHealthDiffPressure()->setRawValue((sysStatus.onboard_control_sensors_health & MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE) != 0);
    sensorsHealthGps()->setRawValue((sysStatus.onboard_control_sensors_health & MAV_SYS_STATUS_SENSOR_GPS) != 0);
    
    _setTelemetryAvailable(true);
}
