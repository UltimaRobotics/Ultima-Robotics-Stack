#ifndef URWT_EVENTS_EVENT_LISTENER_HPP
#define URWT_EVENTS_EVENT_LISTENER_HPP

#include "urwt/events/event_types.hpp"

namespace urwt {
namespace events {

class IEventListener {
public:
    virtual ~IEventListener() = default;
    
    virtual void onEvent(const WirelessEvent& event) = 0;
};

} // namespace events
} // namespace urwt

#endif // URWT_EVENTS_EVENT_LISTENER_HPP
