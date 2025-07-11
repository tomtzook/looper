#pragma once

#include "loop_stream.h"
#include "os/factory.h"

namespace looper::impl {

class tcp final : public stream {
public:
    enum class state {
        open,
        connecting,
        connected,
        closed
    };

    tcp(looper::tcp handle, loop_context *context);
    tcp(looper::tcp handle, loop_context *context, os::tcp_ptr&& socket);

    state get_state() const;

    void bind(uint16_t port);
    void bind(std::string_view address, uint16_t port);
    void connect(std::string_view address, uint16_t port, tcp_callback&& callback);
    void close();

protected:
    void handle_events(std::unique_lock<std::mutex>& lock, event_types events) override;
    looper::error read_from_obj(uint8_t *buffer, size_t buffer_size, size_t &read_out) override;
    looper::error write_to_obj(const uint8_t *buffer, size_t size, size_t &written_out) override;

private:
    void handle_connect(std::unique_lock<std::mutex>& lock);
    void on_connect_done(std::unique_lock<std::mutex>& lock, error error = error_success);

    state m_state;
    os::tcp_ptr m_socket_obj;
    tcp_callback m_connect_callback;
};

class tcp_server final : looper_resource {
public:
    tcp_server(looper::tcp_server handle, loop_context* context);

    void bind(uint16_t port);
    void bind(std::string_view address, uint16_t port);
    void listen(size_t backlog, tcp_server_callback&& callback);
    std::unique_ptr<tcp> accept(looper::handle handle);
    void close();
protected:
    void handle_events(std::unique_lock<std::mutex>& lock, event_types events) override;
private:
    looper::tcp_server m_handle;
    os::tcp_ptr m_socket_obj;
    tcp_server_callback m_callback;
};

}
