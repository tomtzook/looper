
#include "loop_internal.h"
#include "loop_stream.h"

namespace looper::impl {

#define log_module loop_log_module "_stream"

stream::stream(const handle handle, loop_context* context)
    : looper_resource(context)
    , m_handle(handle)
    , m_is_errored(false)
    , m_can_read(false)
    , m_can_write(false)
    , m_user_read_callback(nullptr)
    , m_write_requests()
    , m_completed_write_requests()
    , m_reading(false)
    , m_write_pending(false)
{}

void stream::start_read(read_callback&& callback) {
    std::unique_lock lock(m_context->mutex);

    if (!m_can_read) {
        throw std::runtime_error("stream cannot read at the current state");
    }
    if (m_reading) {
        throw std::runtime_error("stream already reading");
    }
    if (m_is_errored) {
        throw std::runtime_error("stream cannot read because it is errored");
    }

    looper_trace_info(log_module, "stream starting read: handle=%lu", m_handle);

    m_user_read_callback = callback;
    request_events(event_in, events_update_type::append);
    m_reading = true;
}

void stream::stop_read() {
    std::unique_lock lock(m_context->mutex);

    if (!m_reading) {
        return;
    }

    looper_trace_info(log_module, "stream stopping read: handle=%lu", m_handle);

    m_reading = false;
    request_events(event_in, events_update_type::remove);
}

void stream::write(write_request&& request) {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    if (!m_can_write) {
        throw std::runtime_error("stream cannot write at the current state");
    }

    looper_trace_info(log_module, "stream writing: handle=%lu, buffer_size=%lu", m_handle, request.size);

    m_write_requests.push_back(std::move(request));

    if (!m_write_pending) {
        request_events(event_out, events_update_type::append);
        m_write_pending = true;
    }
}

void stream::handle_events(std::unique_lock<std::mutex>& lock, const event_types events) {
    if ((events & event_in) != 0) {
        // new data
        handle_read(lock);
    }

    if ((events & event_out) != 0) {
        handle_write(lock);
    }
}

void stream::set_read_enabled(const bool enabled) {
    m_can_read = enabled;
}

void stream::set_write_enabled(const bool enabled) {
    m_can_write = enabled;
}

bool stream::is_errored() const {
    return m_is_errored;
}

void stream::mark_errored() {
    m_is_errored = true;
}

void stream::verify_not_errored() const {
    if (m_is_errored) {
        throw std::runtime_error("stream is errored and cannot be used");
    }
}

void stream::handle_read(std::unique_lock<std::mutex>& lock) {
    if (!m_reading || m_is_errored || !m_can_read) {
        request_events(event_in, events_update_type::remove);
        return;
    }

    std::span<const uint8_t> data{};
    size_t read;
    const auto error = read_from_obj(m_context->read_buffer, sizeof(m_context->read_buffer), read);
    if (error == error_success) {
        data = std::span<const uint8_t>{m_context->read_buffer, read};
        looper_trace_debug(log_module, "stream read new data: handle=%lu, data_size=%lu", m_handle, data.size());
    } else {
        m_is_errored = true;
        looper_trace_error(log_module, "stream read error: handle=%lu, code=%lu", m_handle, error);
    }

    invoke_func<>(lock, "stream_loop_callback", m_user_read_callback, m_context->handle, m_handle, data, error);
}

void stream::handle_write(std::unique_lock<std::mutex>& lock) {
    if (!m_write_pending || m_is_errored || !m_can_write) {
        request_events(event_out, events_update_type::remove);
        return;
    }

    if (!do_write()) {
        m_is_errored = true;
        m_write_pending = false;
        request_events(event_out, events_update_type::remove);
    } else {
        if (m_write_requests.empty()) {
            m_write_pending = false;
            request_events(event_out, events_update_type::remove);
        }
    }

    report_write_requests_finished(lock);
}

void stream::report_write_requests_finished(std::unique_lock<std::mutex>& lock) {
    while (!m_completed_write_requests.empty()) {
        auto& request = m_completed_write_requests.front();
        invoke_func<>(lock, "stream_loop_callback", request.write_callback, m_context->handle, m_handle, request.error);

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
        const auto error = write_to_obj(request.buffer.get() + request.pos, request.size, written);
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
