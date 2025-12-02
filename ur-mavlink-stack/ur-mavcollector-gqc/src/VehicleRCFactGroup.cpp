#include "VehicleRCFactGroup.h"
#include "Vehicle.h"
#include <mavlink/v2.0/common/mavlink.h>

VehicleRCFactGroup::VehicleRCFactGroup(bool ignoreCamelCase)
    : FactGroup(100, ignoreCamelCase) // Update every 100ms
{
    _addFact(std::make_shared<Fact>(0, "channelRaw", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "channelCount", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "rssi", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "rcRSSI", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "rcReceivedPacketCount", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "rcLostPacketCount", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "rcPPMFrameCount", FactMetaData::valueTypeUint16));
    _addFact(std::make_shared<Fact>(0, "rcOVERRUN", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "rcFCS", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "rcRSSIDB", FactMetaData::valueTypeUint8));
    _addFact(std::make_shared<Fact>(0, "rcRSSIRegen", FactMetaData::valueTypeUint8));
}

void VehicleRCFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    switch (message.msgid) {
        case MAVLINK_MSG_ID_RC_CHANNELS_RAW:
            _handleRCChannelsRaw(message);
            break;
        case MAVLINK_MSG_ID_RC_CHANNELS:
            _handleRCChannels(message);
            break;
        case MAVLINK_MSG_ID_RADIO_STATUS:
            _handleRadioStatus(message);
            break;
    }
}

void VehicleRCFactGroup::_handleRCChannelsRaw(const mavlink_message_t &message)
{
    mavlink_rc_channels_raw_t rcChannels;
    mavlink_msg_rc_channels_raw_decode(&message, &rcChannels);
    
    channelCount()->setRawValue(static_cast<uint8_t>(8));
    rssi()->setRawValue(static_cast<uint8_t>(rcChannels.rssi));
    
    _setTelemetryAvailable(true);
}

void VehicleRCFactGroup::_handleRCChannels(const mavlink_message_t &message)
{
    mavlink_rc_channels_t rcChannels;
    mavlink_msg_rc_channels_decode(&message, &rcChannels);
    
    channelCount()->setRawValue(static_cast<uint8_t>(rcChannels.chancount));
    rssi()->setRawValue(static_cast<uint8_t>(rcChannels.rssi));
    
    _setTelemetryAvailable(true);
}

void VehicleRCFactGroup::_handleRadioStatus(const mavlink_message_t &message)
{
    mavlink_radio_status_t radioStatus;
    mavlink_msg_radio_status_decode(&message, &radioStatus);
    
    rcRSSI()->setRawValue(static_cast<uint8_t>(radioStatus.rssi));
    rcReceivedPacketCount()->setRawValue(static_cast<uint16_t>(radioStatus.rxerrors));
    rcLostPacketCount()->setRawValue(static_cast<uint16_t>(radioStatus.fixed));
    rcRSSIDB()->setRawValue(static_cast<uint8_t>(radioStatus.remrssi));
    rcRSSIRegen()->setRawValue(static_cast<uint8_t>(radioStatus.txbuf));
    
    _setTelemetryAvailable(true);
}
