#pragma once

#include "FactGroup.h"

/// RC FactGroup containing RC telemetry data.
/// This is a Qt-free port of QGroundControl's RC telemetry.
class VehicleRCFactGroup : public FactGroup
{
public:
    explicit VehicleRCFactGroup(bool ignoreCamelCase = false);
    virtual ~VehicleRCFactGroup() = default;

    // Fact accessors
    std::shared_ptr<Fact> channelRaw() { return getFact("channelRaw"); }
    std::shared_ptr<Fact> channelCount() { return getFact("channelCount"); }
    std::shared_ptr<Fact> rssi() { return getFact("rssi"); }
    std::shared_ptr<Fact> rcRSSI() { return getFact("rcRSSI"); }
    std::shared_ptr<Fact> rcReceivedPacketCount() { return getFact("rcReceivedPacketCount"); }
    std::shared_ptr<Fact> rcLostPacketCount() { return getFact("rcLostPacketCount"); }
    std::shared_ptr<Fact> rcPPMFrameCount() { return getFact("rcPPMFrameCount"); }
    std::shared_ptr<Fact> rcOVERRUN() { return getFact("rcOVERRUN"); }
    std::shared_ptr<Fact> rcFCS() { return getFact("rcFCS"); }
    std::shared_ptr<Fact> rcRSSIDB() { return getFact("rcRSSIDB"); }
    std::shared_ptr<Fact> rcRSSIRegen() { return getFact("rcRSSIRegen"); }

    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) override;

protected:
    void _handleRCChannelsRaw(const mavlink_message_t &message);
    void _handleRCChannels(const mavlink_message_t &message);
    void _handleRadioStatus(const mavlink_message_t &message);
};
