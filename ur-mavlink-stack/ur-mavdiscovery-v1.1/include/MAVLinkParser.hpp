#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <common/mavlink.h>

struct ParsedPacket {
    bool valid;
    uint8_t sysid;
    uint8_t compid;
    uint8_t msgid;
    uint8_t mavlinkVersion;
    std::string messageName;
};

class MAVLinkParser {
public:
    MAVLinkParser();

    ParsedPacket parse(const uint8_t* data, size_t length);
    bool isMagicByte(uint8_t byte);
    std::string getMessageName(uint8_t msgid);

private:
    mavlink_message_t msg_;
    mavlink_status_t status_;
};