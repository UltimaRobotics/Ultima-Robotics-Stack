#ifndef URWT_EVENTS_EVENT_TYPES_HPP
#define URWT_EVENTS_EVENT_TYPES_HPP

#include <string>
#include <chrono>
#include <json.hpp>
#include "urwt/state/wireless_state.hpp"
#include "urwt/config/wireless_config_types.hpp"

namespace urwt {
namespace events {

using json = nlohmann::json;

enum class EventType {
    ConnectionStarted,
    ConnectionSucceeded,
    ConnectionFailed,
    Disconnected,

    ScanStarted,
    ScanCompleted,
    ScanFailed,
    NetworkDetected,
    NetworkLost,

    ModeChangeStarted,
    ModeChangeCompleted,
    ModeChangeFailed,

    APStarted,
    APStopped,
    APClientConnected,
    APClientDisconnected,

    StateChanged,
    AutomationEnabled,
    AutomationDisabled,

    ConfigurationUpdated,
    NetworkSaved,
    NetworkRemoved,

    HardwareError,
    ConfigurationError,
    OperationError
};

struct WirelessEvent {
    EventType type;
    std::string event_id;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    json data;

    std::string severity{"info"};
    std::string source{"ur-wireless-mann"};
};

void to_json(nlohmann::json& j, const WirelessEvent& event);
void from_json(const nlohmann::json& j, WirelessEvent& event);
std::string eventTypeToString(EventType type);

} // namespace events
} // namespace urwt

#endif // URWT_EVENTS_EVENT_TYPES_HPP
