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

template<typename t_, typename wr_t_, typename rd_t_>
concept connectable_io_type = io_type<t_, wr_t_, rd_t_> && requires(t_ t) {
    { t.finalize_connect() } -> std::same_as<looper::error>;
};

struct io_control {
    io_control(resource_state& state, loop_resource::control resource_control);

    void request_events(event_type events, events_update_type type) const noexcept;

    resource_state& state;
private:
    loop_resource::control m_resource_control;
};

// todo: potentional problem with moving this object and similar because of callbacks to loop and such being pointers
//  to old memory
template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
struct base_io final {
    using write_request = t_wr_;
    using read_data = t_rd_;
    using io_type = t_io_;
    using read_callback = std::function<void(looper::handle, const t_rd_&)>;

    base_io(looper::handle handle, const loop_ptr& loop, io_type&& io_obj) noexcept;

    base_io(const base_io&) = delete;
    base_io(base_io&&) = default;
    base_io& operator=(const base_io&) = delete;
    base_io& operator=(base_io&&) = default;

    [[nodiscard]] looper::error start_read(read_callback&& callback) noexcept;
    [[nodiscard]] looper::error stop_read() noexcept;
    [[nodiscard]] looper::error write(write_request&& request) noexcept;

    void close() noexcept;

    void handle_read(std::unique_lock<std::mutex>& lock, const loop_resource::control& control) noexcept;
    void handle_write(std::unique_lock<std::mutex>& lock, const loop_resource::control& control) noexcept;
    void handle_connect(std::unique_lock<std::mutex>& lock, const loop_resource::control& control) noexcept requires connectable_io_type<t_io_, t_wr_, t_rd_>;
    void on_connect_done(std::unique_lock<std::mutex>& lock, const loop_resource::control& control, error error = error_success) noexcept requires connectable_io_type<t_io_, t_wr_, t_rd_>;
    void report_write_requests_finished(std::unique_lock<std::mutex>& lock) noexcept;
    bool do_write() noexcept;

    const looper::handle m_handle;
    io_type m_io;

    loop_resource m_resource;
    resource_state m_state;

    read_callback m_read_callback;
    std::deque<write_request> m_write_requests;
    std::deque<write_request> m_completed_write_requests;
    bool m_write_pending;
    connect_callback m_connect_callback;
    bool m_connection_pending;
    bool m_connected;
};

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
class io final {
public:
    using base = base_io<t_wr_, t_rd_, t_io_>;
    using write_request = t_wr_;
    using read_data = t_rd_;
    using io_type = t_io_;
    using read_callback = std::function<void(looper::handle, const t_rd_&)>;
    using connector = std::function<looper::error(const io_type&)>;

    io(looper::handle handle, const loop_ptr& loop, t_io_&& io_obj) noexcept;

    io(const io&) = delete;
    io(io&&) = default;
    io& operator=(const io&) = delete;
    io& operator=(io&&) = default;

    [[nodiscard]] looper::handle handle() const;
    [[nodiscard]] const io_type& io_obj() const;

    [[nodiscard]] std::pair<std::unique_lock<std::mutex>, io_control> use() noexcept;

    void register_to_loop() noexcept;

    [[nodiscard]] looper::error mark_connected() noexcept requires connectable_io_type<t_io_, t_wr_, t_rd_>;
    [[nodiscard]] looper::error connect(connector&& connector, connect_callback&& callback) noexcept requires connectable_io_type<t_io_, t_wr_, t_rd_>;

    [[nodiscard]] looper::error start_read(read_callback&& callback) noexcept;
    [[nodiscard]] looper::error stop_read() noexcept;
    [[nodiscard]] looper::error write(write_request&& request) noexcept;

    // todo: return errors if closed in other funcs
    void close() noexcept;

private:
    void handle_events(std::unique_lock<std::mutex>& lock, loop_resource::control& control, event_type events) noexcept;

    base m_base;
};

#define loop_io_log_module loop_log_module "_io"

