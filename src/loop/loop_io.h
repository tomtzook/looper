#pragma once

#include "loop_resource.h"
#include "os/os.h"


namespace looper::impl {

template<typename t_>
concept write_request_type = requires(t_ t) {
    { t.pos } -> std::convertible_to<size_t>;
    { t.size } -> std::convertible_to<size_t>;
    { t.error } -> std::convertible_to<looper::error>;
    { t.write_callback } -> std::convertible_to<looper::write_callback>;
};

template<typename t_>
concept read_data_type = requires(t_ t) {
    { t.buffer } -> std::convertible_to<std::span<const uint8_t>>;
    { t.read_count } -> std::convertible_to<size_t>;
    { t.error } -> std::convertible_to<looper::error>;
};

template<typename t_, typename wr_t_, typename rd_t_>
concept io_type = requires(
    t_ t,
    rd_t_& f1_data,
    const wr_t_& f2_request, size_t& f2_written) {
    { t.get_descriptor() } -> std::same_as<os::descriptor>;
    { t.read(f1_data) } -> std::same_as<looper::error>;
    { t.write(f2_request, f2_written) } -> std::same_as<looper::error>;
    { t.close() } -> std::same_as<void>;
};

struct io_control {
    io_control(resource_state& state, loop_resource::control resource_control);

    void request_events(event_types events, events_update_type type) const;
    void invoke_in_loop(loop_callback&& callback) const;

    template<typename... args_>
    void invoke_in_loop(const std::function<void(args_...)>& ref, args_... args) const {
        auto ref_val = ref;
        if (ref_val != nullptr) {
            invoke_in_loop([ref_val, args...]()->void {
                ref_val(args...);
            });
        }
    }

    resource_state& state;
private:
    loop_resource::control m_resource_control;
};

// todo: potentional problem with moving this object and similar because of callbacks to loop and such being pointers
//  to old memory
template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
class io {
public:
    using write_request = t_wr_;
    using read_data = t_rd_;
    using io_type = t_io_;
    using read_callback = std::function<void(looper::handle, const t_rd_&)>;
    using custom_handle_events = std::function<bool(std::unique_lock<std::mutex>&, const io_control&, event_types)>;

    io(looper::handle handle, const loop_ptr& loop, io_type&& io_obj);

    io(const io&) = delete;
    io(io&&) = default;
    io& operator=(const io&) = delete;
    io& operator=(io&&) = default;

    [[nodiscard]] looper::handle handle() const;
    [[nodiscard]] const io_type& io_obj() const;

    void register_to_loop();

    std::pair<std::unique_lock<std::mutex>, io_control> use();
    void set_custom_event_handler(custom_handle_events&& func);

    void start_read(read_callback&& callback);
    void stop_read();
    void write(write_request&& request);

    void close();

protected:
    const looper::handle m_handle;

private:
    void handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control& control, event_types events);
    void handle_read(std::unique_lock<std::mutex>& lock, const loop_resource::control& control);
    void handle_write(std::unique_lock<std::mutex>& lock, const loop_resource::control& control);
    void report_write_requests_finished(std::unique_lock<std::mutex>& lock);
    bool do_write();

    io_type m_io;

    loop_resource m_resource;
    resource_state m_state;
    custom_handle_events m_custom_handle_events;

    read_callback m_read_callback;
    std::deque<write_request> m_write_requests;
    std::deque<write_request> m_completed_write_requests;
    bool m_write_pending;
};

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
    explicit stream_io(t_&& obj);

    [[nodiscard]] os::descriptor get_descriptor() const;

    looper::error read(stream_read_data& data);
    looper::error write(const stream_write_request& request, size_t& written);
    void close();

    t_ m_obj;
};

template<os::os_stream_type t_>
class stream : public io<stream_write_request, stream_read_data, stream_io<t_>> {
public:
    // using handle_events_ext_func = std::function<bool(std::unique_lock<std::mutex>&, control&, event_types)>;
    stream(looper::handle handle, const loop_ptr& loop, t_&& obj);

    void start_read_stream(looper::read_callback&& callback);
};

