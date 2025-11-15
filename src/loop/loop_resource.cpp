
#include "loop_resource.h"

#include <utility>

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

looper::error resource_state::verify_not_errored() const {
    if (m_is_errored) {
        return error_resource_errored;
    }

    return error_success;
}

bool resource_state::is_reading() const {
    return m_is_reading;
}

looper::error resource_state::verify_not_reading() const {
    if (m_is_errored) {
        return error_already_reading;
    }

    return error_success;
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

loop_resource::control::control(loop_ptr loop, looper::impl::resource& resource)
    : m_loop(std::move(loop))
    , m_resource(resource)
{}

looper::impl::resource loop_resource::control::handle() const {
    return m_resource;
}

void loop_resource::control::attach_to_loop(const os::descriptor descriptor, const event_type events, handle_events_func&& handle_events) {
    if (m_resource != empty_handle) {
        throw std::runtime_error("already attached as resource");
    }

    auto loop = m_loop;
    m_resource = m_loop->add_resource(descriptor, events, [loop, handle_events](
        resource resource, void*, const event_type events_act)->void {
        if (resource == empty_handle) {
            return;
        }

        auto lock = loop->lock_loop();
        control control(loop, resource);
        handle_events(lock, control, events_act);
    });
}

void loop_resource::control::detach_from_loop() {
    if (m_resource != empty_handle) {
        m_loop->remove_resource(m_resource);
        m_resource = empty_handle;
    }
}

void loop_resource::control::request_events(const event_type events, const events_update_type type) const {
    m_loop->request_resource_events(m_resource, events, type);
}

void loop_resource::control::invoke_in_loop(loop_callback&& callback) const {
    m_loop->invoke_from_loop(std::move(callback));
}

loop_resource::loop_resource(loop_ptr loop)
    : m_loop(std::move(loop))
    , m_resource(empty_handle)
{}

loop_resource::~loop_resource() {
    if (m_loop) {
        auto [lock, control] = lock_loop();
        control.detach_from_loop();
    }
}

loop_resource::loop_resource(loop_resource&& other) noexcept
    : m_loop(std::move(other.m_loop))
    , m_resource(other.m_resource) {
    other.m_loop.reset();
    other.m_resource = empty_handle;
}

loop_resource& loop_resource::operator=(loop_resource&& other) noexcept {
    m_loop = std::move(other.m_loop);
    m_resource = other.m_resource;
    other.m_loop.reset();
    other.m_resource = empty_handle;

    return *this;
}

looper::impl::resource loop_resource::handle() const {
    return m_resource;
}

std::pair<std::unique_lock<std::mutex>, loop_resource::control> loop_resource::lock_loop() {
    auto lock = m_loop->lock_loop();
    return {std::move(lock), control(m_loop, m_resource)};
}

}