// BASE_IO ---------------------------------------------------------

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
base_io<t_wr_, t_rd_, t_io_>::base_io(const looper::handle handle, const loop_ptr& loop, io_type&& io_obj) noexcept
    : m_handle(handle)
    , m_io(std::move(io_obj))
    , m_resource(loop)
    , m_state()
    , m_read_callback()
    , m_write_requests()
    , m_completed_write_requests()
    , m_write_pending(false)
    , m_connect_callback()
    , m_connection_pending(false)
    , m_connected(false)
{}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::error base_io<t_wr_, t_rd_, t_io_>::start_read(read_callback&& callback) noexcept {
    auto [lock, control] = m_resource.lock_loop();
    RETURN_IF_ERROR(m_state.verify_not_errored());
    RETURN_IF_ERROR(m_state.verify_not_reading());

    looper_trace_info(loop_io_log_module, "io starting read: handle=%lu", m_handle);

    m_read_callback = callback;
    control.request_events(event_type::in, events_update_type::append);
    m_state.set_reading(true);

    return error_success;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::error base_io<t_wr_, t_rd_, t_io_>::stop_read() noexcept {
    auto [lock, control] = m_resource.lock_loop();

    if (!m_state.is_reading()) {
        return error_success;
    }

    looper_trace_info(loop_io_log_module, "io stopping read: handle=%lu", m_handle);

    m_state.set_reading(false);
    control.request_events(event_type::in, events_update_type::remove);

    return error_success;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::error base_io<t_wr_, t_rd_, t_io_>::write(write_request&& request) noexcept {
    auto [lock, control] = m_resource.lock_loop();
    RETURN_IF_ERROR(m_state.verify_not_errored());

    looper_trace_info(loop_io_log_module, "writing, new request: handle=%lu, buffer_size=%lu", m_handle, request.size);

    m_write_requests.push_back(std::move(request));

    if (!m_write_pending) {
        control.request_events(event_type::out, events_update_type::append);
        m_write_pending = true;
    }

    return error_success;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void base_io<t_wr_, t_rd_, t_io_>::close() noexcept {
    auto [lock, control] = m_resource.lock_loop();

    control.detach_from_loop();
    m_io.close();
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void base_io<t_wr_, t_rd_, t_io_>::handle_read(std::unique_lock<std::mutex>& lock, const loop_resource::control& control) noexcept {
    if (!m_state.is_reading() || m_state.is_errored() || !m_state.can_read()) {
        control.request_events(event_type::in, events_update_type::remove);
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
void base_io<t_wr_, t_rd_, t_io_>::handle_write(std::unique_lock<std::mutex>& lock, const loop_resource::control& control) noexcept {
    if (!m_write_pending || m_state.is_errored() || !m_state.can_write()) {
        control.request_events(event_type::out, events_update_type::remove);
        return;
    }

    if (!do_write()) {
        m_state.mark_errored();
        m_write_pending = false;
        control.request_events(event_type::out, events_update_type::remove);
    } else {
        if (m_write_requests.empty()) {
            m_write_pending = false;
            control.request_events(event_type::out, events_update_type::remove);
        }
    }

    report_write_requests_finished(lock);
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void base_io<t_wr_, t_rd_, t_io_>::handle_connect(
    std::unique_lock<std::mutex>& lock,
    const loop_resource::control& control) noexcept
    requires connectable_io_type<t_io_, t_wr_, t_rd_> {
    // finish connect
    const auto error = m_io.finalize_connect();
    control.request_events(event_type::out, events_update_type::remove);
    on_connect_done(lock, control, error);
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void base_io<t_wr_, t_rd_, t_io_>::on_connect_done(
    std::unique_lock<std::mutex>& lock,
    const loop_resource::control& control,
    const error error) noexcept
    requires connectable_io_type<t_io_, t_wr_, t_rd_> {
    if (error == error_success) {
        // connection finished
        looper_trace_info(loop_io_log_module, "connected tcp: handle=%lu", m_handle);

        m_state.set_read_enabled(true);
        m_state.set_write_enabled(true);
        control.invoke_in_loop<>(m_connect_callback, m_handle, error);
    } else {
        m_state.mark_errored();
        looper_trace_error(loop_io_log_module, "tcp connection failed: handle=%lu, code=0x%x", m_handle, error);
        control.invoke_in_loop<>(m_connect_callback, m_handle, error);
    }
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void base_io<t_wr_, t_rd_, t_io_>::report_write_requests_finished(
    std::unique_lock<std::mutex>& lock) noexcept {
    while (!m_completed_write_requests.empty()) {
        auto& request = m_completed_write_requests.front();
        invoke_func<>(lock, "loop_io_log_module", request.write_callback, m_handle, request.error);

        m_completed_write_requests.pop_front();
    }
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
bool base_io<t_wr_, t_rd_, t_io_>::do_write() noexcept {
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

// BASE_IO ---------------------------------------------------------

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
io<t_wr_, t_rd_, t_io_>::io(
    const looper::handle handle, const loop_ptr& loop, io_type&& io_obj) noexcept
    : m_base(handle, loop, std::move(io_obj))
{}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::handle io<t_wr_, t_rd_, t_io_>::handle() const {
    return m_base.m_handle;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
const t_io_& io<t_wr_, t_rd_, t_io_>::io_obj() const {
    return m_base.m_io;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
std::pair<std::unique_lock<std::mutex>, io_control> io<t_wr_, t_rd_, t_io_>::use() noexcept {
    auto [lock, res_control] = m_base.m_resource.lock_loop();
    return {std::move(lock), io_control(m_base.m_state, res_control)};
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::register_to_loop() noexcept {
    auto [lock, control] = m_base.m_resource.lock_loop();
    control.attach_to_loop(
        m_base.m_io.get_descriptor(),
        event_type::none,
        std::bind_front(&io::handle_events, this));
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::error io<t_wr_, t_rd_, t_io_>::mark_connected() noexcept
    requires connectable_io_type<t_io_, t_wr_, t_rd_> {
    auto [lock, control] = m_base.m_resource.lock_loop();
    RETURN_IF_ERROR(m_base.m_state.verify_not_errored());

    if (m_base.m_connected) {
        return error_invalid_state;
    }

    m_base.m_state.set_read_enabled(true);
    m_base.m_state.set_write_enabled(true);
    m_base.m_connected = true;

    return error_success;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::error io<t_wr_, t_rd_, t_io_>::connect(
    connector&& connector,
    connect_callback&& callback) noexcept
    requires connectable_io_type<t_io_, t_wr_, t_rd_> {
    auto [lock, control] = m_base.m_resource.lock_loop();
    RETURN_IF_ERROR(m_base.m_state.verify_not_errored());

    if (m_base.m_connected) {
        return error_invalid_state;
    }

    looper_trace_info(loop_io_log_module, "connecting socket: handle=%lu", m_base.m_handle);

    m_base.m_connect_callback = std::move(callback);
    const auto error = connector(m_base.m_io);
    if (error == error_success) {
        // connection finished
        m_base.on_connect_done(lock, control);
        m_base.m_connected = true;
    } else if (error == error_in_progress) {
        // wait for connection finish
        m_base.m_connection_pending = true;
        control.request_events(event_type::out, events_update_type::append);
        looper_trace_info(loop_io_log_module, "socket connection not finished: handle=%lu", m_base.m_handle);
    } else {
        m_base.on_connect_done(lock, control, error);
    }

    return error_success;
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::error io<t_wr_, t_rd_, t_io_>::start_read(read_callback&& callback) noexcept {
    return m_base.start_read(std::move(callback));
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::error io<t_wr_, t_rd_, t_io_>::stop_read() noexcept {
    return m_base.stop_read();
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
looper::error io<t_wr_, t_rd_, t_io_>::write(write_request&& request) noexcept {
    return m_base.write(std::move(request));
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::close() noexcept {
    m_base.close();
}

template<write_request_type t_wr_, read_data_type t_rd_, io_type<t_wr_, t_rd_> t_io_>
void io<t_wr_, t_rd_, t_io_>::handle_events(
    std::unique_lock<std::mutex>& lock,
    loop_resource::control& control,
    const event_type events) noexcept {
    if (m_base.m_state.is_errored() && (events & (event_type::error | event_type::hung)) != 0) {
        control.request_events(event_type::none, events_update_type::override);
        control.detach_from_loop();
        return;
    }

    if (m_base.m_connection_pending) {
        if ((events & event_type::out) != 0) {
            m_base.handle_connect(lock, control);
        }
    } else {
        if ((events & event_type::in) != 0) {
            // new data
            m_base.handle_read(lock, control);
        }

        if ((events & event_type::out) != 0) {
            m_base.handle_write(lock, control);
        }
    }
}

}
