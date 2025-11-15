#pragma once

#include "os/os.h"
#include "loop_io.h"

namespace looper::impl {

template<write_request_type t_wr_, typename t_io_>
struct base_socket_client {
    using write_request = t_wr_;
    using io = t_io_;

    enum class state {
        open,
        closed
    };

    base_socket_client(io&& io, state state);

    state m_state;
    io m_io;
};

template<write_request_type t_wr_, is_connectable_io t_io_>
struct base_connectable_socket_client {
    using write_request = t_wr_;
    using io = t_io_;
    using connector = std::function<looper::error(const io&)>;

    enum class state {
        open,
        connecting,
        connected,
        closed
    };

    base_connectable_socket_client(io&& io, state state);

    // todo: this throws while (possible) and is called directly from the user, while handle_events
    //  is called only from loop, so this is a bit not clear
    //  maybe everything must be done via the loop
    looper::error connect(connector&& connector, connect_callback&& callback);

    bool handle_events(std::unique_lock<std::mutex>& lock, const io_control& control, event_types events);
    void handle_connect(std::unique_lock<std::mutex>& lock, const io_control& control);
    void on_connect_done(std::unique_lock<std::mutex>& lock, const io_control& control, error error = error_success);

    state m_state;
    io m_io;
    connect_callback m_connect_callback;
};

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
class stream_socket_client final {
public:
    using base_type = base_connectable_socket_client<stream_write_request, stream<t_>>;

    stream_socket_client(looper::handle handle, const loop_ptr& loop, t_&& skt_obj, base_type::state state);
    stream_socket_client(looper::handle handle, const loop_ptr& loop);

    template<typename... args_>
    looper::error bind(args_... args);
    template<typename... args_>
    looper::error connect(connect_callback&& callback, args_... args);

    looper::error start_read(looper::read_callback&& callback);
    looper::error stop_read();
    looper::error write(stream_write_request&& request);

    void close();

private:
    base_type m_base;
};

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
class socket_server final {
public:
    socket_server(looper::handle handle, const loop_ptr& loop, t_&& skt);
    socket_server(looper::handle handle, const loop_ptr& loop);

    template<typename... args_>
    looper::error bind(args_... args);

    looper::error listen(size_t backlog, listen_callback&& callback);
    std::pair<looper::error, std::unique_ptr<t_client_>> accept(looper::handle new_handle);

    void close();

private:
    void handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control& control, event_types events) const;

    looper::handle m_handle;
    loop_ptr m_loop;
    loop_resource m_resource;
    t_ m_socket_obj;
    listen_callback m_callback;
};

struct tcp_bind_func {
    looper::error operator()(const os::tcp& obj, const std::string_view ip, const uint16_t port) const {
        return os::ipv4_bind(obj, ip, port);
    }
    looper::error operator()(const os::tcp& obj, const uint16_t port) const {
        return os::ipv4_bind(obj, port);
    }
};

struct tcp_connect_func {
    looper::error operator()(const os::tcp& obj, const std::string_view ip, const uint16_t port) const {
        return os::ipv4_connect(obj, ip, port);
    }
};

using tcp_client = stream_socket_client<os::tcp, tcp_bind_func, tcp_connect_func>;
using tcp_server = socket_server<os::tcp, tcp_client, tcp_bind_func>;

#ifdef LOOPER_UNIX_SOCKETS

struct unix_socket_bind_func {
    looper::error operator()(const os::unix_socket& obj, const std::string_view path) const {
        return os::unix_bind(obj, path);
    }
};

struct unix_socket_connect_func {
    looper::error operator()(const os::unix_socket& obj, const std::string_view path) const {
        return os::unix_connect(obj, path);
    }
};

using unix_socket_client = stream_socket_client<os::unix_socket, unix_socket_bind_func, unix_socket_connect_func>;
using unix_socket_server = socket_server<os::unix_socket, unix_socket_client, unix_socket_bind_func>;

#endif

struct udp_write_request {
    size_t pos;
    size_t size;
    looper::error error;
    looper::write_callback write_callback;

