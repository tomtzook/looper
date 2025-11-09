#pragma once

#include "loop_resource.h"
#include "os/os.h"

namespace looper::impl {

class stream {
public:
    struct write_request {
        std::unique_ptr<uint8_t[]> buffer;
        size_t pos;
        size_t size;
        looper::write_callback write_callback;

        looper::error error;
    };

    struct control {
    public:
        control(resource_state& state, const loop_resource::control& resource_control);

        void request_events(event_types events, events_update_type type) const;

        resource_state& state;
    private:
        loop_resource::control m_resource_control;
    };

    using handle_events_ext_func = std::function<bool(std::unique_lock<std::mutex>&, control&, event_types)>;

    template<os::detail::os_object_type os_obj>
    stream(const looper::handle handle, const loop_ptr& loop, os_obj&& obj, handle_events_ext_func&& handle_events)
        : stream(handle, loop, os::streamable_object(std::forward<os_obj>(obj)), std::move(handle_events)) {

    }

    [[nodiscard]] looper::handle handle() const;

    void start_read(read_callback&& callback);
    void stop_read();
    void write(write_request&& request);

protected:
    std::pair<std::unique_lock<std::mutex>, control> use();

private:
    stream(looper::handle handle,
            const loop_ptr& loop,
            os::streamable_object&& stream_obj,
            handle_events_ext_func&& handle_events_ext);

    void handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control& control, event_types events);
    void handle_read(std::unique_lock<std::mutex>& lock, loop_resource::control& control);
    void handle_write(std::unique_lock<std::mutex>& lock, loop_resource::control& control);
    void report_write_requests_finished(std::unique_lock<std::mutex>& lock);
    bool do_write();

    looper::handle m_handle;
    loop_resource m_resource;
    resource_state m_state;

    os::streamable_object m_stream_obj;
    handle_events_ext_func m_handle_events_ext_func;

    read_callback m_user_read_callback;
    std::deque<write_request> m_write_requests;
    std::deque<write_request> m_completed_write_requests;

    bool m_write_pending;
};

template<typename t_han_, typename t_ptr_>
class stream_client : stream {
public:
    using handle_type = t_han_;
    using skt_object_type = t_ptr_;

    enum class state {
        open,
        connecting,
        connected,
        closed
    };

    stream_client(handle_type handle, const loop_ptr& loop);
    stream_client(handle_type handle, const loop_ptr& loop, skt_object_type&& socket);

    [[nodiscard]] state get_state() const;

    void close();

private:
    stream_client(handle_type handle, const loop_ptr& loop, skt_object_type&& socket, state state);

    bool handle_events(std::unique_lock<std::mutex>& lock, stream::control& stream_control, event_types events);
    void handle_connect(std::unique_lock<std::mutex>& lock, stream::control& stream_control);
    void on_connect_done(std::unique_lock<std::mutex>& lock, stream::control& stream_control, error error = error_success);

    skt_object_type m_res_obj;
    stream m_stream;
    state m_state;
    tcp_callback m_connect_callback;
};

}
