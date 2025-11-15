
#include "loop_event.h"

namespace looper::impl {

#define log_module loop_log_module "_event"

event::event(const looper::event handle, const loop_ptr& loop, os::event&& event, event_callback&& callback) noexcept
    : m_handle(handle)
    , m_event_obj(std::move(event))
    , m_resource(loop)
    , m_callback(std::move(callback)) {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(
        os::get_descriptor(m_event_obj),
        event_type::in,
        std::bind_front(&event::handle_events, this));
}

looper::error event::set() noexcept {
    auto [lock, control] = m_resource.lock_loop();
    return os::event_set(m_event_obj);
}

looper::error event::clear() noexcept {
    auto [lock, control] = m_resource.lock_loop();
    return os::event_clear(m_event_obj);
}

void event::handle_events(std::unique_lock<std::mutex>& lock, const loop_resource::control&, event_type) const noexcept {
    invoke_func(lock, "event_callback", m_callback, m_handle);
}

}
