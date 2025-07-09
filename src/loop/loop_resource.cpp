
#include "loop_resource.h"

namespace looper::impl {

looper_resource::looper_resource(loop_context* context)
    : m_context(context)
    , m_resource(empty_handle)
{}

looper_resource::~looper_resource() {
    detach_from_loop();
}

void looper_resource::attach_to_loop(const os::descriptor descriptor, const event_types events) {
    if (m_resource != empty_handle) {
        throw std::runtime_error("already attached as resource");
    }

    m_resource = add_resource(m_context, descriptor, events, [this](loop_context*, void*, event_types events)->void {
        std::unique_lock lock(m_context->mutex);
        if (m_resource == empty_handle) {
            return;
        }

        handle_events(lock, events);
    });
}

void looper_resource::detach_from_loop() {
    if (m_resource != empty_handle) {
        remove_resource(m_context, m_resource);
        m_resource = empty_handle;
    }
}

void looper_resource::request_events(const event_types events, const events_update_type type) {
    request_resource_events(m_context, m_resource, events, type);
}

}