#define loop_io_log_module loop_log_module "_io"

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
io<t_wr_, t_rd_, t_io_>::io(const looper::handle handle, const loop_ptr& loop, io_type&& io_obj)
    : m_handle(handle)
    , m_io(std::move(io_obj))
    , m_resource(loop)
    , m_state()
    , m_custom_handle_events()
    , m_read_callback()
    , m_write_requests()
    , m_completed_write_requests()
    , m_write_pending(false)
{}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::handle io<t_wr_, t_rd_, t_io_>::handle() const {
    return m_handle;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
const t_io_& io<t_wr_, t_rd_, t_io_>::io_obj() const {
    return m_io;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::register_to_loop() {
    auto [lock, control] = m_resource.lock_loop();
    control.attach_to_loop(
        m_io.get_descriptor(),
        0,
        std::bind_front(&io::handle_events, this));
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
std::pair<std::unique_lock<std::mutex>, io_control> io<t_wr_, t_rd_, t_io_>::use() {
    auto [lock, res_control] = m_resource.lock_loop();
    return {std::move(lock), io_control(m_state, res_control)};
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::set_custom_event_handler(custom_handle_events&& func) {
    m_custom_handle_events = std::move(func);
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::start_read(read_callback&& callback) {
    auto [lock, control] = m_resource.lock_loop();
    m_state.verify_not_errored();
    m_state.verify_not_reading();

    looper_trace_info(loop_io_log_module, "io starting read: handle=%lu", m_handle);

    m_read_callback = callback;
    control.request_events(event_in, events_update_type::append);
    m_state.set_reading(true);
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::stop_read() {
    auto [lock, control] = m_resource.lock_loop();

    if (!m_state.is_reading()) {
        return;
    }

    looper_trace_info(loop_io_log_module, "io stopping read: handle=%lu", m_handle);

    m_state.set_reading(false);
    control.request_events(event_in, events_update_type::remove);
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::write(write_request&& request) {
    auto [lock, control] = m_resource.lock_loop();
    m_state.verify_not_errored();

    looper_trace_info(loop_io_log_module, "writing, new request: handle=%lu, buffer_size=%lu", m_handle, request.size);

    m_write_requests.push_back(std::move(request));

    if (!m_write_pending) {
        control.request_events(event_out, events_update_type::append);
        m_write_pending = true;
    }
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::close() {
    auto [lock, control] = m_resource.lock_loop();

    m_io.close();
    control.detach_from_loop();
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control& control, const event_types events) {
    if (m_state.is_errored() && (events & (event_error | event_hung)) != 0) {
        control.request_events(0, events_update_type::override);
        control.detach_from_loop();
        return;
    }

    const io_control our_control(m_state, control);
    if (m_custom_handle_events(lock, our_control, events)) {
        return;
    }

    if ((events & event_in) != 0) {
        // new data
        handle_read(lock, control);
    }

    if ((events & event_out) != 0) {
        handle_write(lock, control);
    }
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::handle_read(std::unique_lock<std::mutex>& lock, const loop_resource::control& control) {
    if (!m_state.is_reading() || m_state.is_errored() || !m_state.can_read()) {
        control.request_events(event_in, events_update_type::remove);
        return;
    }

    uint8_t read_buffer[1024]{};
    t_rd_ read_data{};
    read_data.buffer = std::span<uint8_t>{read_buffer, sizeof(read_buffer)};
    const auto error = m_io.read(read_data);
    read_data.error = error;

    if (error == error_success) {
        read_data.buffer = std::span<uint8_t>{read_buffer, read_data.read_count};
        looper_trace_debug(loop_io_log_module, "stream read new data: handle=%lu, data_size=%lu", m_handle, read_data.buffer.size());
    } else {
        m_state.mark_errored();
        looper_trace_error(loop_io_log_module, "stream read error: handle=%lu, code=%lu", m_handle, error);
    }

    invoke_func<std::mutex, looper::handle, const t_rd_&>(lock, "io_loop_callback", m_read_callback, m_handle, read_data);
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::handle_write(std::unique_lock<std::mutex>& lock, const loop_resource::control& control) {
    if (!m_write_pending || m_state.is_errored() || !m_state.can_write()) {
        control.request_events(event_out, events_update_type::remove);
        return;
    }

    if (!do_write()) {
        m_state.mark_errored();
        m_write_pending = false;
        control.request_events(event_out, events_update_type::remove);
    } else {
        if (m_write_requests.empty()) {
            m_write_pending = false;
            control.request_events(event_out, events_update_type::remove);
        }
    }

    report_write_requests_finished(lock);
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::report_write_requests_finished(std::unique_lock<std::mutex>& lock) {
    while (!m_completed_write_requests.empty()) {
        auto& request = m_completed_write_requests.front();
        invoke_func<>(lock, "loop_io_log_module", request.write_callback, m_handle, request.error);

        m_completed_write_requests.pop_front();
    }
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
bool io<t_wr_, t_rd_, t_io_>::do_write() {
    // todo: better use of queues
    // todo: use iovec

    static constexpr size_t max_writes_to_do_in_one_iteration = 16;

    // only write up to 16 requests, so as not to starve the loop with writes
    size_t write_count = max_writes_to_do_in_one_iteration;

    while (!m_write_requests.empty() && (write_count--) > 0) {
        auto& request = m_write_requests.front();

        size_t written;
        const auto error = m_io.write(request, written);
        if (error == error_success) {
            request.pos += written;
            if (request.pos < request.size) {
                // didn't finish write
                return true;
            }

            looper_trace_debug(loop_io_log_module, "io write request finished: handle=%lu", m_handle);
            request.error = error_success;

            m_completed_write_requests.push_back(std::move(request));
            m_write_requests.pop_front();
        } else if (error == error_in_progress || error == error_again) {
            // didn't finish write, but need to try again later
            return true;
        } else {
            looper_trace_error(loop_io_log_module, "io write request failed: handle=%lu, code=%lu", m_handle, error);
            request.error = error;

            m_completed_write_requests.push_back(std::move(request));
            m_write_requests.pop_front();

            return false;
        }
    }

    return true;
}

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
void stream_io<t_>::close() {
    m_obj.close();
}

template<os::os_stream_type t_>
stream<t_>::stream(looper::handle handle, const loop_ptr& loop, t_&& obj)
    : io<stream_write_request, stream_read_data, stream_io<t_>>(handle, loop, stream_io<t_>(std::move(obj)))
{}

template<os::os_stream_type t_>
void stream<t_>::start_read_stream(looper::read_callback&& callback) {
    this->start_read([callback](const looper::handle handle, const stream_read_data& data)->void {
        callback(handle, data.buffer, data.error);
    });
}

}
