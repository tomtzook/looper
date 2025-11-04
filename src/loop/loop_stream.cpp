
#include "looper_trace.h"
#include "loop_internal.h"
#include "loop_stream.h"

namespace looper::impl {

#define log_module loop_log_module "_stream"

stream::control::control(resource_state& state, const looper_resource::control& resource_control)
    : state(state)
    , m_resource_control(resource_control)
{}

void stream::control::request_events(const event_types events, const events_update_type type) {
    m_resource_control.request_events(events, type);
}

stream::stream(const looper::handle handle, loop_context* context,
    read_from_obj&& read_from_obj, write_to_obj&& write_to_obj,
    const os::descriptor os_descriptor, handle_events_ext_func&& handle_events_ext)
    : m_handle(handle)
    , m_resource(context)
    , m_state()
    , m_read_from_obj(read_from_obj)
    , m_write_to_obj(write_to_obj)
    , m_handle_events_ext_func(handle_events_ext)
    , m_user_read_callback(nullptr)
    , m_write_requests()
    , m_completed_write_requests()
    , m_reading(false)
    , m_write_pending(false) {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(os_descriptor, 0, std::bind_front(&stream::handle_events, this));
}

looper::loop stream::loop_handle() const {
    return m_resource.loop_handle();
}

looper::handle stream::handle() const {
    return m_handle;
}

std::pair<std::unique_lock<std::mutex>, stream::control> stream::use() {
    auto [lock, res_control] = m_resource.lock_loop();
    return {std::move(lock), control(m_state, res_control)};
}

void stream::start_read(read_callback&& callback) {
    auto [lock, control] = use();
    control.state.verify_not_errored();
    control.state.verify_not_reading();

    if (!m_state.can_read()) {
        throw std::runtime_error("stream cannot read at the current state");
    }

    looper_trace_info(log_module, "stream starting read: handle=%lu", m_handle);

    m_user_read_callback = callback;
    control.request_events(event_in, events_update_type::append);
    m_reading = true;
}

void stream::stop_read() {
    auto [lock, control] = use();

    if (!m_reading) {
        return;
    }

    looper_trace_info(log_module, "stream stopping read: handle=%lu", m_handle);

    m_reading = false;
    control.request_events(event_in, events_update_type::remove);
}

void stream::write(write_request&& request) {
    auto [lock, control] = use();
    control.state.verify_not_errored();

    if (!m_state.can_write()) {
        throw std::runtime_error("stream cannot write at the current state");
    }

    looper_trace_info(log_module, "stream writing: handle=%lu, buffer_size=%lu", m_handle, request.size);

    m_write_requests.push_back(std::move(request));

    if (!m_write_pending) {
        control.request_events(event_out, events_update_type::append);
        m_write_pending = true;
    }
}

void stream::handle_events(std::unique_lock<std::mutex>& lock, looper_resource::control& control, const event_types events) {
    struct control our_control(m_state, control);
    if (m_handle_events_ext_func(lock, our_control, events)) {
        return;
    }

    if ((events & event_in) != 0) {
        // new data
        handle_read(lock, control);
    }

    if ((events & event_out) != 0) {
        handle_write(lock, control);
    }
}

void stream::handle_read(std::unique_lock<std::mutex>& lock, looper_resource::control& control) {
    if (!m_state.is_reading() || m_state.is_errored() || !m_state.can_read()) {
        control.request_events(event_in, events_update_type::remove);
        return;
    }

    uint8_t read_buffer[1024]{};
    std::span<const uint8_t> data{};
    size_t read;
    const auto error = m_read_from_obj(std::span<uint8_t>{read_buffer, sizeof(read_buffer)}, read);
    if (error == error_success) {
        data = std::span<const uint8_t>{read_buffer, read};
        looper_trace_debug(log_module, "stream read new data: handle=%lu, data_size=%lu", m_handle, data.size());
    } else {
        m_state.mark_errored();
        looper_trace_error(log_module, "stream read error: handle=%lu, code=%lu", m_handle, error);
    }

    // todo: call this via context?
    invoke_func<>(lock, "stream_loop_callback", m_user_read_callback, control.loop_handle(), m_handle, data, error);
}

void stream::handle_write(std::unique_lock<std::mutex>& lock, looper_resource::control& control) {
    if (!m_write_pending || m_state.is_errored() || !m_state.can_write()) {
        control.request_events(event_out, events_update_type::remove);
        return;
    }

    if (!do_write()) {
        m_state.mark_errored();
        m_write_pending = false;
        control.request_events(event_out, events_update_type::remove);
    } else {
        if (m_write_requests.empty()) {
            m_write_pending = false;
            control.request_events(event_out, events_update_type::remove);
        }
    }

    report_write_requests_finished(lock, control);
}

void stream::report_write_requests_finished(
    std::unique_lock<std::mutex>& lock,
    const looper_resource::control& control) {
    while (!m_completed_write_requests.empty()) {
        auto& request = m_completed_write_requests.front();
        invoke_func<>(lock, "stream_loop_callback", request.write_callback, control.loop_handle(), m_handle, request.error);

        m_completed_write_requests.pop_front();
    }
}

bool stream::do_write() {
    // todo: better use of queues
    // todo: use iovec

    static constexpr size_t max_writes_to_do_in_one_iteration = 16;

    // only write up to 16 requests, so as not to starve the loop with writes
    size_t write_count = max_writes_to_do_in_one_iteration;

    while (!m_write_requests.empty() && (write_count--) > 0) {
        auto& request = m_write_requests.front();

        size_t written;
        const auto error = m_write_to_obj(std::span<const uint8_t>{request.buffer.get() + request.pos, request.size}, written);
        if (error == error_success) {
            request.pos += written;
            if (request.pos < request.size) {
                // didn't finish write
                return true;
            }

            looper_trace_debug(log_module, "stream write request finished: handle=%lu", m_handle);
            request.error = error_success;

            m_completed_write_requests.push_back(std::move(request));
            m_write_requests.pop_front();
        } else if (error == error_in_progress || error == error_again) {
            // didn't finish write, but need to try again later
            return true;
        } else {
            looper_trace_error(log_module, "stream write request failed: handle=%lu, code=%lu", m_handle, error);
            request.error = error;

            m_completed_write_requests.push_back(std::move(request));
            m_write_requests.pop_front();

            return false;
        }
    }

    return true;
}

}
