#pragma once

#include "os/os.h"
#include "loop_io.h"

namespace looper::impl {

struct stream_write_request {
    std::unique_ptr<uint8_t[]> buffer;
    size_t pos;
    size_t size;
    looper::write_callback write_callback;

    looper::error error;
};

struct stream_read_data {
    std::span<uint8_t> buffer;
    size_t read_count;
    looper::error error;
};

template<os::os_stream_type t_>
struct stream_io {
    using underlying_type = t_;

    explicit stream_io(t_&& obj);

    [[nodiscard]] os::descriptor get_descriptor() const;

    looper::error read(stream_read_data& data);
    looper::error write(const stream_write_request& request, size_t& written);
    looper::error finalize_connect();
    void close();

    t_ m_obj;
};

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

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
class stream_socket_client final {
public:
    using io_type = io<stream_write_request, stream_read_data, stream_io<t_>>;

    stream_socket_client(looper::handle handle, const loop_ptr& loop, t_&& skt_obj, bool connected = false);
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
    io_type m_io;
};

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
class socket_server final {
public:
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

class udp_socket final {
public:
    using io_type = io<udp_write_request, udp_read_data, udp_io>;

    udp_socket(looper::handle handle, const loop_ptr& loop);

    looper::error bind(uint16_t port);
    looper::error bind(std::string_view address, uint16_t port);

    looper::error start_read(udp_read_callback&& callback);
    looper::error stop_read();
    looper::error write(udp_write_request&& request);

    void close();

private:
    io_type m_io;
};

template<os::os_stream_type t_>
stream_io<t_>::stream_io(t_&& obj)
    : m_obj(std::move(obj))
{}

template<os::os_stream_type t_>
os::descriptor stream_io<t_>::get_descriptor() const {
    return os::get_descriptor(m_obj);
}

template<os::os_stream_type t_>
looper::error stream_io<t_>::read(stream_read_data& data) {
    return os::detail::os_stream<t_>::read(m_obj, data.buffer, data.read_count);
}

template<os::os_stream_type t_>
looper::error stream_io<t_>::write(const stream_write_request& request, size_t& written) {
    return os::detail::os_stream<t_>::write(
        m_obj,
        std::span<uint8_t>{ request.buffer.get() + request.pos, request.size - request.pos },
        written);
}

template<os::os_stream_type t_>
looper::error stream_io<t_>::finalize_connect() {
    return os::detail::os_socket<t_>::finalize_connect(m_obj);
}

template<os::os_stream_type t_>
void stream_io<t_>::close() {
    m_obj.close();
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
stream_socket_client<t_, bind_func_, connect_func_>::stream_socket_client(looper::handle handle, const loop_ptr& loop, t_&& skt_obj, const bool connected)
    : m_io(handle, loop, stream_io<t_>(std::move(skt_obj))) {
    m_io.register_to_loop();

    if (connected) {
        m_io.mark_connected();
    }
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
stream_socket_client<t_, bind_func_, connect_func_>::stream_socket_client(looper::handle handle, const loop_ptr& loop)
    : stream_socket_client(handle, loop, t_::create())
{}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
template<typename... args_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::bind(args_... args) {
    auto [lock, control] = m_io.use();
    RETURN_IF_ERROR(control.state.verify_not_errored());

    return bind_func_()(m_io.io_obj().m_obj, args...);
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
template<typename... args_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::connect(connect_callback&& callback, args_... args) {
    return m_io.connect([args...](const stream_io<t_>& t)->looper::error {
        return connect_func_()(t.m_obj, args...);
    }, std::move(callback));
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::start_read(looper::read_callback&& callback) {
    return m_io.start_read([callback](const looper::handle handle, const stream_read_data& data)->void {
        callback(handle, data.buffer, data.error);
    });
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::stop_read() {
    return m_io.stop_read();
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
looper::error stream_socket_client<t_, bind_func_, connect_func_>::write(stream_write_request&& request) {
    return m_io.write(std::move(request));
}

template<os::os_stream_type t_, typename bind_func_, typename connect_func_>
void stream_socket_client<t_, bind_func_, connect_func_>::close() {
    m_io.close();
}

template<os::os_stream_type t_, typename t_client_, typename bind_func_>
socket_server<t_, t_client_, bind_func_>::socket_server(const looper::handle handle, const loop_ptr& loop)
    : m_handle(handle)
    , m_loop(loop)
    , m_resource(loop)
    , m_socket_obj(t_::create())
    , m_callback(nullptr) {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(
        os::get_descriptor(m_socket_obj),
        0,
        std::bind_front(&socket_server::handle_events, this));
}

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
    return {error_success, std::make_unique<t_client_>(new_handle, m_loop, std::move(new_obj), true)};
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

}
