#include "urwt/events/event_types.hpp"
#include <sstream>
#include <iomanip>
#include <random>

namespace urwt {
namespace events {

std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::ConnectionStarted:
            return "ConnectionStarted";
        case EventType::ConnectionSucceeded:
            return "ConnectionSucceeded";
        case EventType::ConnectionFailed:
            return "ConnectionFailed";
        case EventType::Disconnected:
            return "Disconnected";
        
        case EventType::ScanStarted:
            return "ScanStarted";
        case EventType::ScanCompleted:
            return "ScanCompleted";
        case EventType::ScanFailed:
            return "ScanFailed";
        case EventType::NetworkDetected:
            return "NetworkDetected";
        case EventType::NetworkLost:
            return "NetworkLost";
        
        case EventType::ModeChangeStarted:
            return "ModeChangeStarted";
        case EventType::ModeChangeCompleted:
            return "ModeChangeCompleted";
        case EventType::ModeChangeFailed:
            return "ModeChangeFailed";
        
        case EventType::APStarted:
            return "APStarted";
        case EventType::APStopped:
            return "APStopped";
        case EventType::APClientConnected:
            return "APClientConnected";
        case EventType::APClientDisconnected:
            return "APClientDisconnected";
        
        case EventType::StateChanged:
            return "StateChanged";
        case EventType::AutomationEnabled:
            return "AutomationEnabled";
        case EventType::AutomationDisabled:
            return "AutomationDisabled";
        
        case EventType::ConfigurationUpdated:
            return "ConfigurationUpdated";
        case EventType::NetworkSaved:
            return "NetworkSaved";
        case EventType::NetworkRemoved:
            return "NetworkRemoved";
        
        case EventType::HardwareError:
            return "HardwareError";
        case EventType::ConfigurationError:
            return "ConfigurationError";
        case EventType::OperationError:
            return "OperationError";
        
        default:
            return "Unknown";
    }
}

EventType stringToEventType(const std::string& str) {
    if (str == "ConnectionStarted") return EventType::ConnectionStarted;
    if (str == "ConnectionSucceeded") return EventType::ConnectionSucceeded;
    if (str == "ConnectionFailed") return EventType::ConnectionFailed;
    if (str == "Disconnected") return EventType::Disconnected;
    
    if (str == "ScanStarted") return EventType::ScanStarted;
    if (str == "ScanCompleted") return EventType::ScanCompleted;
    if (str == "ScanFailed") return EventType::ScanFailed;
    if (str == "NetworkDetected") return EventType::NetworkDetected;
    if (str == "NetworkLost") return EventType::NetworkLost;
    
    if (str == "ModeChangeStarted") return EventType::ModeChangeStarted;
    if (str == "ModeChangeCompleted") return EventType::ModeChangeCompleted;
    if (str == "ModeChangeFailed") return EventType::ModeChangeFailed;
    
    if (str == "APStarted") return EventType::APStarted;
    if (str == "APStopped") return EventType::APStopped;
    if (str == "APClientConnected") return EventType::APClientConnected;
    if (str == "APClientDisconnected") return EventType::APClientDisconnected;
    
    if (str == "StateChanged") return EventType::StateChanged;
    if (str == "AutomationEnabled") return EventType::AutomationEnabled;
    if (str == "AutomationDisabled") return EventType::AutomationDisabled;
    
    if (str == "ConfigurationUpdated") return EventType::ConfigurationUpdated;
    if (str == "NetworkSaved") return EventType::NetworkSaved;
    if (str == "NetworkRemoved") return EventType::NetworkRemoved;
    
    if (str == "HardwareError") return EventType::HardwareError;
    if (str == "ConfigurationError") return EventType::ConfigurationError;
    if (str == "OperationError") return EventType::OperationError;
    
    return EventType::OperationError;
}

void to_json(nlohmann::json& j, const WirelessEvent& event) {
    auto time_t = std::chrono::system_clock::to_time_t(event.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        event.timestamp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    j = json{
        {"type", eventTypeToString(event.type)},
        {"event_id", event.event_id},
        {"timestamp", ss.str()},
        {"message", event.message},
        {"data", event.data},
        {"severity", event.severity},
        {"source", event.source}
    };
}

void from_json(const nlohmann::json& j, WirelessEvent& event) {
    event.type = stringToEventType(j.at("type").get<std::string>());
    event.event_id = j.at("event_id").get<std::string>();
    event.message = j.at("message").get<std::string>();
    event.data = j.at("data");
    event.severity = j.value("severity", "info");
    event.source = j.value("source", "ur-wireless-mann");
    
    if (j.contains("timestamp")) {
        std::string timestamp_str = j.at("timestamp").get<std::string>();
        std::tm tm = {};
        std::istringstream ss(timestamp_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        event.timestamp = time_point;
    } else {
        event.timestamp = std::chrono::system_clock::now();
    }
}

} // namespace events
} // namespace urwt
