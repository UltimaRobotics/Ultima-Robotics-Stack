#include "VehicleTemperatureFactGroup.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>

VehicleTemperatureFactGroup::VehicleTemperatureFactGroup(bool ignoreCamelCase)
    : FactGroup(1000, ignoreCamelCase)
{
    _addFact(std::make_shared<Fact>(0, "temperature1", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "temperature2", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "temperature3", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "temperatureCalibrated", FactMetaData::valueTypeFloat));
}

void VehicleTemperatureFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_SCALED_PRESSURE:
            _handleScaledPressure(message);
            break;
        case MAVLINK_MSG_ID_SCALED_PRESSURE2:
            _handleScaledPressure2(message);
            break;
        case MAVLINK_MSG_ID_SCALED_PRESSURE3:
            _handleScaledPressure3(message);
            break;
        case MAVLINK_MSG_ID_HIGH_LATENCY2:
            _handleHighLatency2(message);
            break;
    }
}

void VehicleTemperatureFactGroup::_handleScaledPressure(const mavlink_message_t &message)
{
    mavlink_scaled_pressure_t scaledPressure;
    mavlink_msg_scaled_pressure_decode(&message, &scaledPressure);
    
    temperature1()->setRawValue(static_cast<float>(scaledPressure.temperature / 100.0f));
    
    _setTelemetryAvailable(true);
}

void VehicleTemperatureFactGroup::_handleScaledPressure2(const mavlink_message_t &message)
{
    mavlink_scaled_pressure2_t scaledPressure2;
    mavlink_msg_scaled_pressure2_decode(&message, &scaledPressure2);
    
    temperature2()->setRawValue(static_cast<float>(scaledPressure2.temperature / 100.0f));
    
    _setTelemetryAvailable(true);
}

void VehicleTemperatureFactGroup::_handleScaledPressure3(const mavlink_message_t &message)
{
    mavlink_scaled_pressure3_t scaledPressure3;
    mavlink_msg_scaled_pressure3_decode(&message, &scaledPressure3);
    
    temperature3()->setRawValue(static_cast<float>(scaledPressure3.temperature / 100.0f));
    
    _setTelemetryAvailable(true);
}

void VehicleTemperatureFactGroup::_handleHighLatency2(const mavlink_message_t &message)
{
    mavlink_high_latency2_t highLatency2;
    mavlink_msg_high_latency2_decode(&message, &highLatency2);
    
    temperatureCalibrated()->setRawValue(static_cast<float>(highLatency2.temperature_air));
    
    _setTelemetryAvailable(true);
}
