#include "urwt/events/event_publisher.hpp"
#include <sstream>
#include <iomanip>
#include <random>

namespace urwt {
namespace events {

namespace {
    std::string generateEventId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        
        std::stringstream ss;
        ss << "evt_" << std::hex << std::setfill('0') << std::setw(16) << dis(gen);
        return ss.str();
    }
}

EventPublisher::EventPublisher(std::shared_ptr<UrRpc::Client> rpc_client,
                               const std::string& base_topic)
    : rpc_client_(rpc_client)
    , base_topic_(base_topic)
{
    running_ = true;
    publisher_thread_ = std::make_unique<std::thread>(&EventPublisher::publisherLoop, this);
}

EventPublisher::~EventPublisher() {
    running_ = false;
    cv_.notify_all();
    
    if (publisher_thread_ && publisher_thread_->joinable()) {
        publisher_thread_->join();
    }
}

void EventPublisher::publishEvent(const WirelessEvent& event) {
    if (!enabled_) {
        return;
    }
    
    if (buffering_) {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        event_buffer_.push(event);
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        publish_queue_.push(event);
    }
    cv_.notify_one();
}

void EventPublisher::publishConnectionEvent(const std::string& ssid, bool success) {
    json data = {
        {"ssid", ssid},
        {"success", success}
    };
    
    auto type = success ? EventType::ConnectionSucceeded : EventType::ConnectionFailed;
    std::string message = success 
        ? "Successfully connected to " + ssid
        : "Failed to connect to " + ssid;
    
    auto event = createEvent(type, message, data);
    event.severity = success ? "info" : "warning";
    publishEvent(event);
}

void EventPublisher::publishScanEvent(size_t network_count) {
    json data = {
        {"network_count", network_count}
    };
    
    auto event = createEvent(
        EventType::ScanCompleted,
        "Network scan completed, found " + std::to_string(network_count) + " networks",
        data
    );
    publishEvent(event);
}

void EventPublisher::publishModeChangeEvent(config::WirelessMode new_mode, bool success) {
    json data = {
        {"mode", config::wirelessModeToString(new_mode)},
        {"success", success}
    };
    
    auto type = success ? EventType::ModeChangeCompleted : EventType::ModeChangeFailed;
    std::string message = success
        ? "Mode changed to " + config::wirelessModeToString(new_mode)
        : "Failed to change mode to " + config::wirelessModeToString(new_mode);
    
    auto event = createEvent(type, message, data);
    event.severity = success ? "info" : "error";
    publishEvent(event);
}

void EventPublisher::publishStateChangeEvent(const state::SystemWirelessState& state) {
    json data;
    state::to_json(data, state);
    
    auto event = createEvent(
        EventType::StateChanged,
        "Wireless state changed",
        data
    );
    publishEvent(event);
}

void EventPublisher::setBaseTopic(const std::string& topic) {
    base_topic_ = topic;
}

void EventPublisher::setQoS(int qos) {
    qos_ = qos;
}

void EventPublisher::setRetained(bool retained) {
    retained_ = retained;
}

void EventPublisher::enable() {
    enabled_ = true;
}

void EventPublisher::disable() {
    enabled_ = false;
}

bool EventPublisher::isEnabled() const {
    return enabled_;
}

void EventPublisher::startBuffering() {
    buffering_ = true;
}

void EventPublisher::stopBuffering() {
    buffering_ = false;
}

void EventPublisher::flushBuffer() {
    std::lock_guard<std::mutex> buffer_lock(buffer_mutex_);
    
    while (!event_buffer_.empty()) {
        auto event = event_buffer_.front();
        event_buffer_.pop();
        
        {
            std::lock_guard<std::mutex> queue_lock(queue_mutex_);
            publish_queue_.push(event);
        }
    }
    
    cv_.notify_all();
}

size_t EventPublisher::getBufferSize() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return event_buffer_.size();
}

void EventPublisher::publisherLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        cv_.wait(lock, [this] { 
            return !publish_queue_.empty() || !running_; 
        });
        
        if (!running_) {
            break;
        }
        
        while (!publish_queue_.empty()) {
            auto event = publish_queue_.front();
            publish_queue_.pop();
            
            lock.unlock();
            publishInternal(event);
            lock.lock();
        }
    }
}

void EventPublisher::publishInternal(const WirelessEvent& event) {
    if (!rpc_client_ || !enabled_) {
        return;
    }
    
    try {
        std::string topic = buildTopic(event.type);
        json event_json;
        to_json(event_json, event);
        
        std::string payload = event_json.dump();
        
        rpc_client_->publishMessage(topic, payload);
    } catch (const std::exception& e) {
    }
}

std::string EventPublisher::buildTopic(EventType type) const {
    std::string event_category;
    
    switch (type) {
        case EventType::ConnectionStarted:
        case EventType::ConnectionSucceeded:
        case EventType::ConnectionFailed:
        case EventType::Disconnected:
            event_category = "connection";
            break;
            
        case EventType::ScanStarted:
        case EventType::ScanCompleted:
        case EventType::ScanFailed:
        case EventType::NetworkDetected:
        case EventType::NetworkLost:
            event_category = "scan";
            break;
            
        case EventType::ModeChangeStarted:
        case EventType::ModeChangeCompleted:
        case EventType::ModeChangeFailed:
            event_category = "mode";
            break;
            
        case EventType::APStarted:
        case EventType::APStopped:
        case EventType::APClientConnected:
        case EventType::APClientDisconnected:
            event_category = "ap";
            break;
            
        case EventType::StateChanged:
        case EventType::AutomationEnabled:
        case EventType::AutomationDisabled:
            event_category = "state";
            break;
            
        case EventType::ConfigurationUpdated:
        case EventType::NetworkSaved:
        case EventType::NetworkRemoved:
            event_category = "config";
            break;
            
        case EventType::HardwareError:
        case EventType::ConfigurationError:
        case EventType::OperationError:
            event_category = "error";
            break;
            
        default:
            event_category = "unknown";
            break;
    }
    
    return base_topic_ + "/" + event_category;
}

WirelessEvent EventPublisher::createEvent(EventType type, const std::string& message,
                                         const json& data) {
    WirelessEvent event;
    event.type = type;
    event.event_id = generateEventId();
    event.timestamp = std::chrono::system_clock::now();
    event.message = message;
    event.data = data;
    event.severity = "info";
    event.source = "ur-wireless-mann";
    
    return event;
}

} // namespace events
} // namespace urwt
