#pragma once

#include "os/factory.h"
#include "loop_resource.h"

namespace looper::impl {

class udp final {
public:
    struct write_request {
        std::unique_ptr<uint8_t[]> buffer;
        size_t pos;
        size_t size;
        looper::write_callback write_callback;

        inet_address destination;

        looper::error error;
    };

    udp(looper::udp handle, loop_context* context);

    void bind(uint16_t port);
    void bind(std::string_view address, uint16_t port);

    void start_read(udp_read_callback&& callback);
    void stop_read();
    void write(write_request&& request);

    void close();

private:
    void handle_events(std::unique_lock<std::mutex>& lock, looper_resource::control& control, event_types events);
    void handle_read(std::unique_lock<std::mutex>& lock, looper_resource::control& control);
    void handle_write(std::unique_lock<std::mutex>& lock, looper_resource::control& control);
    void report_write_requests_finished(std::unique_lock<std::mutex>& lock);
    bool do_write();

    looper::udp m_handle;
    os::udp_ptr m_socket_obj;
    looper_resource m_resource;
    resource_state m_state;
    bool m_write_pending;

    udp_read_callback m_read_callback;
    std::deque<write_request> m_write_requests;
    std::deque<write_request> m_completed_write_requests;
};

}
