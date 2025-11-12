
#include "loop_io.h"

#include <utility>


namespace looper::impl {

io_control::io_control(resource_state& state, loop_resource::control resource_control)
    : state(state)
    , m_resource_control(std::move(resource_control))
{}

void io_control::request_events(const event_types events, const events_update_type type) const {
    m_resource_control.request_events(events, type);
}

void io_control::invoke_in_loop(loop_callback&& callback) const {
    m_resource_control.invoke_in_loop(std::move(callback));
}

}
