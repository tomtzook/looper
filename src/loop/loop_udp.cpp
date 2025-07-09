
#include "loop_internal.h"
#include "loop_udp.h"

namespace looper::impl {

#define log_module loop_log_module "_udp"

udp::udp(const looper::udp handle, loop_context *context)
    : looper_resource(context)
    , m_handle(handle)
    , m_socket_obj(os::make_udp())
    , m_read_callback()
    , m_write_requests()
    , m_completed_write_requests()
    , m_is_errored(false)
    , m_reading(false)
    , m_write_pending(false){
    std::unique_lock lock(m_context->mutex);
    attach_to_loop(os::udp::get_descriptor(m_socket_obj.get()), 0);
}

void udp::bind(const uint16_t port) {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    OS_CHECK_THROW(os::udp::bind(m_socket_obj.get(), port));
}

void udp::bind(const std::string_view address, const uint16_t port) {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    OS_CHECK_THROW(os::udp::bind(m_socket_obj.get(), address, port));
}

void udp::start_read(udp_read_callback&& callback) {
    std::unique_lock lock(m_context->mutex);

    if (m_reading) {
        throw std::runtime_error("udp already reading");
    }
    if (m_is_errored) {
        throw std::runtime_error("udp cannot read because it is errored");
    }

    looper_trace_info(log_module, "udp starting read: handle=%lu", m_handle);

    m_read_callback = callback;
    request_events(event_in, events_update_type::append);
    m_reading = true;
}

void udp::stop_read() {
    std::unique_lock lock(m_context->mutex);

    if (!m_reading) {
        return;
    }

    looper_trace_info(log_module, "udp stopping read: handle=%lu", m_handle);

    m_reading = false;
    request_events(event_in, events_update_type::remove);
}

void udp::write(write_request&& request) {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    looper_trace_info(log_module, "udp writing: handle=%lu, buffer_size=%lu", m_handle, request.size);

    m_write_requests.push_back(std::move(request));

    if (!m_write_pending) {
        request_events(event_out, events_update_type::append);
        m_write_pending = true;
    }
}

void udp::close() {
    std::unique_lock lock(m_context->mutex);

    if (m_socket_obj) {
        m_socket_obj.reset();
    }

    detach_from_loop();
}

void udp::handle_events(std::unique_lock<std::mutex>& lock, const event_types events) {
    if ((events & event_in) != 0) {
        // new data
        handle_read(lock);
    }

    if ((events & event_out) != 0) {
        handle_write(lock);
    }
}

void udp::handle_read(std::unique_lock<std::mutex>& lock) {
    if (!m_reading || m_is_errored) {
        request_events(event_in, events_update_type::remove);
        return;
    }

    char ip_buff[64]{};
    uint16_t port;
    std::span<const uint8_t> data{};
    size_t read;
    const auto error = os::udp::read(
        m_socket_obj.get(),
        m_context->read_buffer,
        sizeof(m_context->read_buffer),
        read,
        ip_buff,
        sizeof(ip_buff),
        port);
    if (error == error_success) {
        data = std::span<const uint8_t>{m_context->read_buffer, read};
        looper_trace_debug(log_module, "udp read new data: handle=%lu, data_size=%lu", m_handle, data.size());
    } else {
        m_is_errored = true;
        looper_trace_error(log_module, "udp read error: handle=%lu, code=%lu", m_handle, error);
    }

    invoke_func<>(lock, "udp_loop_callback", m_read_callback,
        m_context->handle, m_handle, inet_address{std::string_view(ip_buff), port}, data, error);
}

void udp::handle_write(std::unique_lock<std::mutex>& lock) {
    if (!m_write_pending || m_is_errored) {
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

void udp::report_write_requests_finished(std::unique_lock<std::mutex>& lock) {
    while (!m_completed_write_requests.empty()) {
        auto& request = m_completed_write_requests.front();
        invoke_func<>(lock, "udp_loop_callback", request.write_callback, m_context->handle, m_handle, request.error);

        m_completed_write_requests.pop_front();
    }
}

bool udp::do_write() {
    // todo: better use of queues
    // todo: use iovec

    static constexpr size_t max_writes_to_do_in_one_iteration = 16;

    // only write up to 16 requests, so as not to starve the loop with writes
    size_t write_count = max_writes_to_do_in_one_iteration;

    while (!m_write_requests.empty() && (write_count--) > 0) {
        auto& request = m_write_requests.front();

        size_t written;
        const auto error = os::udp::write(
            m_socket_obj.get(),
            request.destination.ip,
            request.destination.port,
            request.buffer.get(),
            request.size,
            written);
        if (error == error_success) {
            request.pos += written;
            if (request.pos < request.size) {
                // didn't finish write
                return true;
            }

            looper_trace_debug(log_module, "udp write request finished: handle=%lu", m_handle);
            request.error = error_success;

            m_completed_write_requests.push_back(std::move(request));
            m_write_requests.pop_front();
        } else if (error == error_in_progress || error == error_again) {
            // didn't finish write, but need to try again later
            return true;
        } else {
            looper_trace_error(log_module, "udp write request failed: handle=%lu, code=%lu", m_handle, error);
            request.error = error;

            m_completed_write_requests.push_back(std::move(request));
            m_write_requests.pop_front();

            return false;
        }
    }

    return true;
}

bool udp::is_errored() const {
    return m_is_errored;
}

void udp::mark_errored() {
    m_is_errored = true;
}

void udp::verify_not_errored() const {
    if (m_is_errored) {
        throw std::runtime_error("udp is errored and cannot be used");
    }
}

}
