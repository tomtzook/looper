
#include "loop_event.h"

namespace looper::impl {

#define log_module loop_log_module "_event"

event::event(const looper::event handle, loop_context* context, event_callback&& callback)
    : looper_resource(context)
    , m_handle(handle)
    , m_event_obj(os::make_event())
    , m_callback(std::move(callback)) {
    std::unique_lock lock(m_context->mutex);
    attach_to_loop(os::event::get_descriptor(m_event_obj.get()), event_in);
}

void event::set() {
    std::unique_lock lock(m_context->mutex);
    OS_CHECK_THROW(os::event::set(m_event_obj.get()));
}

void event::clear() {
    std::unique_lock lock(m_context->mutex);
    OS_CHECK_THROW(os::event::clear(m_event_obj.get()));
}

void event::handle_events(std::unique_lock<std::mutex>& lock, event_types events) {
    invoke_func(lock, "event_callback", m_callback, m_context->handle, m_handle);
}

}
