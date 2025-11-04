
#include "loop_event.h"

namespace looper::impl {

#define log_module loop_log_module "_event"

event::event(const looper::event handle, loop_context* context, event_callback&& callback)
    : m_handle(handle)
    , m_event_obj(os::make_event())
    , m_resource(context)
    , m_callback(std::move(callback)) {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(os::event::get_descriptor(m_event_obj.get()), event_in,
        std::bind_front(&event::handle_events, this));
}

void event::set() {
    auto [lock, control] = m_resource.lock_loop();
    OS_CHECK_THROW(os::event::set(m_event_obj.get()));
}

void event::clear() {
    auto [lock, control] = m_resource.lock_loop();
    OS_CHECK_THROW(os::event::clear(m_event_obj.get()));
}

void event::handle_events(std::unique_lock<std::mutex>& lock, const looper_resource::control& control, event_types) const {
    invoke_func(lock, "event_callback", m_callback, control.loop_handle(), m_handle);
}

}