    std::unique_ptr<uint8_t[]> buffer;
    inet_address destination;
};

struct udp_read_data {
    std::span<uint8_t> buffer;
    size_t read_count;
    inet_address sender;
    looper::error error;
};

struct udp_io {
    using underlying_type = os::udp;

    udp_io();

    [[nodiscard]] os::descriptor get_descriptor() const;
    looper::error read(udp_read_data& data) const;
    looper::error write(const udp_write_request& request, size_t& written) const;

    void close();

    os::udp m_obj;
};

class udp_socket final {
public:
    using io_type = io<udp_write_request, udp_read_data, udp_io>;
    using base_type = base_socket_client<udp_write_request, io_type>;

    udp_socket(looper::handle handle, const loop_ptr& loop);

    looper::error bind(uint16_t port);
    looper::error bind(std::string_view address, uint16_t port);

    looper::error start_read(udp_read_callback&& callback);
    looper::error stop_read();
    looper::error write(udp_write_request&& request);

    void close();

private:
    base_type m_base;
};

template<write_request_type t_wr_, typename t_io_>
base_socket_client<t_wr_, t_io_>::base_socket_client(io&& io, const state state)
    : m_state(state)
    , m_io(std::move(io)) {
    m_io.register_to_loop();

    auto [lock, control] = m_io.use();
    control.state.set_read_enabled(true);
    control.state.set_write_enabled(true);
}

template<write_request_type t_wr_, is_connectable_io t_io_>
base_connectable_socket_client<t_wr_, t_io_>::base_connectable_socket_client(io&& io, const state state)
    : m_state(state)
    , m_io(std::move(io))
    , m_connect_callback() {
    m_io.register_to_loop();

    auto [lock, control] = m_io.use();
    m_io.set_custom_event_handler(std::bind_front(&base_connectable_socket_client::handle_events, this));

    if (m_state == state::connected) {
        control.state.set_read_enabled(true);
        control.state.set_write_enabled(true);
    }
}

template<write_request_type t_wr_, is_connectable_io t_io_>
looper::error base_connectable_socket_client<t_wr_, t_io_>::connect(connector&& connector, connect_callback&& callback) {
    auto [lock, control] = m_io.use();
    control.state.verify_not_errored();

    if (m_state != state::open) {
        return error_invalid_state;
    }

    looper_trace_info(loop_io_log_module, "connecting socket: handle=%lu", m_io.handle());

    m_connect_callback = callback;
    m_state = state::connecting;
    const auto error = connector(m_io);
    if (error == error_success) {
        // connection finished
        on_connect_done(lock, control); // todo: can't call this since we will be locking the thread (the handler will). Must be scheduled for later to be done from looper thread.
    } else if (error == error_in_progress) {
        // wait for connection finish
        control.request_events(event_out, events_update_type::append);
        looper_trace_info(loop_io_log_module, "socket connection not finished: handle=%lu", m_io.handle());
    } else {
        on_connect_done(lock, control, error);
    }

    return error_success;
}

