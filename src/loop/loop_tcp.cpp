
#include "loop_internal.h"
#include "loop_tcp.h"

namespace looper::impl {

#define log_module loop_log_module "_tcp"

tcp::tcp(const looper::tcp handle, loop_context *context)
    : stream(handle, context)
    , m_state(state::open)
    , m_socket_obj(os::make_tcp()) {
    attach_to_loop(os::tcp::get_descriptor(m_socket_obj.get()), 0);
}

tcp::tcp(const looper::tcp handle, loop_context *context, os::tcp_ptr&& socket)
    : stream(handle, context)
    , m_state(state::connected)
    , m_socket_obj(std::move(socket)) {
    std::unique_lock lock(m_context->mutex);
    attach_to_loop(os::tcp::get_descriptor(m_socket_obj.get()), 0);
    set_read_enabled(true);
    set_write_enabled(true);
}

void tcp::bind(const uint16_t port) {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    OS_CHECK_THROW(os::tcp::bind(m_socket_obj.get(), port));
}

void tcp::bind(const std::string_view address, const uint16_t port) {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    OS_CHECK_THROW(os::tcp::bind(m_socket_obj.get(), address, port));
}

void tcp::connect(const std::string_view address, const uint16_t port, tcp_callback&& callback) {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    if (m_state != state::open) {
        throw std::runtime_error("tcp state invalid for connect");
    }

    looper_trace_info(log_module, "connecting tcp: ptr=0x%x", this);

    m_connect_callback = callback;
    m_state = state::connecting;
    const auto error = os::tcp::connect(m_socket_obj.get(), address, port);
    if (error == error_success) {
        // connection finished
        on_connect_done(lock);
    } else if (error == error_in_progress) {
        // wait for connection finish
        request_events(event_out, events_update_type::append);
        looper_trace_info(log_module, "tcp connection not finished: ptr=0x%x", this);
    } else {
        on_connect_done(lock, error);
    }
}

void tcp::close() {
    std::unique_lock lock(m_context->mutex);

    if (m_socket_obj) {
        m_socket_obj.reset();
    }

    set_read_enabled(false);
    set_write_enabled(false);
    m_state = state::closed;

    detach_from_loop();
}

void tcp::handle_events(std::unique_lock<std::mutex>& lock, const event_types events) {
    switch (m_state) {
        case state::connecting: {
            if ((events & event_out) != 0) {
                handle_connect(lock);
            }

            break;
        }
        case state::connected: {
            stream::handle_events(lock, events);
            break;
        }
        default:
            break;
    }
}

looper::error tcp::read_from_obj(uint8_t* buffer, const size_t buffer_size, size_t& read_out) {
    return os::tcp::read(m_socket_obj.get(), buffer, buffer_size, read_out);
}

looper::error tcp::write_to_obj(const uint8_t* buffer, const size_t size, size_t& written_out) {
    return os::tcp::write(m_socket_obj.get(), buffer, size, written_out);
}

void tcp::handle_connect(std::unique_lock<std::mutex>& lock) {
    // finish connect
    const auto error = os::tcp::finalize_connect(m_socket_obj.get());
    request_events(event_out, events_update_type::remove);
    on_connect_done(lock, error);
}

void tcp::on_connect_done(std::unique_lock<std::mutex>& lock, const error error) {
    if (error == error_success) {
        // connection finished
        m_state = state::connected;
        looper_trace_info(log_module, "connected tcp: ptr=0x%x", this);

        set_read_enabled(true);
        set_write_enabled(true);

        invoke_func<>(lock, "tcp_loop_callback", m_connect_callback, m_context->handle, m_handle, error);
    } else {
        mark_errored();
        looper_trace_error(log_module, "tcp connection failed: ptr=0x%x, code=%s", this, error);
        invoke_func<>(lock, "tcp_loop_callback", m_connect_callback, m_context->handle, m_handle, error);
    }
}

tcp_server::tcp_server(const looper::tcp_server handle, loop_context *context)
    : looper_resource(context)
    , m_handle(handle)
    , m_socket_obj(os::make_tcp())
    , m_callback(nullptr) {
    attach_to_loop(os::tcp::get_descriptor(m_socket_obj.get()), 0);
}

void tcp_server::bind(const uint16_t port) {
    std::unique_lock lock(m_context->mutex);

    OS_CHECK_THROW(os::tcp::bind(m_socket_obj.get(), port));
}

void tcp_server::bind(const std::string_view address, const uint16_t port) {
    std::unique_lock lock(m_context->mutex);

    OS_CHECK_THROW(os::tcp::bind(m_socket_obj.get(), address, port));
}

void tcp_server::listen(const size_t backlog, tcp_server_callback&& callback) {
    std::unique_lock lock(m_context->mutex);

    OS_CHECK_THROW(os::tcp::listen(m_socket_obj.get(), backlog));
    m_callback = callback;

    request_events(event_in, events_update_type::append);
}

std::unique_ptr<tcp> tcp_server::accept(looper::handle handle) {
    std::unique_lock lock(m_context->mutex);

    // unlock for the duration of the accept process
    lock.unlock();
    os::tcp::tcp* tcp_struct;
    OS_CHECK_THROW(os::tcp::accept(m_socket_obj.get(), &tcp_struct));
    auto socket = os::make_tcp(tcp_struct);

    // lock for the creation of the new client looper object
    lock.lock();
    return std::make_unique<tcp>(handle, m_context, std::move(socket));
}

void tcp_server::close() {
    std::unique_lock lock(m_context->mutex);

    if (m_socket_obj) {
        m_socket_obj.reset();
    }

    detach_from_loop();
}

void tcp_server::handle_events(std::unique_lock<std::mutex>& lock, const event_types events) {
    if ((events & event_in) != 0) {
        // new data
        invoke_func(lock, "tcp_server_callback", m_callback, m_context->handle, m_handle);
    }
}

}
