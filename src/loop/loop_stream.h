#pragma once

#include "loop_resource.h"

namespace looper::impl {

class stream final {
public:
    // todo: make those functions into a concept?
    using read_from_obj = std::function<looper::error(std::span<uint8_t>, size_t&)>;
    using write_to_obj = std::function<looper::error(std::span<const uint8_t>, size_t&)>;

    struct write_request {
        std::unique_ptr<uint8_t[]> buffer;
        size_t pos;
        size_t size;
        looper::write_callback write_callback;

        looper::error error;
    };

    struct control {
    public:
        control(resource_state& state, const looper_resource::control& resource_control);

        void request_events(event_types events, events_update_type type);

        resource_state& state;
    private:
        looper_resource::control m_resource_control;
    };

    using handle_events_ext_func = std::function<bool(std::unique_lock<std::mutex>&, control&, event_types)>;

    stream(looper::handle handle, loop_context* context,
        read_from_obj&& read_from_obj, write_to_obj&& write_to_obj,
        os::descriptor os_descriptor, handle_events_ext_func&& handle_events_ext);

    [[nodiscard]] looper::loop loop_handle() const;
    [[nodiscard]] looper::handle handle() const;

    std::pair<std::unique_lock<std::mutex>, control> use();

    void start_read(read_callback&& callback);
    void stop_read();
    void write(write_request&& request);

private:
    void handle_events(std::unique_lock<std::mutex>& lock, looper_resource::control& control, event_types events);
    void handle_read(std::unique_lock<std::mutex>& lock, looper_resource::control& control);
    void handle_write(std::unique_lock<std::mutex>& lock, looper_resource::control& control);
    void report_write_requests_finished(std::unique_lock<std::mutex>& lock, const looper_resource::control& control);
    bool do_write();

    looper::handle m_handle;
    looper_resource m_resource;
    resource_state m_state;

    read_from_obj m_read_from_obj;
    write_to_obj m_write_to_obj;
    handle_events_ext_func m_handle_events_ext_func;

    read_callback m_user_read_callback;
    std::deque<write_request> m_write_requests;
    std::deque<write_request> m_completed_write_requests;

    bool m_reading;
    bool m_write_pending;
};

}
