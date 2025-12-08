#ifndef URWT_EVENTS_EVENT_PUBLISHER_HPP
#define URWT_EVENTS_EVENT_PUBLISHER_HPP

#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "urwt/events/event_types.hpp"
#include "ur-rpc-template.hpp"

namespace urwt {
namespace events {

class EventPublisher {
public:
    EventPublisher(std::shared_ptr<UrRpc::Client> rpc_client,
                  const std::string& base_topic);
    ~EventPublisher();

    void publishEvent(const WirelessEvent& event);
    void publishConnectionEvent(const std::string& ssid, bool success);
    void publishScanEvent(size_t network_count);
    void publishModeChangeEvent(config::WirelessMode new_mode, bool success);
    void publishStateChangeEvent(const state::SystemWirelessState& state);

    void setBaseTopic(const std::string& topic);
    void setQoS(int qos);
    void setRetained(bool retained);

    void enable();
    void disable();
    bool isEnabled() const;

    void startBuffering();
    void stopBuffering();
    void flushBuffer();
    size_t getBufferSize() const;

private:
    std::shared_ptr<UrRpc::Client> rpc_client_;
    std::string base_topic_;
    int qos_{1};
    bool retained_{false};
    std::atomic<bool> enabled_{true};

    std::atomic<bool> buffering_{false};
    mutable std::mutex buffer_mutex_;
    std::queue<WirelessEvent> event_buffer_;

    std::unique_ptr<std::thread> publisher_thread_;
    std::atomic<bool> running_{false};
    std::condition_variable cv_;
    std::queue<WirelessEvent> publish_queue_;
    mutable std::mutex queue_mutex_;

    void publisherLoop();
    void publishInternal(const WirelessEvent& event);
    std::string buildTopic(EventType type) const;
    WirelessEvent createEvent(EventType type, const std::string& message,
                             const json& data = json::object());
};

} // namespace events
} // namespace urwt

#endif // URWT_EVENTS_EVENT_PUBLISHER_HPP
