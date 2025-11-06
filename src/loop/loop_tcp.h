#pragma once

#include "loop_stream.h"
#include "os/factory.h"

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
    tcp(looper::tcp handle, const loop_ptr& loop, os::tcp_ptr&& socket);

    [[nodiscard]] state get_state() const;

    void bind(uint16_t port);
    void bind(std::string_view address, uint16_t port);
    void connect(std::string_view address, uint16_t port, tcp_callback&& callback);
    void close();

    void start_read(read_callback&& callback);
    void stop_read();
    void write(stream::write_request&& request);

private:
    tcp(looper::tcp handle, const loop_ptr& loop, os::tcp_ptr&& socket, state state);

    bool handle_events(std::unique_lock<std::mutex>& lock, stream::control& stream_control, event_types events);
    void handle_connect(std::unique_lock<std::mutex>& lock, stream::control& stream_control);
    void on_connect_done(std::unique_lock<std::mutex>& lock, stream::control& stream_control, error error = error_success);

    looper::error read_from_obj(std::span<uint8_t> buffer, size_t& read_out);
    looper::error write_to_obj(std::span<const uint8_t> buffer, size_t& written_out);

    os::tcp_ptr m_socket_obj;
    stream m_stream;
    state m_state;
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
    os::tcp_ptr m_socket_obj;
    tcp_server_callback m_callback;
};

}