template<write_request_type t_wr_, is_connectable_io t_io_>
bool base_connectable_socket_client<t_wr_, t_io_>::handle_events(std::unique_lock<std::mutex>& lock, const io_control& control, const event_types events) {
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

template<write_request_type t_wr_, is_connectable_io t_io_>
void base_connectable_socket_client<t_wr_, t_io_>::handle_connect(std::unique_lock<std::mutex>& lock, const io_control& control) {
    // finish connect
    const auto error = os::finalize_connect(m_io.io_obj().m_obj);
    control.request_events(event_out, events_update_type::remove);
    on_connect_done(lock, control, error);
}

template<write_request_type t_wr_, is_connectable_io t_io_>
void base_connectable_socket_client<t_wr_, t_io_>::on_connect_done(std::unique_lock<std::mutex>& lock, const io_control& control, const error error) {
    if (error == error_success) {
        // connection finished
        m_state = state::connected;
        looper_trace_info(loop_io_log_module, "connected tcp: handle=%lu", m_io.handle());

        control.state.set_read_enabled(true);
        control.state.set_write_enabled(true);
        control.invoke_in_loop<>(m_connect_callback, m_io.handle(), error);
    } else {
        m_state = state::open;
        control.state.mark_errored();
        looper_trace_error(loop_io_log_module, "tcp connection failed: handle=%lu, code=0x%x", m_io.handle(), error);
        control.invoke_in_loop<>(m_connect_callback, m_io.handle(), error);
    }
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
stream_socket_client<t_, bind_func_, connect_func_>::stream_socket_client(looper::handle handle, const loop_ptr& loop, t_&& skt_obj, const typename base_type::state state)
    : m_base(stream<t_>(handle, loop, std::move(skt_obj)), state)
{}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
stream_socket_client<t_, bind_func_, connect_func_>::stream_socket_client(looper::handle handle, const loop_ptr& loop)
    : stream_socket_client(handle, loop, t_::create(), base_type::state::open)
{}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
template<typename... args_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::bind(args_... args) {
    auto [lock, control] = m_base.m_io.use();
    control.state.verify_not_errored();

    return bind_func_()(m_base.m_io.io_obj().m_obj, args...);
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
template<typename... args_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::connect(connect_callback&& callback, args_... args) {
    return m_base.connect([args...](const stream<t_>& stream)->looper::error {
        return connect_func_()(stream.io_obj().m_obj, args...);
    }, std::move(callback));
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::start_read(looper::read_callback&& callback) {
    return m_base.m_io.start_read_stream(std::move(callback));
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::stop_read() {
    return m_base.m_io.stop_read();
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::write(stream_write_request&& request) {
    return m_base.m_io.write(std::move(request));
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
void stream_socket_client<t_, bind_func_, connect_func_>::close() {
    m_base.m_io.close();
}

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
socket_server<t_, t_client_, bind_func_>::socket_server(const looper::handle handle, const loop_ptr& loop, t_&& skt)
    : m_handle(handle)
    , m_loop(loop)
    , m_resource(loop)
    , m_socket_obj(std::move(skt))
    , m_callback(nullptr) {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(
        os::get_descriptor(m_socket_obj),
        0,
        std::bind_front(&socket_server::handle_events, this));
}

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
socket_server<t_, t_client_, bind_func_>::socket_server(looper::handle handle, const loop_ptr& loop)
    : socket_server(handle, loop, t_::create())
{}

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
template<typename... args_>
looper::error socket_server<t_, t_client_, bind_func_>::bind(args_... args) {
    auto [lock, control] = m_resource.lock_loop();
    return bind_func_()(m_socket_obj, args...);
}

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
looper::error socket_server<t_, t_client_, bind_func_>::listen(size_t backlog, listen_callback&& callback) {
    auto [lock, control] = m_resource.lock_loop();

    const auto status = os::socket_listen(m_socket_obj, backlog);
    if (status != error_success) {
        return status;
    }

    m_callback = callback;
    control.request_events(event_in, events_update_type::append);

    return error_success;
}

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
std::pair<looper::error, std::unique_ptr<t_client_>> socket_server<t_, t_client_, bind_func_>::accept(looper::handle new_handle) {
    auto [lock, control] = m_resource.lock_loop();

    auto [error, new_obj] = os::socket_accept(m_socket_obj);
    if (error != error_success) {
        return {error, std::unique_ptr<t_client_>()};
    }

    lock.unlock();
    return {error_success, std::make_unique<t_client_>(new_handle, m_loop, std::move(new_obj), t_client_::base_type::state::connected)};
}

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
void socket_server<t_, t_client_, bind_func_>::close() {
    auto [lock, control] = m_resource.lock_loop();

    m_socket_obj.close();
    control.detach_from_loop();
}

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
void socket_server<t_, t_client_, bind_func_>::handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control&, const event_types events) const {
    if ((events & event_in) != 0) {
        // new data
        invoke_func(lock, "tcp_server_callback", m_callback, m_handle);
    }
}

}
