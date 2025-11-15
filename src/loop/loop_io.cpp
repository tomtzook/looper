
#include "loop_io.h"

#include <utility>


namespace looper::impl {

io_control::io_control(resource_state& state, loop_resource::control resource_control)
    : state(state)
    , m_resource_control(std::move(resource_control))
{}

void io_control::request_events(const event_type events, const events_update_type type) const noexcept {
    m_resource_control.request_events(events, type);
}

}
