#pragma once

#include "loop_resource.h"

namespace looper::impl {

class stream : protected looper_resource {
public:
    struct write_request {
        std::unique_ptr<uint8_t[]> buffer;
        size_t pos;
        size_t size;
        looper::write_callback write_callback;

        looper::error error;
    };

    explicit stream(handle handle, loop_context* context);

    void start_read(read_callback&& callback);
    void stop_read();
    void write(write_request&& request);

protected:
    virtual void handle_events(std::unique_lock<std::mutex>& lock, event_types events) override;

    void set_read_enabled(bool enabled);
    void set_write_enabled(bool enabled);
    bool is_errored() const;
    void mark_errored();
    void verify_not_errored() const;

    virtual looper::error read_from_obj(uint8_t* buffer, size_t buffer_size, size_t& read_out) = 0;
    virtual looper::error write_to_obj(const uint8_t* buffer, size_t size, size_t& written_out) = 0;

    looper::handle m_handle;

private:
    void handle_read(std::unique_lock<std::mutex>& lock);
    void handle_write(std::unique_lock<std::mutex>& lock);
    void report_write_requests_finished(std::unique_lock<std::mutex>& lock);
    bool do_write();

    bool m_is_errored;
    bool m_can_read;
    bool m_can_write;

    read_callback m_user_read_callback;
    std::deque<write_request> m_write_requests;
    std::deque<write_request> m_completed_write_requests;

    bool m_reading;
    bool m_write_pending;
};

}
