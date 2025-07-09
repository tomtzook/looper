#pragma once

#include "os/factory.h"
#include "loop_resource.h"

namespace looper::impl {

class udp final : looper_resource {
public:
    struct write_request {
        std::unique_ptr<uint8_t[]> buffer;
        size_t pos;
        size_t size;
        looper::write_callback write_callback;

        inet_address destination;

        looper::error error;
    };

    udp(looper::udp handle, loop_context *context);

    void bind(uint16_t port);
    void bind(std::string_view address, uint16_t port);

    void start_read(udp_read_callback&& callback);
    void stop_read();
    void write(write_request&& request);

    void close();

protected:
    void handle_events(std::unique_lock<std::mutex>& lock, event_types events) override;

private:
    void handle_read(std::unique_lock<std::mutex>& lock);
    void handle_write(std::unique_lock<std::mutex>& lock);
    void report_write_requests_finished(std::unique_lock<std::mutex>& lock);
    bool do_write();

    bool is_errored() const;
    void mark_errored();
    void verify_not_errored() const;

    looper::udp m_handle;
    os::udp_ptr m_socket_obj;

    udp_read_callback m_read_callback;
    std::deque<write_request> m_write_requests;
    std::deque<write_request> m_completed_write_requests;

    bool m_is_errored;
    bool m_reading;
    bool m_write_pending;
};

}
