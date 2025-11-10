
#include "loop_event.h"

namespace looper::impl {

#define log_module loop_log_module "_event"

event::event(const looper::event handle, const loop_ptr& loop, event_callback&& callback)
    : m_handle(handle)
    , m_event_obj(os::event::create())
    , m_resource(loop)
    , m_callback(std::move(callback)) {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(
        os::get_descriptor(m_event_obj),
        event_in,
        std::bind_front(&event::handle_events, this));
}

void event::set() {
    auto [lock, control] = m_resource.lock_loop();
    OS_CHECK_THROW(os::event_set(m_event_obj));
}

void event::clear() {
    auto [lock, control] = m_resource.lock_loop();
    OS_CHECK_THROW(os::event_clear(m_event_obj));
}

void event::handle_events(std::unique_lock<std::mutex>& lock, const loop_resource::control&, event_types) const {
    invoke_func(lock, "event_callback", m_callback, m_handle);
}

}
