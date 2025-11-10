
#include "loop_tcp.h"

namespace looper::impl {

#define log_module loop_log_module "_tcp"

tcp::tcp(const looper::tcp handle, const loop_ptr& loop)
    : tcp(handle, loop, os::tcp::create(), state::open)
{}

tcp::tcp(const looper::tcp handle, const loop_ptr& loop, os::tcp&& socket)
    : tcp(handle, loop, std::move(socket), state::connected)
{}

tcp::tcp(const looper::tcp handle, const loop_ptr& loop, os::tcp&& socket, const state state)
    : m_state(state)
    , m_stream(handle, loop, std::move(socket))
    , m_connect_callback() {
    auto [lock, control] = m_stream.use();

    m_stream.set_custom_event_handler(std::bind_front(&tcp::handle_events, this));

    if (m_state == state::connected) {
        control.state.set_read_enabled(true);
        control.state.set_write_enabled(true);
    }
}

void tcp::bind(const uint16_t port) {
    auto [lock, control] = m_stream.use();
    control.state.verify_not_errored();

    OS_CHECK_THROW(os::ipv4_bind(m_stream.io_obj().m_obj, port));
}

void tcp::bind(const std::string_view address, const uint16_t port) {
    auto [lock, control] = m_stream.use();
    control.state.verify_not_errored();

    OS_CHECK_THROW(os::ipv4_bind(m_stream.io_obj().m_obj, address, port));
}

void tcp::connect(const std::string_view address, const uint16_t port, tcp_callback&& callback) {
    auto [lock, control] = m_stream.use();
    control.state.verify_not_errored();

    if (m_state != state::open) {
        throw std::runtime_error("tcp state invalid for connect");
    }

    looper_trace_info(log_module, "connecting tcp: handle=%lu", m_stream.handle());

    m_connect_callback = callback;
    m_state = state::connecting;
    const auto error = os::ipv4_connect(m_stream.io_obj().m_obj, address, port);
    if (error == error_success) {
        // connection finished
        on_connect_done(lock, control);
    } else if (error == error_in_progress) {
        // wait for connection finish
        control.request_events(event_out, events_update_type::append);
        looper_trace_info(log_module, "tcp connection not finished: handle=%lu", m_stream.handle());
    } else {
        on_connect_done(lock, control, error);
    }
}

void tcp::start_read(looper::read_callback&& callback) {
    m_stream.start_read_stream(std::move(callback));
}

void tcp::stop_read() {
    m_stream.stop_read();
}

void tcp::write(stream_write_request&& request) {
    m_stream.write(std::move(request));
}

void tcp::close() {
    m_stream.close();
}

bool tcp::handle_events(std::unique_lock<std::mutex>& lock, const io_control& control, const event_types events) {
    switch (m_state) {
        case state::connecting: {
            if ((events & event_out) != 0) {
                handle_connect(lock, control);
            }

            return true;
        }
        case state::connected: {
            return false;
        }
        default:
            return false;
    }
}

void tcp::handle_connect(std::unique_lock<std::mutex>& lock, const io_control& control) {
    // finish connect
    const auto error = os::ipv4_finalize_connect(m_stream.io_obj().m_obj);
    control.request_events(event_out, events_update_type::remove);
    on_connect_done(lock, control, error);
}

void tcp::on_connect_done(std::unique_lock<std::mutex>& lock, const io_control& control, const error error) {
    if (error == error_success) {
        // connection finished
        m_state = state::connected;
        looper_trace_info(log_module, "connected tcp: handle=%lu", m_stream.handle());

        control.state.set_read_enabled(true);
        control.state.set_write_enabled(true);

        invoke_func<>(lock, "tcp_loop_callback", m_connect_callback, m_stream.handle(), error);
    } else {
        m_state = state::open;
        control.state.mark_errored();
        looper_trace_error(log_module, "tcp connection failed: handle=%lu, code=0x%x", m_stream.handle(), error);
        invoke_func<>(lock, "tcp_loop_callback", m_connect_callback, m_stream.handle(), error);
    }
}

tcp_server::tcp_server(const looper::tcp_server handle, const loop_ptr& loop)
    : m_handle(handle)
    , m_loop(loop)
    , m_resource(loop)
    , m_socket_obj(os::tcp::create())
    , m_callback(nullptr) {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(
        os::get_descriptor(m_socket_obj),
        0,
        std::bind_front(&tcp_server::handle_events, this));
}

void tcp_server::bind(const uint16_t port) {
    auto [lock, control] = m_resource.lock_loop();
    OS_CHECK_THROW(os::ipv4_bind(m_socket_obj, port));
}

void tcp_server::bind(const std::string_view address, const uint16_t port) {
    auto [lock, control] = m_resource.lock_loop();
    OS_CHECK_THROW(os::ipv4_bind(m_socket_obj, address, port));
}

void tcp_server::listen(const size_t backlog, tcp_server_callback&& callback) {
    auto [lock, control] = m_resource.lock_loop();

    OS_CHECK_THROW(os::ipv4_listen(m_socket_obj, backlog));
    m_callback = callback;

    control.request_events(event_in, events_update_type::append);
}

std::unique_ptr<tcp> tcp_server::accept(looper::handle handle) {
    auto [lock, control] = m_resource.lock_loop();

    auto [error, new_tcp] = os::ipv4_accept(m_socket_obj);
    OS_CHECK_THROW(error);

    lock.unlock();
    return std::make_unique<tcp>(handle, m_loop, std::move(new_tcp));
}

void tcp_server::close() {
    auto [lock, control] = m_resource.lock_loop();

    m_socket_obj.close();
    control.detach_from_loop();
}

void tcp_server::handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control& control, const event_types events) const {
    if ((events & event_in) != 0) {
        // new data
        invoke_func(lock, "tcp_server_callback", m_callback, m_handle);
    }
}

}
