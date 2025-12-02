#include "VehicleWindFactGroup.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>

VehicleWindFactGroup::VehicleWindFactGroup(bool ignoreCamelCase)
    : FactGroup(1000, ignoreCamelCase)
{
    _addFact(std::make_shared<Fact>(0, "direction", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "speed", FactMetaData::valueTypeFloat));
    _addFact(std::make_shared<Fact>(0, "speedZ", FactMetaData::valueTypeFloat));
}

void VehicleWindFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    // Wind message types are not available in this MAVLink version
    // Just set telemetry as available
    _setTelemetryAvailable(true);
}

// Wind handler methods removed since wind message types are not available in this MAVLink version
