#pragma once

#include "loop_io.h"

namespace looper::impl {

class tcp final {
public:
    enum class state {
        open,
        connecting,
        connected,
        closed
    };

    tcp(looper::tcp handle, const loop_ptr& loop);
    tcp(looper::tcp handle, const loop_ptr& loop, os::tcp&& socket);

    void bind(uint16_t port);
    void bind(std::string_view address, uint16_t port);
    void connect(std::string_view address, uint16_t port, tcp_callback&& callback);

    void start_read(looper::read_callback&& callback);
    void stop_read();
    void write(stream_write_request&& request);

    void close();

private:
    tcp(looper::tcp handle, const loop_ptr& loop, os::tcp&& socket, state state);

    bool handle_events(std::unique_lock<std::mutex>& lock, const io_control& control, event_types events);
    void handle_connect(std::unique_lock<std::mutex>& lock, const io_control& control);
    void on_connect_done(std::unique_lock<std::mutex>& lock, const io_control& control, error error = error_success);

    state m_state;
    stream<os::tcp> m_stream;
    tcp_callback m_connect_callback;
};

class tcp_server final {
public:
    tcp_server(looper::tcp_server handle, const loop_ptr& loop);

    void bind(uint16_t port);
    void bind(std::string_view address, uint16_t port);

    void listen(size_t backlog, tcp_server_callback&& callback);
    std::unique_ptr<tcp> accept(looper::handle handle);

    void close();

private:
    void handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control& control, event_types events) const;

    looper::tcp_server m_handle;
    loop_ptr m_loop;
    loop_resource m_resource;
    os::tcp m_socket_obj;
    tcp_server_callback m_callback;
};

}
