
#include "loop_internal.h"
#include "loop_udp.h"

namespace looper::impl {

#define log_module loop_log_module "_udp"

udp::udp(const looper::udp handle, loop_context *context)
    : m_handle(handle)
    , m_socket_obj(os::make_udp())
    , m_resource(context)
    , m_state()
    , m_write_pending(false)
    , m_read_callback()
    , m_write_requests()
    , m_completed_write_requests() {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(os::udp::get_descriptor(m_socket_obj.get()), 0,
        std::bind_front(&udp::handle_events, this));
}

void udp::bind(const uint16_t port) {
    auto [lock, control] = m_resource.lock_loop();
    m_state.verify_not_errored();

    OS_CHECK_THROW(os::udp::bind(m_socket_obj.get(), port));
}

void udp::bind(const std::string_view address, const uint16_t port) {
    auto [lock, control] = m_resource.lock_loop();
    m_state.verify_not_errored();

    OS_CHECK_THROW(os::udp::bind(m_socket_obj.get(), address, port));
}

void udp::start_read(udp_read_callback&& callback) {
    auto [lock, control] = m_resource.lock_loop();
    m_state.verify_not_errored();
    m_state.verify_not_reading();

    looper_trace_info(log_module, "udp starting read: handle=%lu", m_handle);

    m_read_callback = callback;
    control.request_events(event_in, events_update_type::append);
    m_state.set_reading(true);
}

void udp::stop_read() {
    auto [lock, control] = m_resource.lock_loop();

    if (!m_state.is_reading()) {
        return;
    }

    looper_trace_info(log_module, "udp stopping read: handle=%lu", m_handle);

    m_state.set_reading(false);
    control.request_events(event_in, events_update_type::remove);
}

void udp::write(write_request&& request) {
    auto [lock, control] = m_resource.lock_loop();
    m_state.verify_not_errored();

    looper_trace_info(log_module, "udp writing: handle=%lu, buffer_size=%lu", m_handle, request.size);

    m_write_requests.push_back(std::move(request));

    if (!m_write_pending) {
        control.request_events(event_out, events_update_type::append);
        m_write_pending = true;
    }
}

void udp::close() {
    auto [lock, control] = m_resource.lock_loop();

    if (m_socket_obj) {
        m_socket_obj.reset();
    }

    control.detach_from_loop();
}

void udp::handle_events(std::unique_lock<std::mutex>& lock, looper_resource::control& control, const event_types events) {
    if ((events & event_in) != 0) {
        // new data
        handle_read(lock, control);
    }

    if ((events & event_out) != 0) {
        handle_write(lock, control);
    }
}

void udp::handle_read(std::unique_lock<std::mutex>& lock, looper_resource::control& control) {
    if (!m_state.is_reading() || m_state.is_errored()) {
        control.request_events(event_in, events_update_type::remove);
        return;
    }

    char ip_buff[64]{};
    uint16_t port;
    uint8_t read_buffer[1024]{};
    std::span<const uint8_t> data{};
    size_t read;
    const auto error = os::udp::read(
        m_socket_obj.get(),
        read_buffer,
        sizeof(read_buffer),
        read,
        ip_buff,
        sizeof(ip_buff),
        port);
    if (error == error_success) {
        data = std::span<const uint8_t>{read_buffer, read};
        looper_trace_debug(log_module, "udp read new data: handle=%lu, data_size=%lu", m_handle, data.size());
    } else {
        m_state.mark_errored();
        looper_trace_error(log_module, "udp read error: handle=%lu, code=%lu", m_handle, error);
    }

    invoke_func<>(lock, "udp_loop_callback", m_read_callback,
        m_handle,
        inet_address_view{std::string_view(ip_buff), port}, data, error);
}

void udp::handle_write(std::unique_lock<std::mutex>& lock, looper_resource::control& control) {
    if (!m_write_pending || m_state.is_errored()) {
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

    report_write_requests_finished(lock);
}

void udp::report_write_requests_finished(std::unique_lock<std::mutex>& lock) {
    while (!m_completed_write_requests.empty()) {
        auto& request = m_completed_write_requests.front();
        invoke_func<>(lock, "udp_loop_callback", request.write_callback,
            m_handle, request.error);

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

}
