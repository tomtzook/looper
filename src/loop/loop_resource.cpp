
#include "loop_resource.h"

namespace looper::impl {

resource_state::resource_state()
    : m_is_errored(false)
    , m_is_reading(false)
    , m_can_read(false)
    , m_can_write(false)
{}

bool resource_state::is_errored() const {
    return m_is_errored;
}

void resource_state::verify_not_errored() const {
    if (m_is_errored) {
        throw std::runtime_error("resource is errored and cannot be used");
    }
}

bool resource_state::is_reading() const {
    return m_is_reading;
}

void resource_state::verify_not_reading() const {
    if (m_is_errored) {
        throw std::runtime_error("resource is reading");
    }
}

bool resource_state::can_read() const {
    return m_can_read;
}

bool resource_state::can_write() const {
    return m_can_write;
}

void resource_state::mark_errored() {
    m_is_errored = true;
}

void resource_state::set_reading(const bool reading) {
    m_is_reading = reading;
}

void resource_state::set_read_enabled(const bool enabled) {
    m_can_read = enabled;
}

void resource_state::set_write_enabled(const bool enabled) {
    m_can_write = enabled;
}

looper_resource::control::control(loop_context* context, looper::impl::resource& resource)
    : m_context(context)
    , m_resource(resource)
{}

looper::loop looper_resource::control::loop_handle() const {
    return m_context->handle;
}

looper::impl::resource looper_resource::control::handle() const {
    return m_resource;
}

void looper_resource::control::attach_to_loop(const os::descriptor descriptor, const event_types events, handle_events_func&& handle_events) {
    if (m_resource != empty_handle) {
        throw std::runtime_error("already attached as resource");
    }

    m_resource = add_resource(m_context, descriptor, events, [handle_events](
        loop_context* context, resource resource, void*, const event_types events_act)->void {
        if (resource == empty_handle) {
            return;
        }

        std::unique_lock lock(context->mutex);
        control control(context, resource);
        handle_events(lock, control, events_act);
    });
}

void looper_resource::control::detach_from_loop() {
    if (m_resource != empty_handle) {
        remove_resource(m_context, m_resource);
        m_resource = empty_handle;
    }
}

void looper_resource::control::request_events(const event_types events, const events_update_type type) const {
    request_resource_events(m_context, m_resource, events, type);
}

looper_resource::looper_resource(loop_context* context)
    : m_context(context)
    , m_resource(empty_handle)
{}

looper_resource::~looper_resource() {
    auto [lock, control] = lock_loop();
    control.detach_from_loop();
}

looper::loop looper_resource::loop_handle() const {
    return m_context->handle;
}

looper::impl::resource looper_resource::handle() const {
    return m_resource;
}

std::pair<std::unique_lock<std::mutex>, looper_resource::control> looper_resource::lock_loop() {
    return {std::unique_lock(m_context->mutex), control(m_context, m_resource)};
}

}
