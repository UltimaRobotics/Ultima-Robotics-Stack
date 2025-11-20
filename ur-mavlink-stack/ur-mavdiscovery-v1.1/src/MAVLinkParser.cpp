#include "MAVLinkParser.hpp"
#include "Logger.hpp"
#include <cstring>

MAVLinkParser::MAVLinkParser() {
    memset(&msg_, 0, sizeof(msg_));
    memset(&status_, 0, sizeof(status_));
}

ParsedPacket MAVLinkParser::parse(const uint8_t* data, size_t length) {
    ParsedPacket result;
    result.valid = false;

    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];

        if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg_, &status_)) {
            result.valid = true;
            result.sysid = msg_.sysid;
            result.compid = msg_.compid;
            result.msgid = msg_.msgid;
            result.mavlinkVersion = (status_.flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1) ? 1 : 2;
            result.messageName = getMessageName(msg_.msgid);
            return result;
        }
    }

    return result;
}

bool MAVLinkParser::isMagicByte(uint8_t byte) {
    return (byte == MAVLINK_STX || byte == MAVLINK_STX_MAVLINK1);
}

std::string MAVLinkParser::getMessageName(uint8_t msgid) {
    // MAVLink v2 doesn't provide message names in the msg_entry structure
    // Return the message ID as a string instead
    return "MSG_" + std::to_string(msgid);
}