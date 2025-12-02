#include "VehicleVibrationFactGroup.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>

VehicleVibrationFactGroup::VehicleVibrationFactGroup(bool ignoreCamelCase)
    : FactGroup(100, ignoreCamelCase)
{
    _addFact(std::make_shared<Fact>(0, "vibrationX", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "vibrationY", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "vibrationZ", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "clipping0", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "clipping1", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "clipping2", FactMetaData::valueTypeUint32));
    _addFact(std::make_shared<Fact>(0, "clipping3", FactMetaData::valueTypeUint32));
}

void VehicleVibrationFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_VIBRATION:
            _handleVibration(message);
            break;
    }
}

void VehicleVibrationFactGroup::_handleVibration(const mavlink_message_t &message)
{
    mavlink_vibration_t vibration;
    mavlink_msg_vibration_decode(&message, &vibration);
    
    vibrationX()->setRawValue(static_cast<float>(vibration.vibration_x));
    vibrationY()->setRawValue(static_cast<float>(vibration.vibration_y));
    vibrationZ()->setRawValue(static_cast<float>(vibration.vibration_z));
    clipping0()->setRawValue(static_cast<uint32_t>(vibration.clipping_0));
    clipping1()->setRawValue(static_cast<uint32_t>(vibration.clipping_1));
    clipping2()->setRawValue(static_cast<uint32_t>(vibration.clipping_2));
    // clipping_3 field is not available in this MAVLink version
    // clipping3()->setRawValue(static_cast<uint32_t>(vibration.clipping_3));
    
    _setTelemetryAvailable(true);
}
