
#include "looper_trace.h"
#include "loop.h"
#include "loop_tcp.h"

#include "loop_stream.h"

namespace looper::impl {

#define log_module loop_log_module "_tcp"

tcp::tcp(const looper::tcp handle, const loop_ptr& loop)
    : tcp(handle, loop, os::make_tcp(), state::open) {
}

tcp::tcp(const looper::tcp handle, const loop_ptr& loop, os::tcp_ptr&& socket)
    : tcp(handle, loop, std::move(socket), state::connected) {
    auto [lock, stream_control] = m_stream.use();
    stream_control.state.set_read_enabled(true);
    stream_control.state.set_write_enabled(true);
}

tcp::tcp(const looper::tcp handle, const loop_ptr& loop, os::tcp_ptr&& socket, const state state)
    : m_socket_obj(std::move(socket))
    , m_stream(handle, loop,
        std::bind_front(&tcp::read_from_obj, this), std::bind_front(&tcp::write_to_obj, this),
        os::tcp::get_descriptor(m_socket_obj.get()), std::bind_front(&tcp::handle_events, this))
    , m_state(state) {}

tcp::state tcp::get_state() const {
    return m_state;
}

void tcp::bind(const uint16_t port) {
    auto [lock, stream_control] = m_stream.use();
    stream_control.state.verify_not_errored();

    OS_CHECK_THROW(os::tcp::bind(m_socket_obj.get(), port));
}

void tcp::bind(const std::string_view address, const uint16_t port) {
    auto [lock, stream_control] = m_stream.use();
    stream_control.state.verify_not_errored();

    OS_CHECK_THROW(os::tcp::bind(m_socket_obj.get(), address, port));
}

void tcp::connect(const std::string_view address, const uint16_t port, tcp_callback&& callback) {
    auto [lock, stream_control] = m_stream.use();
    stream_control.state.verify_not_errored();

    if (m_state != state::open) {
        throw std::runtime_error("tcp state invalid for connect");
    }

    looper_trace_info(log_module, "connecting tcp: handle=%lu", m_stream.handle());

    m_connect_callback = callback;
    m_state = state::connecting;
    const auto error = os::tcp::connect(m_socket_obj.get(), address, port);
    if (error == error_success) {
        // connection finished
        on_connect_done(lock, stream_control);
    } else if (error == error_in_progress) {
        // wait for connection finish
        stream_control.request_events(event_out, events_update_type::append);
        looper_trace_info(log_module, "tcp connection not finished: handle=%lu", m_stream.handle());
    } else {
        on_connect_done(lock, stream_control, error);
    }
}

void tcp::close() {
    auto [lock, stream_control] = m_stream.use();

    if (m_socket_obj) {
        m_socket_obj.reset();
    }

    stream_control.state.set_read_enabled(false);
    stream_control.state.set_write_enabled(false);
    m_state = state::closed;

    // todo: detach?
}

void tcp::start_read(read_callback&& callback) {
    m_stream.start_read(std::move(callback));
}

void tcp::stop_read() {
    m_stream.stop_read();
}

void tcp::write(stream::write_request&& request) {
    m_stream.write(std::move(request));
}

bool tcp::handle_events(std::unique_lock<std::mutex>& lock, stream::control& stream_control, const event_types events) {
    switch (m_state) {
        case state::connecting: {
            if ((events & event_out) != 0) {
                handle_connect(lock, stream_control);
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

void tcp::handle_connect(std::unique_lock<std::mutex>& lock, stream::control& stream_control) {
    // finish connect
    const auto error = os::tcp::finalize_connect(m_socket_obj.get());
    stream_control.request_events(event_out, events_update_type::remove); // todo:
    on_connect_done(lock, stream_control, error);
}

void tcp::on_connect_done(std::unique_lock<std::mutex>& lock, stream::control& stream_control, const error error) {
    if (error == error_success) {
        // connection finished
        m_state = state::connected;
        looper_trace_info(log_module, "connected tcp: handle=%lu", m_stream.handle());

        stream_control.state.set_read_enabled(true);
        stream_control.state.set_write_enabled(true);

        invoke_func<>(lock, "tcp_loop_callback", m_connect_callback, m_stream.handle(), error);
    } else {
        stream_control.state.mark_errored();
        looper_trace_error(log_module, "tcp connection failed: handle=%lu, code=0x%x", m_stream.handle(), error);
        invoke_func<>(lock, "tcp_loop_callback", m_connect_callback, m_stream.handle(), error);
    }
}

looper::error tcp::read_from_obj(const std::span<uint8_t> buffer, size_t& read_out) {
    return os::tcp::read(m_socket_obj.get(), buffer.data(), buffer.size_bytes(), read_out);
}

looper::error tcp::write_to_obj(const std::span<const uint8_t> buffer, size_t& written_out) {
    return os::tcp::write(m_socket_obj.get(), buffer.data(), buffer.size_bytes(), written_out);
}

tcp_server::tcp_server(const looper::tcp_server handle, const loop_ptr& loop)
    : m_handle(handle)
    , m_loop(loop)
    , m_resource(loop)
    , m_socket_obj(os::make_tcp())
    , m_callback(nullptr) {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(os::tcp::get_descriptor(m_socket_obj.get()), 0,
        std::bind_front(&tcp_server::handle_events, this));
}

void tcp_server::bind(const uint16_t port) {
    auto [lock, control] = m_resource.lock_loop();
    OS_CHECK_THROW(os::tcp::bind(m_socket_obj.get(), port));
}

void tcp_server::bind(const std::string_view address, const uint16_t port) {
    auto [lock, control] = m_resource.lock_loop();
    OS_CHECK_THROW(os::tcp::bind(m_socket_obj.get(), address, port));
}

void tcp_server::listen(const size_t backlog, tcp_server_callback&& callback) {
    auto [lock, control] = m_resource.lock_loop();

    OS_CHECK_THROW(os::tcp::listen(m_socket_obj.get(), backlog));
    m_callback = callback;

    control.request_events(event_in, events_update_type::append);
}

std::unique_ptr<tcp> tcp_server::accept(looper::handle handle) {
    os::tcp::tcp* tcp_struct;
    OS_CHECK_THROW(os::tcp::accept(m_socket_obj.get(), &tcp_struct));
    auto socket = os::make_tcp(tcp_struct);

    return std::make_unique<tcp>(handle, m_loop, std::move(socket));
}

void tcp_server::close() {
    auto [lock, control] = m_resource.lock_loop();

    if (m_socket_obj) {
        m_socket_obj.reset();
    }

    control.detach_from_loop();
}

void tcp_server::handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control& control, const event_types events) const {
    if ((events & event_in) != 0) {
        // new data
        invoke_func(lock, "tcp_server_callback", m_callback, m_handle);
    }
}

}
